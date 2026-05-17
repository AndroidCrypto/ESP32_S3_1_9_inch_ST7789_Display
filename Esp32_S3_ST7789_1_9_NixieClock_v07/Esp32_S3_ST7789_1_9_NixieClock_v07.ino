/*
  This sketch will display a Nixie-style clock on the ST7789 TFT display with 170 x 320 px.

  The device is synchronized using Wi-Fi with an NTP time server. The display brightness is
  adjustable by pressing the Boot button (increase) or Key button (decrease).

*/

/*
Clock face Source
https://github.com/aly-fly/EleksTubeHAX/
Folder: data
Size 80 x 148 pixels
32 bit

Converter: 
https://mischianti.org/rgb-image-to-byte-array-converter-for-arduino-tft-displays/
Settings (default):
Code format Hex 0x00
Palette mod 16bit RRRRRGGGGGGBBBBB (2byte/pixel)
Resize: use both parameters to match exact size, e.g. 35 x 70
Multi line yes
Endianness Little Endian
static yes
const yes
Data type uint16_t
PROGMEM yes
*/



/*
Version Management

18.05.2026 V07 Tutorial version
16.05.2026 V06 This is using another Nixie Clock faces with 148x80 pixel that should run on the
               1,9 inches 170x320 ST7789 TFT display
...
02.06.2025 V01 Initial programming, does not show the flips
*/

const char* WIFI_SSID = "change to your router";
const char* WIFI_PASSWORD = "change to your password";

#include <Arduino.h>
#include "SPI.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

// Time management
// Configuration of NTP
// choose the best fitting NTP server pool for your country
#define MY_NTP_SERVER "pool.ntp.org"

// choose your time zone from this list
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// Germany/Europe: #define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define MY_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

#include <time.h>
#include <WiFi.h>

// Globals
time_t now;  // this are the seconds since Epoch (1970) - UTC
tm tm;       // the structure tm holds time information in a more convenient way *

// Image files with the digits
// Original source was https://savageelectronics.com/display-array-clock-faces-update/
// https://savageelectronics.com/wp-content/uploads/2021/05/Clock-Faces.zip
#include "zero.h";
#include "one.h";
#include "two.h";
#include "three.h";
#include "four.h";
#include "five.h";
#include "six.h";
#include "seven.h";
#include "eight.h";
#include "nine.h";
#include "point2.h";

TFT_eSprite ts[11] = { TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft), TFT_eSprite(&tft) };

#define DISPLAY_HEIGHT 170
#define DISPLAY_WIDTH 320
#define DISPLAY_X_OFFSET 0
#define DISPLAY_Y_OFFSET 10
#define DISPLAY_BRIGHTNESS 30
#define TFT_BACKLITE 38

#define BOOT_BUTTON 0
#define KEY_BUTTON 14

const uint16_t DISPLAY_BACKGROUND_COLOR = TFT_BLACK;  // background

int16_t displayBrightness = DISPLAY_BRIGHTNESS;


unsigned long ms = millis();
byte start = 1;          // initial start flag
byte display_sHand = 1;  // display seconds hand 0=no, 1=yes
uint8_t hh, mm, ss;

TFT_eSprite display = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object

// ----------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-S3 for TFT ST7789 1,9 in 170 x 320 pixels display V07");

  // Init buttons
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(KEY_BUTTON, INPUT_PULLUP);

  // Obtain current time and set variables for the Second, Minute and Hour
  configTime(0, 0, MY_NTP_SERVER);  // 0, 0 because we will use TZ in the next line
  setenv("TZ", MY_TIMEZONE, 1);     // Set environment variable with your time zone
  tzset();

  // start network
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");

  // initial setup of display unit
  tft.init();  // initialise the display unit

  pinMode(TFT_BACKLITE, OUTPUT);
  analogWrite(TFT_BACKLITE, displayBrightness);

  tft.setRotation(3);  // 3 = terminal left side, 2 = USB Top, 1 = terminal right side
  tft.fillScreen(DISPLAY_BACKGROUND_COLOR);

  for (int i = 0; i < 11; i++) {
    ts[i].setSwapBytes(true);
  }

  createDisplayTimeCharacters80148();
}

void loop() {
  manage_Display();  // manage clock display

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
}

void manage_Display() {
  // Only update the screen once every second and at the start
  if (millis() - ms >= 1000 || start) {
    hh = tm.tm_hour;
    mm = tm.tm_min;
    ss = tm.tm_sec;
    print_current_time();
    ms = millis();
    if (++ss > 59) {
      ss = 0;
      if (++mm > 59) {
        mm = 0;
        if (++hh > 23) hh = 0;
      }
    }
    createDisplayBackground();
    // 4 nixie digits
    flipClockUpdate();
    display.pushSprite(DISPLAY_X_OFFSET, DISPLAY_Y_OFFSET, TFT_TRANSPARENT);
  }
}

