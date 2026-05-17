# ESP32-S3 1,9 inch ST7789 TFT Display

By chance, I was able to figure out the correct pin assignments for the TFT display on the ESP32-S3 development board and use them to create a Nixie-style clock.

This repository accompanies the article "**How do I find the correct connection pins for an ESP32-S3 development board with an attached TFT display (ST7789)?**" published here: <soon>

### Pin Assignments

````plaintext
TFT Display:
TFT-Backlight: GPIO 38 (HIGH = ON)
TFT-CS       : GPIO  6
TFT-MOSI     : GPIO  9 // = SDA
TFT-SCLK     : GPIO  8   
TFT-MISO     : GPIO 13 // Not connected
TFT-DC       : GPIO  7
TFT-RST      : GPIO  5

Buttons:
Boot Button  : GPIO 0
Key Button   : GPIO 14

QWIIC Connector seen from back side from left to right:
1 GND
2 3.3 V
3 GPIO 43 (labled U0TX)
4 GPIO 44 (labled U0RX)

-= Not tested =-
Battery Voltage measurement: GPIO  4
````

## Development Environment (Arduino)
````plaintext
Arduino IDE Version 2.3.8 (Windows)
arduino-esp32 boards Version 3.3.8 (https://github.com/espressif/arduino-esp32) that is based on Espressif ESP32 Version 5.5.1
````
