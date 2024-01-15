#include "wifiUtils.h"
#include <time.h>
#include <WiFi.h>
#include <Preferences.h>



Preferences Settings;
RemoteDebug Debug;


void OTA_onStart();
void OTA_onEnd();
void OTA_onProgress(uint32_t progress, uint32_t total);
void OTA_onError(ota_error_t error);
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info);

void DebugStart()
{
	debugI("Iniciando servicio RemoteDebug....");

	String name = Settings.getString("debug-name", "Medidor de gas");
	String pass = Settings.getString("debug-pass", "");
	Debug.begin(name);
	Debug.setResetCmdEnabled(true);

	if (pass.length() > 0) Debug.setPassword(pass);
	Debug.showProfiler(true); // Profiler (Good to measure times, to optimize codes)
	Debug.showColors(true); // Colors

	debugI("Debugger listo!");
}



/*
		Iniciar el Wifi según la configuración que se lee
*/
void WifiStart()
{
	Debug.setSerialEnabled(true);

	WiFi.disconnect(true, true);
	if (Settings.begin("wifi-stuff", false) == false) {
		debugE("Fallo al cargar 'wifi-stuff' desde Preferences");
	}

	debugI("Iniciando Wifi...\n");
	Wifi_Mode mode = static_cast<Wifi_Mode>(Settings.getInt("mode", static_cast<int32_t>(Wifi_Mode::Station)));

	WiFi.mode(static_cast<wifi_mode_t>(mode));

	if (mode == Wifi_Mode::Station)
		debugI("Wifi modo estacion\n");
	else if (mode == Wifi_Mode::SoftAP)
		debugI("Wifi modo soft AP\n");
	else if (mode == Wifi_Mode::Station_AP)
		debugI("Wifi modo soft AP y estacion\n");
	else {
		debugI("Wifi modo off\n");
		return;
	}

	WiFi.onEvent(WiFiStationConnected);
	WiFi.setSleep(false);

	IPAddress ip, mask, gateway, dns1, dns2;
	bool StaticIP = Settings.getBool("staticIp", false);
	ip.fromString(Settings.getString("sta-ip", "192.168.1.80"));
	mask.fromString(Settings.getString("sta-mask", "255.255.255.0"));
	gateway.fromString(Settings.getString("sta-gw", "192.168.1.1"));
	dns1.fromString(Settings.getString("sta-dns1", "8.8.8.8"));
	dns2.fromString(Settings.getString("sta-dns2", "4.4.4.4"));

	if (StaticIP) {
		debugI("Seteando dirección IP estatica... \n");
		debugI("Dirección IP:    %s\n", ip.toString());
		debugI("Mascara de red:  %s\n", mask.toString());
		debugI("Gateway:         %s\n", gateway.toString());
		debugI("DNS preferiado:  %s\n", dns1.toString());
		debugI("DNS alternativo: %s\n", dns2.toString());

		if (WiFi.config(ip, gateway, mask, dns1, dns2))
			debugI("IP iniciada correctamente!\n");
		else
			debugW("Error al setear IP!\n");
	}

	String ssid = Settings.getString("sta-ssid", "Wifi-Luis");
	String pass = Settings.getString("sta-pass", "");

	debugI("Iniciando wifi...\nConectando a: %s...", ssid);
	WiFi.setAutoReconnect(true);

	if (WiFi.begin(ssid.c_str(), pass.c_str()))
		debugI("Ok!\n");
	else
		debugE("Error!\n");

	if (mode == Wifi_Mode::SoftAP || mode == Wifi_Mode::Station_AP) {
		// Eventos de modo softAP
		WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
			Serial.printf("Info: WiFi AP iniciado\n"); },
			ARDUINO_EVENT_WIFI_AP_START);

		WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
			Serial.printf("Info: WiFi AP terminado\n"); }, ARDUINO_EVENT_WIFI_AP_STOP);

		WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
			Serial.printf("Info: WiFi AP direccion IP aignada: ",
			Serial.println(WiFi.softAPIP())); }, ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED);

		IPAddress ip, mask, gateway;
		ip.fromString(Settings.getString("ap-ip", "192.168.1.1"));
		mask.fromString(Settings.getString("ap-mask", "255.255.255.0"));
		gateway.fromString(Settings.getString("ap-gw", "192.168.1.1"));
		String ssid = Settings.getString("ap-ssid", "ESP32-AP");
		String pass = Settings.getString("ap-pass", "");
		bool hide = Settings.getBool("ap-hide", false);
		bool customIp = Settings.getBool("ap-custom-ip", false);

		// Configuracion soft ap
		debugI("Configurando el punto de acceso (AP)...");
		if (WiFi.softAP(ssid.c_str(), pass.c_str(), 1, hide))
			debugI("Ok!");
		else
			debugE("Error!");

		if (customIp) {
			debugI("Usar direccion IP definida por usuario para AP");
			debugI("Dirección IP:    %s", ip.toString());
			debugI("Mascara de red:  %s", mask.toString());
			debugI("Gateway:         %s", gateway.toString());

			if (WiFi.softAPConfig(ip, gateway, mask))
				debugI("IP iniciada correctamente!");
			else
				debugI("Error al setear IP!");
		}
		WiFi.softAPsetHostname("Medidor de Gas AP");
		WiFi.enableAP(true);
	}
	debugI("Configuracion Wifi terminada correctamente\n");

	// OTAStart();

	// TimeSetServer();
}

