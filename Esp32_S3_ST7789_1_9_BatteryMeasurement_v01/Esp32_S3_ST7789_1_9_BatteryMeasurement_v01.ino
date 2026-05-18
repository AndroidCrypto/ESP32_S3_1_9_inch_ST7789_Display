/*
  This sketch will measure the battery voltage of a battery connected by the battery
  connector. The measured raw value on the ADC and calculated voltage are displayed
  on the ST7789 TFT display with 170 x 320 px.

*/

/*
  Version Management
18.05.2026 V01 Initial programming
*/

#include <Arduino.h>
#include "SPI.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

#define DISPLAY_HEIGHT 170
#define DISPLAY_WIDTH 320
#define DISPLAY_X_OFFSET 0
#define DISPLAY_Y_OFFSET 0
#define DISPLAY_BRIGHTNESS 30
#define TFT_BACKLITE 38

#define TFT_POWER 15

#define BOOT_BUTTON 0
#define KEY_BUTTON 14

const uint16_t DISPLAY_BACKGROUND_COLOR = TFT_BLACK;  // background

int16_t displayBrightness = DISPLAY_BRIGHTNESS;

// -----------------------------------------------------------------------
// Battery Management

#define BATTERY_VOLTAGE_ADC_PIN 4

#define BATTERY_VOLTAGE_DIVIDER 588.8
//#define BATTERY_VOLTAGE_DIVIDER 500.1
const uint8_t READ_SAMPLES = 20;
uint16_t batteryVoltageRaw = 1;
float batteryVoltage = 1.23;
const long BATTERY_MEASUREMENT_INTERVAL = 1000;  // each second
long lastBatteryMeasurementMillis = 0;

// ----------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 for TFT ST7789 1,9 in 170 x 320 pixels Battery Voltage Measurement V01");

  // Init buttons
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(KEY_BUTTON, INPUT_PULLUP);

  // When the device is powered by USB the display is set ON by default,
  // but when powering the device by a battery you need to set the pin to HIGH
  // or the display stays dark
  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, HIGH);

  // initial setup of display unit
  tft.init();  // initialise the display unit

  pinMode(TFT_BACKLITE, OUTPUT);
  analogWrite(TFT_BACKLITE, displayBrightness);

  tft.setRotation(1);  // 3 = terminal left side, 2 = USB Top, 1 = terminal right side
  tft.fillScreen(DISPLAY_BACKGROUND_COLOR);

  tft.println("Test");


}

void readBatteryVoltage() {
  pinMode(BATTERY_VOLTAGE_ADC_PIN, INPUT);
  analogReadResolution(12);  //Use 12 bits which would give range of 0 to 4095
  analogSetAttenuation(ADC_11db);  // this is the default value
  float measures = 0;
  for (uint8_t i = 0; i < READ_SAMPLES; i++) {
    measures += analogRead(BATTERY_VOLTAGE_ADC_PIN);
  }
  batteryVoltageRaw = measures / READ_SAMPLES;
  batteryVoltage = batteryVoltageRaw / BATTERY_VOLTAGE_DIVIDER;
}

void displayData() {
  tft.fillScreen(DISPLAY_BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE);
  tft.setCursor(10, 10);
  tft.print("Battery Measurement");
  tft.setCursor(10, 60);
  tft.printf("Raw value: %d", batteryVoltageRaw);
  tft.setCursor(10, 110);
  if (batteryVoltage > 4.3) {
    tft.printf("Voltage: %4.2f Volts (USB)", batteryVoltage);
  } else {
    tft.printf("Voltage: %4.2f Volts", batteryVoltage);
  }
}

void loop() {

  // Handle buttons in loop()
  if (digitalRead(KEY_BUTTON) == LOW) {  // Push button pressed
    // Key debounce handling
    while (digitalRead(KEY_BUTTON) == LOW) {
      delay(50);
    }
    // decrease brightness
    displayBrightness = displayBrightness - 20;
    if (displayBrightness < 20) {
      displayBrightness = 10;
    }
    analogWrite(TFT_BACKLITE, displayBrightness);
  }
  // increase brightness
  if (digitalRead(BOOT_BUTTON) == LOW) {  // Push button pressed
    // Key debounce handling
    while (digitalRead(BOOT_BUTTON) == LOW) {
      delay(50);
    }
    // increase brightness
    displayBrightness = displayBrightness + 20;
    if (displayBrightness > 230) {
      displayBrightness = 230;
    }
    analogWrite(TFT_BACKLITE, displayBrightness);
  }

  if (millis() - lastBatteryMeasurementMillis > BATTERY_MEASUREMENT_INTERVAL) {
    readBatteryVoltage();
    displayData();
    Serial.printf("Battery Voltage Raw: %d = %4.2f Volts\n", batteryVoltageRaw, batteryVoltage);
    lastBatteryMeasurementMillis = millis();
  }
}
