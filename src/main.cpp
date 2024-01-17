#include <Arduino.h>
#include "MedidorGas.h"
#include "wifiUtils.h"



LiquidCrystal lcd(pinLcdRS, pinLcdEN, pinLcdD4, pinLcdD5, pinLcdD6, pinLcdD7);
HX711 cell1, cell2;
Freenove_ESP32_WS2812 statusLed = Freenove_ESP32_WS2812(2, pinPixelLed, 1);
BMx280I2C bmx280(0x76);

TactSwitch BtnEnter, BtnUp, BtnDown, BtnEsc, BtnNext; // Interfase de 4 botones
keyboard Keyboard;                           // Interfase de teclado
lcd_ui ui(16, 2, pinBuzzer);                            // Interfaz de usuario con una pantalla de 2 x 16


void setup() {

  Serial.begin(115200);
  Serial.println("Iniciando medidor de gas....");


  pinMode(pinRelay, OUTPUT);
  pinMode(pinBuzzer, OUTPUT);
  pinMode(pinLedOk, OUTPUT);
  pinMode(pinInput, INPUT);

  // Iniciar LCD
  lcd.begin(16, 2);
  analogWrite(pinLcdBL, 128);

  // Inicializar las teclas
  BtnEsc.begin(Keys::Esc, Keys::Left, pinBtnEsc);
  BtnDown.begin(Keys::Down, pinBtnDown);
  BtnUp.begin(Keys::Up, pinBtnUp);
  BtnEnter.begin(Keys::Enter, pinBtnOk);
  BtnNext.begin(Keys::Next, pinBtnNext);

  // Agregar teclas al teclado
  Keyboard.AddButton(&BtnEnter);
  Keyboard.AddButton(&BtnUp);
  Keyboard.AddButton(&BtnDown);
  Keyboard.AddButton(&BtnEsc);
  Keyboard.AddButton(&BtnNext);

  ui.begin(&lcd, &Keyboard); // Inicia el stack UI


  //Inicializar HX711
  cell1.begin(pinHX711Dat, pinHX711Clk);
  cell2.begin(pinHX711Dat2, pinHX711Clk2);
  //Inicializar pixel leds
  statusLed.begin();

  //Inicializar bus I2C
  Wire.begin(pinSDA, pinSCL);
  if (!bmx280.begin()) {
    Serial.println("begin() failed. check your BMx280 Interface and I2C Address.");
  }
  bmx280.resetToDefaults();
  bmx280.writeOversamplingPressure(BMx280MI::OSRS_P_x16);
  bmx280.writeOversamplingTemperature(BMx280MI::OSRS_T_x16);

  if (!bmx280.measure())
    Serial.println("could not start measurement, is a measurement already running?");

  Serial.println("Todo en linea!");

  UtilsStart();

  WifiStart();

  TimeStart();

  // TimeSetServer();

  // DebugStart();

  // OTAStart();

}

void loop() {
 // UtilsLoop();

  ui.Run();

}