/*
		Evento de conexión de Wifi
*/
void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{

	if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
		debugI("Conectado a AP (SSID): %s", getWiFiSSID());

		// ui.Msg.ShowMessage("Conectado!", getWiFiSSID());

		// TimeSetServer();

		// mqtt.begin(Settings.Mqtt.Broker, Settings.Mqtt.User,
		// 	Settings.Mqtt.Password);


		// initWebServer();
	}
	else if (event == (WiFiEvent_t)ARDUINO_EVENT_WIFI_STA_GOT_IP) {
		debugI("Se obtuvo direccion IP: %s", WiFi.localIP().toString().c_str());
	}
	else {
		debugI("Evento wifi! code: %d, descripcion: %s", event, getWifiEventName(event));
	}
}




/*
		Métodos de actualización OTA (over the air)
*/
void OTAStart()
{
	/*
			Iniciar el servicio de actualización por OTA
	*/
	bool enabled = Settings.getBool("ota-enabled", true);
	String name = Settings.getString("ota-name", "");
	String pass = Settings.getString("ota-pass", "");

	if (enabled) {
		debugI("Iniciando servicio de actualización de software OTA\n");

		if (name.length() > 0)
			ArduinoOTA.setHostname(name.c_str());
		else
			ArduinoOTA.setHostname("MedidorGas");

		if (pass.length() > 0)
			ArduinoOTA.setPassword(pass.c_str());

		ArduinoOTA.onStart(OTA_onStart);
		ArduinoOTA.onEnd(OTA_onEnd);
		ArduinoOTA.onProgress(OTA_onProgress);
		ArduinoOTA.onError(OTA_onError);

		ArduinoOTA.begin();
	}
	else {
		debugI("Iniciando servicio de actualización de software OTA... no esta activo");
		ArduinoOTA.end();
	}
}

void OTAEnd()
{
	debugI("Deteniendo el servicio de actualización de software OTA...");
	ArduinoOTA.end();
}

void OTA_onStart()
{
	if (ArduinoOTA.getCommand() == U_FLASH)
		debugW("Iniciando actualización de firware via OTA...");
	else // U_SPIFFS
		debugW("Iniciando actualización de sistema de archivos via OTA...");
}

void OTA_onEnd()
{
	debugI("Actualización terminada");
}

void OTA_onProgress(uint32_t progress, uint32_t total)
{
	static int32_t lastProg = -1, prog;
	prog = (float)progress / (float)total * 100;
	if (prog != lastProg) {
		lastProg = prog;
		debugI("Progreso OTA: %d%%", prog);
	}
}

char OTA_ErrorMsg[32] = { 0 };
void OTA_onError(ota_error_t error)
{
	if (error == OTA_AUTH_ERROR)
		strcpy(OTA_ErrorMsg, "Error de autenticacion");
	else if (error == OTA_BEGIN_ERROR)
		strcpy(OTA_ErrorMsg, "Error al iniciar");
	else if (error == OTA_CONNECT_ERROR)
		strcpy(OTA_ErrorMsg, "Error de conexion");
	else if (error == OTA_RECEIVE_ERROR)
		strcpy(OTA_ErrorMsg, "Error de recepcion");
	else if (error == OTA_END_ERROR)
		strcpy(OTA_ErrorMsg, "Error al terminar");

	debugE("OTA Error[%u]: %s", error, OTA_ErrorMsg);
}


