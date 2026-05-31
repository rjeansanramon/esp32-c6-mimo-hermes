// AI Pocket - TFT_eSPI User Setup for ESP32-C6 Waveshare 1.47" Display
// 
// Copy this file to your TFT_eSPI library folder:
//   Arduino/libraries/TFT_eSPI/User_Setup.h
//
// Or place in your sketch folder and include:
//   #include "User_Setup.h"
//   #include <TFT_eSPI.h>

// ============== DRIVER ==============
#define USER_SETUP_INFO "AI-Pocket-ESP32-C6-1.47"

#define ST7789_DRIVER

// ============== COLOR ORDER ==============
#define TFT_RGB_ORDER TFT_RGB

// ============== DISPLAY DIMENSIONS ==============
// For ESP32-C6-LCD-1.47: 172 x 320 pixels
#define TFT_WIDTH  172
#define TFT_HEIGHT 320

// ============== SPI PINS ==============
// ESP32-C6 Waveshare 1.47" LCD pin mapping
#define TFT_MOSI 6     // SPI MOSI (SDA)
#define TFT_SCLK 7     // SPI Clock (SCL)
#define TFT_CS   14    // Chip Select
#define TFT_DC   15    // Data/Command
#define TFT_RST  21    // Reset

// ============== BACKLIGHT ==============
#define TFT_BL   22    // Backlight control pin
#define TFT_BACKLIGHT_ON HIGH

// ============== SPI SPEED ==============
#define SPI_FREQUENCY  40000000   // 40 MHz

// ============== OPTIONAL ==============
// #define TFT_INVERSION_ON
// #define TFT_INVERSION_OFF

// ============== LOAD FONTS ==============
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
