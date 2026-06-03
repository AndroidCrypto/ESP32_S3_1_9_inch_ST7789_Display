/*
  This sketch will display a Nixie-style clock on the ST7789 TFT display with 170 x 320 px.

  The device is synchronized using Wi-Fi with an NTP time server. The Wi-Fi credentials are
  not provided within the sketch but using a Web BLE interface. To use Web BLE you need a
  Chrome or Edge Brwser on Windows or Android and on Apple you should search for Bluefy, 
  BLE Link or WebBLE in the AppStore.

  As a NTP server provides the Universal Coordinated Time ("UTC") you need to calculate your
  local time by an offset, but fortunately the 'time.h' system library can do this for us
  by a POSIX configuration string, this is provided by Web BLE as well.

  Please open the following link in your browser: 
  https://androidcrypto.github.io/WebBle1/wifi_posix_04.html
  
  The transmitted Wi-Fi and POSIX credentials are stored in the Non Volatile Storage (NVS)
  or the ESP32-S3. If no data is found or the Wi-Fi connection is broken, the Web BLE
  interface is started automatically. If you want to force Web BLE you just need to press
  the Boot button.

  The display brightness is fixed because the Boot button is used for other purposes 
  (starting Web BLE interface).

*/

/*
  Important: As the coexistence of TFT_eSPI using large Sprites, Wi-Fi and (Web) BLE crashes
  the system, I'm using a two part sketch for two totally different tasks:
  OTA_0: is the regular clock sketch, that loads a stored Wi-Fi SSID, password and POSIX
         Configuration String, connect to your Wi-Fi router, synchronizes the internal clock
         with an Netword Time Protocol Server (NTP) and display the current time as Nixie-
         style clock faces. After pressing the Boot button, the sketch restarts the ESP32-S3
         but will start the second partition ('OTA_1').
  OTA_1: is the (Web) BLE interface for receiving the Wi-Fi credentials and POSIX Configuration
         String. The TFT display is used without any Sprites and no Wi-Fi is used in this part.
         After receiving and storing the new data the sketch witches back to the regular sketch
         in OTA_0.         
*/

/*
  Important: you don't need a development board with PSRAM but you need to use this
             Partition Scheme: Minimal SPIFFS (1.9 MB App with OTA / 128 KB SPIFFS)
             or any other OTA enabled scheme with >= 1.3 MB Flash for each app.

             Sketch uses 1216384 bytes (61%) of program storage space. 
             Maximum is 1966080 bytes.
  
             Global variables use 50860 bytes (15%) of dynamic memory, 
             leaving 276820 bytes for local variables. Maximum is 327680 bytes.
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
29.05.2026 V13 Code cleaning for tutorial
27.05.2026 V12 OTA_0 sketch: code cleaning, displaying a note after pressing the Boot button and
                             a welcome message at start up
27.05.2026 V11 This is the OTA_0 split sketch for the regular workflow: connect to your router, get
               the NTP time, display the time. There is a second 'OTA_1' that runs all the Web BLE
               parts. This sketch does not need any PSRAM for running.
26.05.2026 V10 After providing credentials the BLE service is stopped, but that crashes the ESP32-S3.
               Probably it is an error due to some memory corruption
26.05.2026 V09 Little Improvements, but still using PSRAM
               For WebBLE, please use https://androidcrypto.github.io/WebBle1/wifi_posix_04.html
25.05.2026 V08 Including BLE/WebBLE for providing Wi-Fi and POSIX string credentials
               ESP32-S3 Board with ST7789 1,9 170x320 px Version is working but need 
               PSRAM activated (OPI PSRAM, not QSPI RAM) and 
               Partition Scheme: Huge APP (3 MB App)
               
18.05.2026 V07 Tutorial version
16.05.2026 V06 This is using another Nixie Clock faces with 148x80 pixel that should run on the
               1,9 inches 170x320 ST7789 TFT display
...
02.06.2025 V01 Initial programming, does not show the flips
*/

#include <Arduino.h>

// --------------------------------------------------------------
// Programm Information
const char *PROGRAM_VERSION = "ESP32-S3 for TFT ST7789 1,9 in 170 x 320 pixels display BLE interface OTA_0 V13";

// --------------------------------------------------------------
// Programm Information

#include "SPI.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
// Setup810_S3_1_9_ST7789_170x320.h is now tft_setup.h in the sketch folder

// --------------------------------------------------------------

// Time management
// Configuration of NTP
// choose the best fitting NTP server pool for your country
#define MY_NTP_SERVER "pool.ntp.org"

// choose your time zone from this list
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// Germany/Europe: #define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"
//#define MY_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"
// this is done in the OTA_1 sketch

#include <time.h>
#include <WiFi.h>

// Globals
time_t now;  // this are the seconds since Epoch (1970) - UTC
tm tm;       // the structure tm holds time information in a more convenient way *

// --------------------------------------------------------------
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
TFT_eSprite display = TFT_eSprite(&tft);  // Declare Sprite object "spr" with pointer to "tft" object

#define DISPLAY_HEIGHT 170
#define DISPLAY_WIDTH 320
#define DISPLAY_X_OFFSET 0
#define DISPLAY_Y_OFFSET 10
#define DISPLAY_BRIGHTNESS 30
#define TFT_BACKLITE 38

// --------------------------------------------------------------
// power and buttons
#define TFT_POWER 15
#define BOOT_BUTTON 0

// --------------------------------------------------------------
// display
const uint16_t DISPLAY_BACKGROUND_COLOR = TFT_BLACK;  // background
int16_t displayBrightness = DISPLAY_BRIGHTNESS;

