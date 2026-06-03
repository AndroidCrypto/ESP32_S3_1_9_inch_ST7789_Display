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
27.05.2026 V12 OTA_1 sketch: code cleaning
27.05.2026 V11 This is the OTA_1 split sketch for the Web BLE workflow: display a page on the display,
               wait for a connection from a Web BLE Client that provides the Wi-Fi router SSID and
               passwort. Additionally, the POSIX Configuration String is provided and stored in NVS. 
               Now a countdown runs to 0 and the ESP32 is starting 'OTA_1' that runs all the clock
               display parts. This sketch does not need any PSRAM for running.
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

/*
  Important: you don't need a development board with PSRAM but you need to use this
             Partition Scheme: Minimal SPIFFS (1.9 MB App with OTA / 128 KB SPIFFS)
             or any other OTA enabled scheme with >= 1.3 MB Flash for each app.

             Sketch uses 647764 bytes (32%) of program storage space. 
             Maximum is 1966080 bytes.
             
             Global variables use 28344 bytes (8%) of dynamic memory, 
             leaving 299336 bytes for local variables. Maximum is 327680 bytes.


Partition Scheme: Minimal (1.3 MB App / 700 KB SPIFFS)
Sketch uses 647748 bytes (49%) of program storage space. Maximum is 1310720 bytes.

Sketch uses 711724 bytes (36%) of program storage space. Maximum is 1966080 bytes.

Global variables use 28404 bytes (8%) of dynamic memory, leaving 299276 bytes for 
local variables. Maximum is 327680 bytes.

*/

#include <Arduino.h>

// --------------------------------------------------------------
// Programm Information
const char *PROGRAM_VERSION = "ESP32-S3 for TFT ST7789 1,9 in 170 x 320 pixels display BLE interface OTA_1 V13";

// --------------------------------------------------------------
// Graphic library
#include "SPI.h"
#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();
// Setup810_S3_1_9_ST7789_170x320.h is now tft_setup.h in the sketch folder

#define DISPLAY_HEIGHT 170
#define DISPLAY_WIDTH 320
#define DISPLAY_X_OFFSET 0
#define DISPLAY_Y_OFFSET 10
#define DISPLAY_BRIGHTNESS 30
#define TFT_BACKLITE 38

#define TFT_POWER 15

#define BOOT_BUTTON 0

const uint16_t DISPLAY_BACKGROUND_COLOR = TFT_BLACK;  // background

int16_t displayBrightness = DISPLAY_BRIGHTNESS;

// ----------------------------------------------------------------
// BLE Configuration and Wi-Fi connection
#include "BLE_Configuration.h"

// ----------------------------------------------------------------
// Use second OTA partition
#include "esp_ota_ops.h"

// ----------------------------------------------------------------

void drawConfigPage() {
    tft.fillScreen(TFT_BLACK);
    
    int iconSize = 40;
    tft.fillRoundRect(10, 10, iconSize, iconSize, 8, TFT_BLUE);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(3);
    tft.drawCentreString("!", 10 + (iconSize/2), 10 + 8, 1);

    tft.setTextSize(2);
    tft.setTextColor(TFT_GOLD);
    tft.drawString("Web BLE Config Mode", 60, 10);
    
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE);
    tft.drawString("Use Chrome or Edge", 60, 35);

    int startY = 60;
    int marginX = 10;
    int lineHeight = 22;

    tft.setCursor(marginX, startY);
    tft.println("Connect with:");

    tft.setTextColor(TFT_CYAN);
    tft.println("https://androidcrypto.github.io/");
    tft.println("WebBle1/wifi_posix_04.html");
    
    tft.setTextColor(TFT_WHITE);
    tft.setCursor(marginX, tft.getCursorY() + 5);
    tft.println("Enter credentials&connect");
    
    tft.setTextColor(TFT_GREEN);
    tft.setTextSize(2);
    tft.drawCentreString("ESP32-Config-Portal", 160, tft.getCursorY() + 5, 1);
}

void showSuccessAndCountdownReboot() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    
    int marginX = 15;
    int startY = 30;
    int lineHeight = 25;

    tft.setTextColor(TFT_GREEN);
    tft.setCursor(marginX, startY);
    tft.println("Thank you for providing");
    tft.setCursor(marginX, startY + lineHeight);
    tft.println("the Wi-Fi and POSIX");
    tft.setCursor(marginX, startY + (lineHeight * 2));
    tft.println("String credentials.");

    // Countdown-loop
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

  BLEDevice::init("ESP32-Config-Portal");

  // initial setup of display unit
  tft.init();  // initialise the display unit

  pinMode(TFT_BACKLITE, OUTPUT);
  analogWrite(TFT_BACKLITE, displayBrightness);

  tft.setRotation(3);  // 3 = terminal left side, 2 = USB Top, 1 = terminal right side
  tft.fillScreen(DISPLAY_BACKGROUND_COLOR);

  drawConfigPage();

  startBLE();
}

void loop() {

  // after receiving all data by BLE
  if (dataReceived) {
    dataReceived = false;

    // show countdown
    showSuccessAndCountdownReboot();
    
    Serial.println("Switching to OTA_0");
    delay(1000);
    const esp_partition_t* target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, NULL);
    if (target) {
      esp_ota_set_boot_partition(target);
      esp_restart();
    } else {
      Serial.println("Error: ota_0 partition not found!");
    }
  }
}