time_t getTime(time_t* _timer)
{
	time_t now = time(_timer);
	// return now + (Settings.Time.TimeZone.tz_minuteswest * 60);
	return now;
	// return difftime(now, Settings.Time.TimeZone.tz_minuteswest * 60);
}











void printLocalTime()
{
	unsigned long Time;

	struct tm timeinfo;
	Time = micros();
	if (!getLocalTime(&timeinfo)) {
		debugE("Failed to obtain time");
		return;
	}
	Time = micros() - Time;
	Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
	Serial.printf("Time: %u us\n", Time);
}

String getElapsedTime(time_t time, bool format)
{
	int dias, horas, minutos, segundos;
	dias = time / (24 * 60 * 60);
	if (dias)
		time %= dias * (24 * 60 * 60);

	horas = time / (60 * 60);
	if (horas)
		time %= horas * (60 * 60);

	minutos = time / 60;
	if (minutos)
		time %= minutos * 60;
	segundos = time;

	String res = "";
	if (dias == 1)
		res += String(dias) + " dia ";
	else if (dias > 1)
		res += String(dias) + " dias ";
	if (format) {
		if (horas > 1)
			res += String(horas) + " horas ";
		else if (horas > 0)
			res += String(horas) + " hora ";
		if (minutos > 1)
			res += String(minutos) + " minutos ";
		else if (minutos > 0)
			res += String(minutos) + " minuto ";
		if (segundos > 1 || segundos == 0)
			res += String(segundos) + " segundos ";
		else if (segundos > 0)
			res += String(segundos) + " segundo ";
	}
	else {
		char str[16];
		sprintf(str, "%02d:%02d:%02d", horas, minutos, segundos);
		res += String(str);
	}
	res.trim();
	return res;
}

int32_t getWiFiRSSI()
{
	wifi_ap_record_t wifidata;
	// uint64_t time = micros();
	if (esp_wifi_sta_get_ap_info(&wifidata) == 0) {
		// time = micros() - time;
		// printf("rssi:%d in %u us\n", wifidata.rssi, time);
		return wifidata.rssi;
	}
	return 0;
}

char* getWiFiSSID()
{
	static wifi_ap_record_t wifidata;
	if (esp_wifi_sta_get_ap_info(&wifidata) == 0) {
		return (char*)wifidata.ssid;
	}
	return nullptr;
}

char getWiFiRSSICode(int32_t RSSI)
{
	char c = ' ';
	if (RSSI < -80)
		c = WiFiChar_RSSI_1;
	else if (RSSI < -70)
		c = WiFiChar_RSSI_2;
	else if (RSSI < -60)
		c = WiFiChar_RSSI_3;
	else // if (RSSI >= -60)
		c = WiFiChar_RSSI_4;
	return c;
}

char getWiFiRSSICode() { return getWiFiRSSICode(getWiFiRSSI()); }

const char* arduino_event_names[] = {
		"WIFI_READY",
		"SCAN_DONE",
		"STA_START", "STA_STOP", "STA_CONNECTED", "STA_DISCONNECTED", "STA_AUTHMODE_CHANGE", "STA_GOT_IP", "STA_GOT_IP6", "STA_LOST_IP",
		"AP_START", "AP_STOP", "AP_STACONNECTED", "AP_STADISCONNECTED", "AP_STAIPASSIGNED", "AP_PROBEREQRECVED", "AP_GOT_IP6",
		"FTM_REPORT",
		"ETH_START", "ETH_STOP", "ETH_CONNECTED", "ETH_DISCONNECTED", "ETH_GOT_IP", "ETH_GOT_IP6",
		"WPS_ER_SUCCESS", "WPS_ER_FAILED", "WPS_ER_TIMEOUT", "WPS_ER_PIN", "WPS_ER_PBC_OVERLAP",
		"SC_SCAN_DONE", "SC_FOUND_CHANNEL", "SC_GOT_SSID_PSWD", "SC_SEND_ACK_DONE",
		"PROV_INIT", "PROV_DEINIT", "PROV_START", "PROV_END", "PROV_CRED_RECV", "PROV_CRED_FAIL", "PROV_CRED_SUCCESS"
};