unsigned long ms = millis();
byte start = 1;  // initial start flag
uint8_t hh, mm, ss;

// ----------------------------------------------------------------
// Preferences are stored in Non Volatile Storage (NVS)
#include <Preferences.h>
Preferences preferences;
String ssid = "";
String password = "";
String tzString = "";

// ----------------------------------------------------------------
// Use second OTA partition
#include "esp_ota_ops.h"

// ----------------------------------------------------------------

void showEnterBleAndCountdownReboot() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(2);

  int marginX = 10;
  int startY = 30;
  int lineHeight = 25;

  tft.setTextColor(TFT_GREEN);
  tft.setCursor(marginX, startY);
  tft.println("After the restart you can");
  tft.setCursor(marginX, startY + lineHeight);
  tft.println("enter the Wi-Fi and POSIX");
  tft.setCursor(marginX, startY + (lineHeight * 2));
  tft.println("String credentials.");

  // 3 seconds countdown-loop
  for (int i = 3; i >= 0; i--) {
    tft.fillRect(0, startY + (lineHeight * 4), 320, 40, TFT_BLACK);
    tft.setCursor(marginX, startY + (lineHeight * 4));
    tft.setTextColor(TFT_WHITE);
    tft.print("Rebooting in ");

    tft.setTextColor(TFT_RED);
    tft.print(i);
    tft.setTextColor(TFT_WHITE);
    tft.print(" seconds...");

    delay(1000);
  }
}

void connectToWifiAndSetTime(String s, String p, String tz) {
  Serial.println("Connect to Wi-Fi...");
  WiFi.begin(s.c_str(), p.c_str());

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter < 20) {
    delay(500);
    Serial.print(".");
    counter++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWI-FI connection with success");
    configTime(0, 0, MY_NTP_SERVER);
    setenv("TZ", tz.c_str(), 1);
    tzset();

    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      Serial.print("Current local time: ");
      Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    }
  } else {
    Serial.println("\nWi-Fi connection failed");
    Serial.println("starting BLE...");
    showEnterBleAndCountdownReboot();
    Serial.println("Switching to OTA_1");
    delay(1000);
    const esp_partition_t* target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (target) {
      esp_ota_set_boot_partition(target);
      esp_restart();
    } else {
      Serial.println("Error: ota_1 partition not found!");
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println(PROGRAM_VERSION);

  // Init buttons
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // When the device is powered by USB the display is set ON by default,
  // but when powering the device by a battery you need to set the pin to HIGH
  // or the display stays dark
  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, HIGH);

  // initial setup of display unit
  tft.init();  // initialise the display unit

  pinMode(TFT_BACKLITE, OUTPUT);
  analogWrite(TFT_BACKLITE, displayBrightness);

  tft.setRotation(3);  // 3 = terminal left side, 2 = USB Top, 1 = terminal right side
  tft.fillScreen(DISPLAY_BACKGROUND_COLOR);
  // just a welcome screen
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN);
  int marginX = 5;
  int startY = 30;
  int lineHeight = 25;
  tft.setCursor(marginX, startY);
  tft.println("ESP32-S3 Nixie style clock");
  tft.setCursor(marginX, startY + lineHeight);
  tft.setTextColor(TFT_WHITE);
  tft.println("Web BLE Version 12");
  tft.setCursor(marginX, startY + (lineHeight * 2));
  tft.println("Initialization...");
  delay(500);

  for (int i = 0; i < 11; i++) {
    ts[i].setSwapBytes(true);
  }

  // load stored data
  preferences.begin("wifi-config", true);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  tzString = preferences.getString("tz", "");
  preferences.end();
  Serial.println("loaded preferences");

  // when data is available try to connect
  if (ssid.length() > 0) {
    connectToWifiAndSetTime(ssid, password, tzString);
  } else {
    Serial.println("No Wi-Fi credentials available, please press the BOOT-Button for starting BLE.");
    showEnterBleAndCountdownReboot();

    Serial.println("Switching to OTA_1");
    delay(1000);
    const esp_partition_t* target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
    if (target) {
      esp_ota_set_boot_partition(target);
      esp_restart();
    } else {
      Serial.println("Error: ota_1 partition not found!");
    }
  }

  createDisplayTimeCharacters80148();
}

void loop() {
  manage_Display();  // manage clock display

  if (digitalRead(BOOT_BUTTON) == LOW) {
    delay(50);  // Debounce
    if (digitalRead(BOOT_BUTTON) == LOW) {
      Serial.println("Boot Button pressed, starting BLE...");
      showEnterBleAndCountdownReboot();
      Serial.println("Switching to OTA_1");
      delay(1000);

      const esp_partition_t* target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, NULL);
      if (target) {
        esp_ota_set_boot_partition(target);
        esp_restart();
      } else {
        Serial.println("Error: ota_1 partition not found!");
      }
    }
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
    ts[i].setAttribute(PSRAM_ENABLE, true);  // a good choice
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
  ts[10].setAttribute(PSRAM_ENABLE, true);  // a good choice
  ts[10].createSprite(5, 5);
  ts[10].pushImage(0, 0, 5, 5, point2);
}

// Create the background sprite i.e. the clock face and push the image of the clock face to that sprite
void createDisplayBackground() {
  display.setColorDepth(8);
  display.setAttribute(PSRAM_ENABLE, true);  // a good choice
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
    //Serial.printf("mmFirst: %d mmLast: %d\n", mmFirst, mm - (mmFirst * 10));
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
  /*
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
  */
}
