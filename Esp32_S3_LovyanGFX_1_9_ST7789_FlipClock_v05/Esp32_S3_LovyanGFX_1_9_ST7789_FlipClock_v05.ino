/*
  This is a flip clock with a 'real' anmitation for the flip. It uses a Wi-Fi connection
  to retrieve the current time by NTP (Network Time Protocol) and a parameter string to
  calculate the current, local time.

  This work is completely based on the CYDFlipClock by Thom Dyson, so all credits go to him:
  Source: https://github.com/ThomDyson/CYDFlipClock
  https://www.reddit.com/r/esp32/comments/1gu20d7/flip_clock_on_a_cheap_yellow_display/

  The original work is using the TFT_eSPI graphic library, but as this library is no longer
  updated you may get a lot of problems when using Arduino ESP32-Board versions > 3.x, so
  I converted the code for using the LovyanGFX library that is still in development.

  As this development board has two buttons, I'm using them to increase or decrease the
  displays brightness.

  The project was developed in Arduino IDE 2.3.8 and esp32 Boards version 3.3.8.

*/

/*
  Version Management
20.05.2026 V05 As the device has two buttons, a display brightness control is in use
               This device needs an additional step when the device is powered by battery
               to prevent a black screen: this is included now
19.05.2026 V04 new display arrangement, all data is displayed on the 170 x 320 px display
18.05.2026 V03 Initial programming by using an existing sketch that was for a larger display  
*/

#include <Arduino.h>
#include <time.h>
#include <WiFi.h>

#include <LovyanGFX.hpp> // https://github.com/lovyan03/LovyanGFX
#include "LovyanGFX_ST7789_170_320_Settings.h"

#include <roboto_64.h>

// *****************
// create secrets.h  in the include folder and put your wifi data there
//       OR
// comment out this include and uncomment the lines for ssid and WiFipassword
// *****************
//#include <secrets.h>

const char *ssid         = "My Home Wifi Name";     // Change this to your WiFi SSID
const char *WiFipassword = "MySuperSecretPassword"; // Change this to your WiFi password


// Timezone and NTP servers
const char *ntpServer = "pool.ntp.org";

// choose your time zone from this list
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
// Germany/Europe: #define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"
#define MY_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

/*
To use a 24 hour display, set this line const bool MilTime = true; to true. 
To use a 12 hour display, set MilTime to false, const bool MilTime = false;
*/
const bool MilTime = true;  // 24 hour clock, AKA military time

#define TFT_POWER 15

#define BOOT_BUTTON 0
#define KEY_BUTTON 14
#define DISPLAY_BRIGHTNESS 30 // default value
int16_t displayBrightness = DISPLAY_BRIGHTNESS;

#define CLOCK_BACKGROUND_COLOR 0x75BE
#define BORDER_HIGHLIGHT_COLOR TFT_WHITE

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 170
#define SCREEN_X_PADDING 2
#define TOP_BORDER_WIDTH 3
#define SIDE_BORDER_WIDTH 3
#define LARGE_DIGIT_INTER_SPACE 2
#define SMALL_DIGIT_INTER_SPACE 1
#define LARGE_DIGIT_COUNT 4
#define SMALL_DIGIT_COUNT 8
// Touchscreen coordinates: (x, y) and pressure (z)
int16_t touchX, touchY, touchZ;
int16_t text_height = 0;

// Set X and Y coordinates for center of display
const int16_t centerX = SCREEN_WIDTH / 2;
const int16_t centerY = SCREEN_HEIGHT / 2;

LGFX tft;

LGFX_Sprite sprite_LargeTopBorder(&tft);
LGFX_Sprite sprite_LargeBottomBorder(&tft);
LGFX_Sprite sprite_LargeSideBorder(&tft);
LGFX_Sprite sprite_LargeCenterLine(&tft);

LGFX_Sprite sprite_SmallTopBorder(&tft);
LGFX_Sprite sprite_SmallBottomBorder(&tft);
LGFX_Sprite sprite_SmallSideBorder(&tft);
LGFX_Sprite sprite_SmallCenterLine(&tft);

LGFX_Sprite sprite_blipBackground(&tft);
LGFX_Sprite sprite_blipCenter(&tft);

LGFX_Sprite sprite_newValue(&tft);
LGFX_Sprite sprite_currentValue(&tft);
LGFX_Sprite sprite_Display(&tft);

int16_t h1X, h2X, m1X, m2X, DOW1X, DOW2X, DOW3X, Date1X, Date2X, Month1X, Month2X, Month3X;  // x position for all the sprites

// Define dimensions for each sprite (adjust size as needed)
int16_t largeSpriteWidth = ((SCREEN_WIDTH - (SCREEN_X_PADDING * 2) - (SIDE_BORDER_WIDTH * LARGE_DIGIT_COUNT * 2) + ((LARGE_DIGIT_COUNT - 1) * LARGE_DIGIT_INTER_SPACE))) / LARGE_DIGIT_COUNT;  // Width for each digit sprite
int16_t largeSpriteSpace = largeSpriteWidth + SIDE_BORDER_WIDTH * 2;                                                                                                                           // the full width of a sprite and borders
const int16_t largeSpriteHeight = 110;
const int16_t largeSpriteTop = 52;