const char* getWifiEventName(WiFiEvent_t event)
{
	if (event >= ARDUINO_EVENT_MAX) return nullptr;
	return arduino_event_names[event];
}


const char* wifiEventDescription[] = {
	"ESP32 WiFi ready", 										/*!< SYSTEM_EVENT_WIFI_READY  */
	"ESP32 finish scanning AP",                					/*!< SYSTEM_EVENT_SCAN_DONE  */
	"ESP32 station start",                						/*!< SYSTEM_EVENT_STA_START*/
	"ESP32 station stop",                 						/*!< SYSTEM_EVENT_STA_STOP */
	"ESP32 station connected to AP",            				/*!< SYSTEM_EVENT_STA_CONNECTED */
	"ESP32 station disconnected from AP",         				/*!< SYSTEM_EVENT_STA_DISCONNECTED */
	"the auth mode of AP connected by ESP32 station changed",   /*!< SYSTEM_EVENT_STA_AUTHMODE_CHANGE */
	"ESP32 station got IP from connected AP",               	/*!< SYSTEM_EVENT_STA_GOT_IP */
	"ESP32 station lost IP and the IP is reset to 0",           /*!< SYSTEM_EVENT_STA_LOST_IP */
	"ESP32 station connected BSS rssi goes below threshold",    /*!< SYSTEM_EVENT_STA_BSS_RSSI_LOW */
	"ESP32 station wps succeeds in enrollee mode",       		/*!< SYSTEM_EVENT_STA_WPS_ER_SUCCESS */
	"ESP32 station wps fails in enrollee mode",        			/*!< SYSTEM_EVENT_STA_WPS_ER_FAILED */
	"ESP32 station wps timeout in enrollee mode",       		/*!< SYSTEM_EVENT_STA_WPS_ER_TIMEOUT */
	"ESP32 station wps pin code in enrollee mode",           	/*!< SYSTEM_EVENT_STA_WPS_ER_PIN */
	"ESP32 station wps overlap in enrollee mode",   			/*!< SYSTEM_EVENT_STA_WPS_ER_PBC_OVERLAP */
	"ESP32 soft-AP start",                 						/*!< SYSTEM_EVENT_AP_START */
	"ESP32 soft-AP stop",                  						/*!< SYSTEM_EVENT_AP_STOP */
	"a station connected to ESP32 soft-AP",          			/*!< SYSTEM_EVENT_AP_STACONNECTED */
	"a station disconnected from ESP32 soft-AP",       			/*!< SYSTEM_EVENT_AP_STADISCONNECTED */
	"ESP32 soft-AP assign an IP to a connected station",        /*!< SYSTEM_EVENT_AP_STAIPASSIGNED */
	"Receive probe request packet in soft-AP interface",        /*!< SYSTEM_EVENT_AP_PROBEREQRECVED */
	"Receive status of Action frame transmitted",         		/*!< SYSTEM_EVENT_ACTION_TX_STATUS */
	"Indicates the completion of Remain-on-Channel operation status" ,/*!< SYSTEM_EVENT_ROC_DONE*/
	"ESP32 station beacon timeout",       						/*!< SYSTEM_EVENT_STA_BEACON_TIMEOUT */
	"Receive report of FTM procedure",               			/*!< SYSTEM_EVENT_FTM_REPORT */
	"ESP32 station or ap or ethernet interface v6IP addr is preferred", /*!< SYSTEM_EVENT_GOT_IP6 */
	"ESP32 ethernet start",                						/*!< SYSTEM_EVENT_ETH_START */
	"ESP32 ethernet stop",                 						/*!< SYSTEM_EVENT_ETH_STOP */
	"ESP32 ethernet phy link up",            					/*!< SYSTEM_EVENT_ETH_CONNECTED */
	"ESP32 ethernet phy link down",         					/*!< SYSTEM_EVENT_ETH_DISCONNECTED */
	"ESP32 ethernet got IP from connected AP",               	/*!< SYSTEM_EVENT_ETH_GOT_IP */
	"ESP32 ethernet lost IP and the IP is reset to 0",          /*!< SYSTEM_EVENT_ETH_LOST_IP */
};