void createDisplayTimeCharacters80148() {
  for (int i = 0; i < 10; i++) {
    ts[i].setColorDepth(8);
    ts[i].createSprite(80, 148);
  }
  ts[0].pushImage(0, 0, 80, 148, zero);
  ts[1].pushImage(0, 0, 80, 148, one);
  ts[2].pushImage(0, 0, 80, 148, two);
  ts[3].pushImage(0, 0, 80, 148, three);
  ts[4].pushImage(0, 0, 80, 148, four);
  ts[5].pushImage(0, 0, 80, 148, five);
  ts[6].pushImage(0, 0, 80, 148, six);
  ts[7].pushImage(0, 0, 80, 148, seven);
  ts[8].pushImage(0, 0, 80, 148, eight);
  ts[9].pushImage(0, 0, 80, 148, nine);

  ts[10].setColorDepth(8);
  ts[10].createSprite(5, 5);
  ts[10].pushImage(0, 0, 5, 5, point2);
}

// Create the background sprite i.e. the clock face and push the image of the clock face to that sprite
void createDisplayBackground() {
  display.setColorDepth(8);
  display.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  display.fillSprite(DISPLAY_BACKGROUND_COLOR);
}

void flipClockUpdate() {
  const uint8_t DIGITS_OFFSET_X = 2;
  const uint8_t DIGITS_OFFSET_Y = 3;
  const uint8_t DIGITS_WIDTH = 80;

  // number of the hour
  if (hh < 10) {
    ts[0].pushToSprite(&display, DIGITS_OFFSET_X, DIGITS_OFFSET_Y);
    ts[hh].pushToSprite(&display, DIGITS_OFFSET_X + DIGITS_WIDTH, DIGITS_OFFSET_Y);
  } else if ((hh > 9) && (hh < 20)) {
    ts[1].pushToSprite(&display, DIGITS_OFFSET_X, DIGITS_OFFSET_Y);
    ts[hh - 10].pushToSprite(&display, DIGITS_OFFSET_X + DIGITS_WIDTH, DIGITS_OFFSET_Y);
  } else {
    ts[2].pushToSprite(&display, DIGITS_OFFSET_X, DIGITS_OFFSET_Y);
    ts[hh - 20].pushToSprite(&display, DIGITS_OFFSET_X + DIGITS_WIDTH, DIGITS_OFFSET_Y);
  }

  // colon
  ts[10].pushToSprite(&display, 160, DIGITS_OFFSET_Y + 60);
  ts[10].pushToSprite(&display, 163, DIGITS_OFFSET_Y + 60);
  ts[10].pushToSprite(&display, 160, DIGITS_OFFSET_Y + 80);
  ts[10].pushToSprite(&display, 163, DIGITS_OFFSET_Y + 80);

  // minutes
  if (mm < 10) {
    ts[0].pushToSprite(&display, DIGITS_OFFSET_X + (2 * DIGITS_WIDTH), DIGITS_OFFSET_Y);
    ts[mm].pushToSprite(&display, DIGITS_OFFSET_X + (3 * DIGITS_WIDTH), DIGITS_OFFSET_Y);
  } else if (mm > 9) {
    int mmFirst = mm / 10;
    Serial.printf("mmFirst: %d mmLast: %d\n", mmFirst, mm - (mmFirst * 10));
    ts[mmFirst].pushToSprite(&display, DIGITS_OFFSET_X + (2 * DIGITS_WIDTH), DIGITS_OFFSET_Y);
    ts[mm - (mmFirst * 10)].pushToSprite(&display, DIGITS_OFFSET_X + (3 * DIGITS_WIDTH), DIGITS_OFFSET_Y);
  }

  // seconds
  // this is a simple bar
  display.drawFastHLine(10, DISPLAY_HEIGHT - DISPLAY_Y_OFFSET - 10, ss * 5, TFT_RED);
}

void print_current_time() {
  time(&now);              // read the current time
  localtime_r(&now, &tm);  // update the structure tm with the current time
  Serial.print("year:");
  Serial.print(tm.tm_year + 1900);  // years since 1900
  Serial.print("\tmonth:");
  Serial.print(tm.tm_mon + 1);  // January = 0 (!)
  Serial.print("\tday:");
  Serial.print(tm.tm_mday);  // day of month
  Serial.print("\thour:");
  Serial.print(tm.tm_hour);  // hours since midnight 0-23
  Serial.print("\tmin:");
  Serial.print(tm.tm_min);  // minutes after the hour 0-59
  Serial.print("\tsec:");
  Serial.print(tm.tm_sec);  // seconds after the minute 0-61*
  Serial.print("\twday");
  Serial.print(tm.tm_wday);  // days since Sunday 0-6
  if (tm.tm_isdst == 1)      // Daylight Saving Time flag
    Serial.print("\tDST");
  else
    Serial.print("\tstandard");
  Serial.println();
}