int16_t smallSpriteWidth = (SCREEN_WIDTH - (SCREEN_X_PADDING * 2) - (SIDE_BORDER_WIDTH * SMALL_DIGIT_COUNT * 2) + +((SMALL_DIGIT_COUNT - 1) * SMALL_DIGIT_INTER_SPACE)) / (SMALL_DIGIT_COUNT + 1);  // Width for each sprite, based on number of sprites plus padding
int16_t smallSpriteSpace = smallSpriteWidth + TOP_BORDER_WIDTH * 2;
const int16_t smallSpriteHeight = 34;
const int16_t smallSpriteTop = 7;

const int16_t blinkSpriteTop = smallSpriteTop + 2;
const int16_t blinkSpriteX = 111;

const char *monthsOfYear[] = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN",
  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};

const char *daysOfWeek[] = {
  "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
};

int8_t currentMinute1 = 9;
int8_t currentMinute2 = 9;
int8_t currentHour1 = 9;
int8_t currentHour2 = 9;
int8_t currentDate1 = 9;
int8_t currentDate2 = 9;
char currentDOW[4] = "XXX";
char currentMonth[4] = "XXX";
bool firstpass;

static unsigned long lastMillis = 0;
int blipCycle = 0;

void setup_character_sprite(LGFX_Sprite &thisSprite, int16_t width, int16_t height, uint8_t colorDepth, uint16_t textColor, uint16_t bgColor, const lgfx::GFXfont *font) {
  thisSprite.setColorDepth(colorDepth);
  thisSprite.createSprite(width, height);
  thisSprite.setTextDatum(MC_DATUM);
  thisSprite.setTextColor(textColor, bgColor);
  thisSprite.setFreeFont(font);
}

void please_wait() {
  setup_character_sprite(sprite_Display, largeSpriteWidth, largeSpriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &fonts::FreeSans24pt7b);
  int16_t midWidth = (largeSpriteWidth / 2);
  int16_t midHeight = (largeSpriteHeight / 2);
  sprite_Display.fillRect(0, 0, largeSpriteWidth, largeSpriteHeight, CLOCK_BACKGROUND_COLOR);
  sprite_Display.drawString("W", midWidth, midHeight);                    // Draw the text in the sprite
  sprite_Display.fillRect(0, midHeight, largeSpriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(h1X, largeSpriteTop);

  sprite_Display.fillRect(0, 0, largeSpriteWidth, largeSpriteHeight, CLOCK_BACKGROUND_COLOR);
  sprite_Display.drawString("A", midWidth, midHeight);                    // Draw the text in the sprite
  sprite_Display.fillRect(0, midHeight, largeSpriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(h2X, largeSpriteTop);

  sprite_Display.fillRect(0, 0, largeSpriteWidth, largeSpriteHeight, CLOCK_BACKGROUND_COLOR);
  sprite_Display.drawString("I", midWidth, midHeight);                    // Draw the text in the sprite
  sprite_Display.fillRect(0, midHeight, largeSpriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(m1X, largeSpriteTop);

  sprite_Display.fillRect(0, 0, largeSpriteWidth, largeSpriteHeight, CLOCK_BACKGROUND_COLOR);
  sprite_Display.drawString("T", midWidth, midHeight);                    // Draw the text in the sprite
  sprite_Display.fillRect(0, midHeight, largeSpriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(m2X, largeSpriteTop);
  sprite_Display.deleteSprite();
}

// function to darken a color to look like it is in shadow
uint16_t shadow_color(uint16_t color, float shadowFactor) {
  // Extract RGB components from the 16-bit RGB565 color
  uint8_t r = (color >> 11) & 0x1F;  // 5 bits for red
  uint8_t g = (color >> 5) & 0x3F;   // 6 bits for green
  uint8_t b = color & 0x1F;          // 5 bits for blue

  // Apply shadow effect by reducing the brightness of each color channel
  r = (uint8_t)(r * shadowFactor);
  g = (uint8_t)(g * shadowFactor);
  b = (uint8_t)(b * shadowFactor);

  // Clamp values to the maximum allowed for RGB565 (5 bits for red and blue, 6 bits for green)
  r = r > 31 ? 31 : r;
  g = g > 63 ? 63 : g;
  b = b > 31 ? 31 : b;

  // Rebuild the RGB565 color from the adjusted channels
  uint16_t shadow_color = (r << 11) | (g << 5) | b;

  return shadow_color;
}

// function to animate the transition between characters
// animation happens in 4 steps:
// 1. draw the orig value,
// 2. draw the top half of the orig value in a constrained space so it looks like foreshortening. reveal the top of the new value behind that.
// 3. draw the top half of the new value and the bottom  half of the old value.
// 4. draw the old value, then draw the top of the new over that and the bottom half of the new in a constrained space over the bottom
// 5. draw the new value.
void change_sprite_display(const char *currentValue, const char *newValue, int16_t posX, int16_t posY, bool largeSprite) {
  int16_t spriteHeight;
  int16_t spriteWidth;
  int16_t animationDelay = 100;
  int16_t newY;
  int16_t lastYrow = 0;
  float scalefactor = (2.0 / 3.0);  // this sets the space for the partial drawing segments. gotta force floating  point math

  int16_t newRangeSize;
  int16_t newMaxY;
  int16_t newMinY;

  if (largeSprite) {
    spriteHeight = largeSpriteHeight;
    spriteWidth = largeSpriteWidth;
    setup_character_sprite(sprite_Display, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &roboto_regular64pt7b);
    setup_character_sprite(sprite_newValue, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &roboto_regular64pt7b);
    setup_character_sprite(sprite_currentValue, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &roboto_regular64pt7b);
  } else {
    spriteHeight = smallSpriteHeight;
    spriteWidth = smallSpriteWidth;
    setup_character_sprite(sprite_Display, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &fonts::FreeSans18pt7b);
    setup_character_sprite(sprite_newValue, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &fonts::FreeSans18pt7b);
    setup_character_sprite(sprite_currentValue, spriteWidth, spriteHeight, 16, TFT_WHITE, CLOCK_BACKGROUND_COLOR, &fonts::FreeSans18pt7b);
  }

  sprite_Display.fillSprite(CLOCK_BACKGROUND_COLOR);
  sprite_newValue.fillSprite(CLOCK_BACKGROUND_COLOR);
  sprite_currentValue.fillSprite(CLOCK_BACKGROUND_COLOR);

  int16_t midWidth = (spriteWidth / 2);
  int16_t midHeight = (spriteHeight / 2);
  int16_t midHeightLgfx;
  if (largeSprite) {
    midHeightLgfx = (spriteHeight / 2) + 14;  // the font is displyed a the top of the flip instead of middle
  } else {
    midHeightLgfx = (spriteHeight / 2) + 2;
  }

  Serial.printf("Animating from %s to %s\n", currentValue, newValue);
  sprite_currentValue.drawString(currentValue, midWidth, midHeightLgfx);      // Draw the text in the sprite
  sprite_currentValue.fillRect(0, midHeight, spriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_newValue.drawString(newValue, midWidth, midHeightLgfx);              // Draw the text in the sprite
  sprite_newValue.fillRect(0, midHeight, spriteWidth, 1, TFT_BLACK);      // add in split line

  // display current value
  sprite_currentValue.pushSprite(&sprite_Display, 0, 0);

  sprite_Display.pushSprite(posX, posY);

  // animation step one. fold the top half a little
  newRangeSize = midHeight * scalefactor;
  newMaxY = midHeight + 1;
  newMinY = newMaxY - newRangeSize;
  sprite_newValue.pushSprite(&sprite_Display, 0, 0);  // set the new display and then over write parts

  for (int y = 0; y < spriteHeight; y++) {
    if (y < newMaxY) {
      newY = map(y, 0, midHeight, newMinY, newMaxY);  // re draw the  top half in less space, "revealing" the new value a little
    } else {
      newY = y;  // redraw the bottom half as is
    }
    for (int x = 0; x < spriteWidth; x++) {
      uint16_t color = sprite_currentValue.readPixel(x, y);  // Read the pixel from the original sprite
      if (((lastYrow + 1) < newY) && (lastYrow != 0)) {
        sprite_Display.drawPixel(x, newY, color);          // Draw it on the new sprite
        sprite_Display.drawPixel(x, lastYrow + 1, color);  // sometimes the map function leaves a gap from rounding, so ensure there are no gaps
      } else {
        sprite_Display.drawPixel(x, newY, color);  // Draw it on the new sprite
      }
    }
    lastYrow = newY;
  }
  sprite_Display.fillRect(0, newMinY, spriteWidth, 1, TFT_BLACK);    // simulate top of flip card
  sprite_Display.fillRect(0, midHeight, spriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(posX, posY);                             // Display  sprite
  delay(animationDelay * .9);

  // animation step two, just show the bottom, with a shadow look
  sprite_Display.fillSprite(CLOCK_BACKGROUND_COLOR);  // reset the display sprite to blank
  sprite_newValue.pushSprite(&sprite_Display, 0, 0);  // set the display to the new value and then over write parts

  lastYrow = 0;
  for (int y = midHeight; y < spriteHeight; y++) {  // leave the top alone and only draw the bottom
    for (int x = 0; x < spriteWidth; x++) {
      uint16_t color = shadow_color(sprite_currentValue.readPixel(x, y), 0.8);  // Read the pixel from the original sprite
      sprite_Display.drawPixel(x, y, color);                                    // Draw it on the new sprite
    }
  }
  sprite_Display.pushSprite(posX, posY);  // Display  sprite
  delay(animationDelay * .8);

  // animation step 3, new value top, inverted lower section
  sprite_Display.fillSprite(CLOCK_BACKGROUND_COLOR);  // reset the new sprite to blank
  sprite_currentValue.pushSprite(&sprite_Display, 0, 0);  // set the new display to the CURRENT value and then over write new parts

  lastYrow = 0;
  newMaxY = midHeight + newRangeSize;  // scale factor down from the mid point
  newMinY = midHeight - 1;             // mid point
  for (int y = 0; y < spriteHeight; y++) {
    if (y < midHeight) {  // top half so we just write the new values
      for (int x = 0; x < spriteWidth; x++) {
        uint16_t color = sprite_newValue.readPixel(x, y);  // Read the pixel from the new  value
        sprite_Display.drawPixel(x, y, color);             // Draw it on the new sprite
      }
    } else {
      newY = map(y, midHeight, spriteHeight, newMinY, newMaxY);  // map the bottom half of the new image to a constrained space
      for (int x = 0; x < spriteWidth; x++) {
        uint16_t color = sprite_newValue.readPixel(x, y);    // Read the pixel from the new  value
        if (((lastYrow + 1) < newY) && (lastYrow != 0)) {    // mapping sometimes leaves gaps, so fill them
          sprite_Display.drawPixel(x, newY, color);          // Draw it on the new sprite
          sprite_Display.drawPixel(x, lastYrow + 1, color);  // Draw it on the new sprite
        } else {
          sprite_Display.drawPixel(x, newY, color);  // Draw it on the new sprite
        }
      }
      lastYrow = newY;
      if (y > newMaxY) {  // redraw the last portion is in shadow
        for (int x = 0; x < spriteWidth; x++) {
          uint16_t color = sprite_Display.readPixel(x, y);          // Read the pixel from the new  value
          sprite_Display.drawPixel(x, y, shadow_color(color, .8));  // Draw it on the new sprite
        }
      }
    }
  }
  sprite_Display.fillRect(0, newMaxY, spriteWidth, 1, TFT_BLACK);    // simulate bottom of flip
  sprite_Display.fillRect(0, midHeight, spriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(posX, posY);                             // Display  sprite
  delay(animationDelay * .7);                                        // the animation gets a little faster as the card flips down.

  // last step draw full new value
  sprite_Display.fillSprite(CLOCK_BACKGROUND_COLOR);  // reset the new sprite to blank
  sprite_newValue.pushSprite(&sprite_Display, 0, 0);
  sprite_Display.fillRect(0, midHeight, spriteWidth, 1, TFT_BLACK);  // add in split line
  sprite_Display.pushSprite(posX, posY);                             // Display  sprite
  sprite_Display.deleteSprite();
  sprite_currentValue.deleteSprite();
  sprite_newValue.deleteSprite();
}

void setup_blip_sprites() {
  sprite_blipBackground.createSprite(14, 14);
  sprite_blipBackground.fillSprite(TFT_BLACK);  // You can change the color or use an image
  sprite_blipBackground.fillRect(2, 2, sprite_blipBackground.width() - 4, sprite_blipBackground.height() - 4, CLOCK_BACKGROUND_COLOR);

  sprite_blipCenter.createSprite(6, 6);
  sprite_blipCenter.fillSprite(TFT_WHITE);
}

void setup_large_sprite_top_border() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_LargeTopBorder.setColorDepth(8);
  sprite_LargeTopBorder.createSprite(largeSpriteWidth, TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_LargeTopBorder.fillSprite(TFT_BLACK);                             // Fill sprite with black background
  // Draw a 2-pixel white border along the long side (BORDER_LENGTH)
  sprite_LargeTopBorder.fillRect(0, TOP_BORDER_WIDTH - 1, largeSpriteWidth, 1, BORDER_HIGHLIGHT_COLOR);  // highlight
}

void setup_large_sprite_bottom_border() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_LargeBottomBorder.setColorDepth(8);
  sprite_LargeBottomBorder.createSprite(largeSpriteWidth + (2 * SIDE_BORDER_WIDTH), TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_LargeBottomBorder.fillSprite(TFT_BLACK);                                                       // Fill sprite with black background
  // Draw a 2-pixel white border along the long side (BORDER_LENGTH)
  sprite_LargeBottomBorder.fillRect(0, TOP_BORDER_WIDTH - 1, largeSpriteWidth + (2 * SIDE_BORDER_WIDTH), 1, BORDER_HIGHLIGHT_COLOR);  // highlight
}

void setup_large_sprite_center_line() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_LargeCenterLine.setColorDepth(8);
  sprite_LargeCenterLine.createSprite(largeSpriteWidth, 1);  // Create a sprite with specified width and height
  sprite_LargeCenterLine.fillSprite(TFT_BLACK);              // Fill sprite with black background
}

void add_hinge(LGFX_Sprite &thisSprite, int16_t steps) {
  uint16_t colorStartTop = TFT_WHITE;   //// was 0x75BE
  uint16_t colorEndTop = 0x75BE;        /// 0x4B05;
  uint16_t colorStartBottom = 0x75BE;   //// was 0x75BE
  uint16_t colorEndBottom = TFT_BLACK;  /// 0x4B05;

  // As LovyanGFX has no 'fillRectVGradient' method I'm emulating this by drawing gradient vertical lines
  for (int i = 0; i < SIDE_BORDER_WIDTH; i++) {
    thisSprite.drawGradientVLine(i, (thisSprite.height() + TOP_BORDER_WIDTH) / 2, steps, colorStartBottom, colorEndBottom);
    thisSprite.drawGradientVLine(i, (thisSprite.height() + TOP_BORDER_WIDTH ) / 2 - steps + 1, steps, colorStartTop, colorEndTop);
  }
}

void setup_large_sprite_side_border() {
  // Define sprite dimensions based onTOP_BORDER_WIDTH and BORDER_LENGTH
  sprite_LargeSideBorder.setColorDepth(8);
  sprite_LargeSideBorder.createSprite(SIDE_BORDER_WIDTH, largeSpriteHeight + TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_LargeSideBorder.fillSprite(TFT_BLACK);                                                  // Fill sprite with black background

  add_hinge(sprite_LargeSideBorder, 10);
}

void setup_small_sprite_top_border() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_SmallTopBorder.setColorDepth(8);
  sprite_SmallTopBorder.createSprite(smallSpriteWidth, TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_SmallTopBorder.fillSprite(TFT_BLACK);                             // Fill sprite with black background
  // Draw a 2-pixel white border along the long side (BORDER_LENGTH)
  sprite_SmallTopBorder.fillRect(0, TOP_BORDER_WIDTH - 1, smallSpriteWidth, 1, BORDER_HIGHLIGHT_COLOR);  // highlight
}

void setup_small_sprite_bottom_border() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_SmallBottomBorder.setColorDepth(8);
  sprite_SmallBottomBorder.createSprite(smallSpriteWidth + (2 * SIDE_BORDER_WIDTH), TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_SmallBottomBorder.fillSprite(TFT_BLACK);                                                       // Fill sprite with black background
  // Draw a 2-pixel white border along the long side (BORDER_LENGTH)
  sprite_SmallBottomBorder.fillRect(0, TOP_BORDER_WIDTH - 1, smallSpriteWidth + (2 * SIDE_BORDER_WIDTH), 1, BORDER_HIGHLIGHT_COLOR);  // highlight
}

void setup_small_sprite_side_border() {
  // Define sprite dimensions based onTOP_BORDER_WIDTH and BORDER_LENGTH
  sprite_SmallSideBorder.setColorDepth(8);
  sprite_SmallSideBorder.createSprite(SIDE_BORDER_WIDTH, smallSpriteHeight + TOP_BORDER_WIDTH);  // Create a sprite with specified width and height
  sprite_SmallSideBorder.fillSprite(TFT_BLACK);                                                  // Fill sprite with black background
  add_hinge(sprite_SmallSideBorder, 5);
}

void setup_small_sprite_center_line() {
  // for horizontal border,TOP_BORDER_WIDTH is the Y dimension
  sprite_SmallCenterLine.setColorDepth(8);
  sprite_SmallCenterLine.createSprite(smallSpriteWidth, 1);  // Create a sprite with specified width and height
  sprite_SmallCenterLine.fillSprite(TFT_BLACK);              // Fill sprite with black background
}

// this function drawers the  borders around a large sprite
void draw_large_borders(int16_t thisTargetXPos) {
  // top borders are the same width as the main sprite
  sprite_LargeTopBorder.pushSprite(thisTargetXPos, largeSpriteTop - TOP_BORDER_WIDTH);                          // top, top borders are as wide as the main sprite
  sprite_LargeBottomBorder.pushSprite(thisTargetXPos - SIDE_BORDER_WIDTH, largeSpriteTop + largeSpriteHeight);  // bottom, bottom borders are as wide as the main sprite and the two side borders
  sprite_LargeSideBorder.pushSprite(thisTargetXPos - SIDE_BORDER_WIDTH, largeSpriteTop - TOP_BORDER_WIDTH);     // left side, borders sit under the top border
  sprite_LargeSideBorder.pushSprite(thisTargetXPos + largeSpriteWidth, largeSpriteTop - TOP_BORDER_WIDTH);      // right
                                                                                                                //  sprite_LargeCenterLine.pushSprite( thisTargetXPos, largeSpriteTop + ( largeSpriteHeight / 2 ) - 1 ); // not drawing this here while using full hight letters because they overlap
}

// this function drawers the  borders around a small sprite
void draw_small_borders(int16_t thisTargetXPos) {
  // top borders are the same width as the main sprite
  sprite_SmallTopBorder.pushSprite(thisTargetXPos, smallSpriteTop - TOP_BORDER_WIDTH);                            // top border
  sprite_SmallBottomBorder.pushSprite((thisTargetXPos - SIDE_BORDER_WIDTH), smallSpriteTop + smallSpriteHeight);  // bottom border
  sprite_SmallSideBorder.pushSprite((thisTargetXPos - SIDE_BORDER_WIDTH), smallSpriteTop - TOP_BORDER_WIDTH);     // left border
  sprite_SmallSideBorder.pushSprite((thisTargetXPos + smallSpriteWidth), smallSpriteTop - TOP_BORDER_WIDTH);      // right border
}

// calculate the positions of all the characters, init them and draw them on screen
void setup_sprites() {
  // check to ensure that rounding errors don't lead to things not fitting on the screen.
  while ((SCREEN_X_PADDING * 2) + ((SIDE_BORDER_WIDTH * 2 + largeSpriteWidth) * LARGE_DIGIT_COUNT) + ((LARGE_DIGIT_COUNT - 1) * LARGE_DIGIT_INTER_SPACE) > SCREEN_WIDTH) {
    largeSpriteWidth--;
  }
  largeSpriteSpace = largeSpriteWidth + SIDE_BORDER_WIDTH * 2;

  // small digit have some space place holder allocation
  while ((SCREEN_X_PADDING * 2) + (((SIDE_BORDER_WIDTH * 2) + smallSpriteWidth) * (SMALL_DIGIT_COUNT + 1)) + (SMALL_DIGIT_COUNT * SMALL_DIGIT_INTER_SPACE) > SCREEN_WIDTH) {
    smallSpriteWidth--;
  }
  smallSpriteSpace = smallSpriteWidth + TOP_BORDER_WIDTH * 2;

  h1X = SCREEN_X_PADDING + SIDE_BORDER_WIDTH;
  h2X = SCREEN_X_PADDING + (largeSpriteSpace + LARGE_DIGIT_INTER_SPACE) + SIDE_BORDER_WIDTH;
  m1X = SCREEN_X_PADDING + (largeSpriteSpace + LARGE_DIGIT_INTER_SPACE) * 2 + SIDE_BORDER_WIDTH;
  m2X = SCREEN_X_PADDING + (largeSpriteSpace + LARGE_DIGIT_INTER_SPACE) * 3 + SIDE_BORDER_WIDTH;

  DOW1X = (SCREEN_X_PADDING + smallSpriteSpace * 0 + SIDE_BORDER_WIDTH);
  DOW2X = (SCREEN_X_PADDING + (smallSpriteSpace + SMALL_DIGIT_INTER_SPACE) * 1 + SIDE_BORDER_WIDTH);
  DOW3X = (SCREEN_X_PADDING + (smallSpriteSpace + SMALL_DIGIT_INTER_SPACE) * 2 + SIDE_BORDER_WIDTH);

  // pushing 4 pixels to the right to get a space for the blink
  const uint8_t smallXOffset = 4;
  Date1X = (centerX - (smallSpriteSpace - SIDE_BORDER_WIDTH) - SMALL_DIGIT_INTER_SPACE) + smallXOffset;
  Date2X = (centerX + SIDE_BORDER_WIDTH + SMALL_DIGIT_INTER_SPACE) + smallXOffset;
  Month1X = (SCREEN_WIDTH - (SCREEN_X_PADDING + (smallSpriteSpace + SMALL_DIGIT_INTER_SPACE) * 3 + SIDE_BORDER_WIDTH)) + smallXOffset;
  Month2X = (SCREEN_WIDTH - (SCREEN_X_PADDING + (smallSpriteSpace + SMALL_DIGIT_INTER_SPACE) * 2 + SIDE_BORDER_WIDTH)) + smallXOffset;
  Month3X = (SCREEN_WIDTH - (SCREEN_X_PADDING + (smallSpriteSpace + SMALL_DIGIT_INTER_SPACE) + SIDE_BORDER_WIDTH)) + smallXOffset;

  setup_large_sprite_top_border();
  setup_large_sprite_bottom_border();
  setup_large_sprite_side_border();
  setup_large_sprite_center_line();

  setup_small_sprite_top_border();
  setup_small_sprite_bottom_border();
  setup_small_sprite_side_border();
  setup_small_sprite_center_line();

  draw_large_borders(h1X);
  draw_large_borders(h2X);
  draw_large_borders(m1X);
  draw_large_borders(m2X);

  draw_small_borders(DOW1X);
  draw_small_borders(DOW2X);
  draw_small_borders(DOW3X);
  draw_small_borders(Month1X);
  draw_small_borders(Month2X);
  draw_small_borders(Month3X);
  draw_small_borders(Date1X);
  draw_small_borders(Date2X);

  setup_blip_sprites();
  sprite_blipBackground.pushSprite(blinkSpriteX, blinkSpriteTop);
  sprite_blipBackground.pushSprite(blinkSpriteX, blinkSpriteTop + sprite_blipBackground.height() + SMALL_DIGIT_INTER_SPACE);

}

void init_TFT_screen() {
  // Start the tft display
  tft.init();
  // Set the TFT display rotation in landscape mode
  tft.setRotation(3); // 3 = terminal left side, 2 = USB Top, 1 = terminal right side
  // Clear the screen before writing to it
  tft.fillScreen(CLOCK_BACKGROUND_COLOR);
}

void update_clock(bool force_update) {
  time_t now = time(NULL);  // Get current time as time_t
  struct tm timeInfo;
  localtime_r(&now, &timeInfo);  // Convert time_t to struct tm

  int nowDOWNumber = timeInfo.tm_wday;  // Get day of week (0 = Sunday, 6 = Saturday)
  int hours = timeInfo.tm_hour;
  int nowH1;
  int nowH2;
  if (!MilTime && (hours > 12)) {
    hours = hours - 12;
    nowH1 = hours / 10;  // First hour digit
    nowH2 = hours % 10;  // Second hour digit
  } else {
    nowH1 = hours / 10;  // First hour digit
    nowH2 = hours % 10;  // Second hour digit
  }

  int nowM1 = timeInfo.tm_min / 10;  // First minute digit
  int nowM2 = timeInfo.tm_min % 10;  // Second minute digit
  int nowMonthNumber = timeInfo.tm_mon;
  int nowDate1 = timeInfo.tm_mday / 10;
  int nowDate2 = timeInfo.tm_mday % 10;

  char nowMonthCharacter[2];
  nowMonthCharacter[0] = monthsOfYear[nowMonthNumber][0];
  nowMonthCharacter[1] = '\0';

  char placeholderNow[2];
  char placeholderCurrent[2];

  char DOWnowCharacter[2];
  DOWnowCharacter[0] = daysOfWeek[nowDOWNumber][0];
  DOWnowCharacter[1] = '\0';

  if ((currentMinute2 != nowM2) || force_update) {
    snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowM2 + '0');
    snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentMinute2 + '0');
    change_sprite_display(placeholderCurrent, placeholderNow, m2X, largeSpriteTop, true);
    currentMinute2 = nowM2;
  }

  if ((currentMinute1 != nowM1) || force_update) {
    snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowM1 + '0');
    snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentMinute1 + '0');
    change_sprite_display(placeholderCurrent, placeholderNow, m1X, largeSpriteTop, true);
    currentMinute1 = nowM1;
  }
  if ((currentHour2 != nowH2) || force_update) {
    snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowH2 + '0');
    snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentHour2 + '0');
    change_sprite_display(placeholderCurrent, placeholderNow, h2X, largeSpriteTop, true);
    currentHour2 = nowH2;
  }

  if ((currentHour1 != nowH1) || force_update) {
    snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowH1 + '0');
    snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentHour1 + '0');
    change_sprite_display(placeholderCurrent, placeholderNow, h1X, largeSpriteTop, true);
    currentHour1 = nowH1;
    if ((nowH2 == 0) || force_update) {  // it is a new day
      Serial.printf("Current day of week is %s to start\n", currentDOW);
      Serial.printf("Current Month  is %s to start\n", currentMonth);
      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentDOW[0]);
      DOWnowCharacter[0] = daysOfWeek[nowDOWNumber][0];
      change_sprite_display(placeholderCurrent, DOWnowCharacter, DOW1X, smallSpriteTop, false);

      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentDOW[1]);
      DOWnowCharacter[0] = daysOfWeek[nowDOWNumber][1];
      change_sprite_display(placeholderCurrent, DOWnowCharacter, DOW2X, smallSpriteTop, false);

      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentDOW[2]);
      DOWnowCharacter[0] = daysOfWeek[nowDOWNumber][2];
      change_sprite_display(placeholderCurrent, DOWnowCharacter, DOW3X, smallSpriteTop, false);

      strcpy(currentDOW, &daysOfWeek[nowDOWNumber][0]);  // we get everything until the first null character

      snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowDate1 + '0');
      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentDate1 + '0');
      change_sprite_display(placeholderCurrent, placeholderNow, Date1X, smallSpriteTop, false);

      snprintf(placeholderNow, sizeof(placeholderNow), "%c", nowDate2 + '0');              // conv int to character
      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentDate2 + '0');  // conv int to character
      change_sprite_display(placeholderCurrent, placeholderNow, Date2X, smallSpriteTop, false);

      currentDate1 = nowDate1;
      currentDate2 = nowDate2;

      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentMonth[0]);
      nowMonthCharacter[0] = monthsOfYear[nowMonthNumber][0];
      change_sprite_display(placeholderCurrent, nowMonthCharacter, Month1X, smallSpriteTop, false);

      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentMonth[1]);
      nowMonthCharacter[0] = monthsOfYear[nowMonthNumber][1];
      change_sprite_display(placeholderCurrent, nowMonthCharacter, Month2X, smallSpriteTop, false);

      snprintf(placeholderCurrent, sizeof(placeholderCurrent), "%c", currentMonth[2]);
      nowMonthCharacter[0] = monthsOfYear[nowMonthNumber][2];
      change_sprite_display(placeholderCurrent, nowMonthCharacter, Month3X, smallSpriteTop, false);

      strcpy(currentMonth, &monthsOfYear[nowMonthNumber][0]);
      Serial.printf("Current DOW is %s to end\n", currentDOW);
      Serial.printf("Current Month  is %s to end\n", currentMonth);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("ESP32-S3 for TFT ST7789 1,9 in 170 x 320 pixels display Flip Clock V05");

  // Init buttons
  pinMode(BOOT_BUTTON, INPUT_PULLUP);
  pinMode(KEY_BUTTON, INPUT_PULLUP);

  // When the device is powered by USB the display is set ON by default,
  // but when powering the device by a battery you need to set the pin to HIGH
  // or the display stays dark
  pinMode(TFT_POWER, OUTPUT);
  digitalWrite(TFT_POWER, HIGH);

  // debug
  Serial.printf("largeSpriteWidth: %d largeSpriteSpace: %d largeSpriteHeight: %d largeSpriteTop: %d\n", largeSpriteWidth, largeSpriteSpace, largeSpriteHeight, largeSpriteTop);
  Serial.printf("smallSpriteWidth: %d smallSpriteSpace: %d smallSpriteHeight: %d smallSpriteTop: %d\n", smallSpriteWidth, smallSpriteSpace, smallSpriteHeight, smallSpriteTop);

  tft.setBrightness(displayBrightness); // default brightness

  init_TFT_screen();

  tft.fillScreen(CLOCK_BACKGROUND_COLOR);
  tft.drawCentreString("Connecting to wifi.", centerX, 75, 4);

  // Obtain current time and set variables for the Second, Minute and Hour
  configTime(0, 0, ntpServer);   // 0, 0 because we will use TZ in the next line
  setenv("TZ", MY_TIMEZONE, 1);  // Set environment variable with your time zone
  tzset();

  // Timeout variables
  unsigned long wifiStartTime = millis();
  const unsigned long wifiTimeout = 40000;  // 20 seconds
  // Wait for connection
  Serial.println(ssid);
  Serial.println(WiFipassword);
  WiFi.setMinSecurity(WIFI_AUTH_WPA2_PSK);
  WiFi.begin(ssid, WiFipassword);

  while (WiFi.status() != WL_CONNECTED && (millis() - wifiStartTime) < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }

  // Check if connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected!");
    Serial.println("IP Address: ");
    Serial.println(WiFi.localIP());
    tft.fillScreen(CLOCK_BACKGROUND_COLOR);
    setup_sprites();
    please_wait();
    Serial.println("");
    // Set up time via NTP
    //configTime( gmtOffset_hours * 3600, daylightOffset_sec, ntpServer );
    firstpass = true;
  } else {
    Serial.println("\nWi-Fi Connection Failed.");
    tft.fillScreen(CLOCK_BACKGROUND_COLOR);
    tft.drawCentreString("WiFi connection failed.", centerX, 115, 4);
  }

  //  tft.drawCentreString( "WiFi connected.", centerX, 75, 4 );
}

void loop() {
  unsigned long currentMillis = millis();
  if (WiFi.status() == WL_CONNECTED) {  // with no wifi connection, do nothing
    time_t now = time(nullptr);
    if (now > 1700000000) {  // do not display the clock until we get ntp time
      if (firstpass) {
        update_clock(true);
        firstpass = false;
      } else {
        update_clock(false);
      }

      // If one second has passed
      if (currentMillis - lastMillis >= 1000) {
        lastMillis = currentMillis;
        
        if (blipCycle == 0) {
          sprite_blipBackground.pushSprite(blinkSpriteX, blinkSpriteTop + sprite_blipBackground.height() + SMALL_DIGIT_INTER_SPACE);
          sprite_blipCenter.pushSprite(blinkSpriteX + 4, blinkSpriteTop + (sprite_blipBackground.height() / 2 - sprite_blipCenter.height() / 2));
          blipCycle = 1;
        } else {
          sprite_blipBackground.pushSprite(blinkSpriteX, blinkSpriteTop);
          sprite_blipCenter.pushSprite(blinkSpriteX + 4, blinkSpriteTop + sprite_blipBackground.height() + SMALL_DIGIT_INTER_SPACE + (sprite_blipBackground.height() / 2 - sprite_blipCenter.height() / 2));
          blipCycle = 0;
        }
      }
    }
    //delay(200);
  }

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
    tft.setBrightness(displayBrightness);
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
    tft.setBrightness(displayBrightness);
  }
}
