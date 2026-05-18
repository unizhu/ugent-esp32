//  TFT_eSPI User_Setup.h for ESP32-2432S028R (Cheap Yellow Display)
//
//  INSTALLATION:
//    Copy this file to your Arduino TFT_eSPI library folder:
//      <Arduino>/libraries/TFT_eSPI/User_Setup.h
//
//    Or use User_Setup_Select.h to point to this file.
//
//  Based on:
//    - Vendor (Waveshare) User_Setup.h
//    - witnessmenow/ESP32-Cheap-Yellow-Display setup
//    - ropg/LVGL_CYD proven configuration
//
//  KEY DECISIONS:
//    - USE_HSPI_PORT: Display uses HSPI (SPI2), touch uses VSPI (SPI3)
//      This avoids the hardware SPI conflict that causes B&W + flickering
//    - TOUCH_CS is NOT defined: We handle touch via separate SPIClass(HSPI)
//      TFT_eSPI must NOT touch GPIO 33
//    - ILI9341_2_DRIVER: Alternative driver for better compatibility

#define USER_SETUP_INFO "UGENT_CYD_2432S028R"

// ─── Display Driver ───────────────────────────────────────────────────────────
#define ILI9341_2_DRIVER   // Alternative ILI9341 driver (better for this board)

// ─── Display Dimensions (portrait, rotation handled in software) ───────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── Display SPI Pins (HSPI hardware) ─────────────────────────────────────────
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15    // Chip select
#define TFT_DC    2    // Data/Command
#define TFT_RST  12    // Reset
#define TFT_BL   21    // Backlight

// ─── SPI Port Selection ────────────────────────────────────────────────────────
// CRITICAL: Must use HSPI so TFT_eSPI uses SPI2 (not SPI3/VSPI).
// Our touch controller uses VSPI (SPI3) via separate SPIClass.
// Without USE_HSPI_PORT, both display and touch fight over SPI3 → B&W + flicker.
#define USE_HSPI_PORT

// ─── Backlight ─────────────────────────────────────────────────────────────────
#define TFT_BACKLIGHT_ON HIGH

// ─── Touch ─────────────────────────────────────────────────────────────────────
// DO NOT define TOUCH_CS here! Touch is handled by our own SPIClass on VSPI.
// Defining TOUCH_CS would make TFT_eSPI try to control GPIO 33, causing conflicts.
// #define TOUCH_CS 33    // <-- COMMENTED OUT

// ─── Fonts ─────────────────────────────────────────────────────────────────────
#define LOAD_GLCD    // Font 1: 8px, ~1820 bytes
#define LOAD_FONT2   // Font 2: 16px, ~3534 bytes
#define LOAD_FONT4   // Font 4: 26px, ~5848 bytes
#define LOAD_FONT6   // Font 6: 48px, numbers only
#define LOAD_FONT7   // Font 7: 7-segment 48px, numbers only
#define LOAD_FONT8   // Font 8: 75px, numbers only
#define LOAD_GFXFF   // FreeFonts (FF1–FF48)
#define SMOOTH_FONT

// ─── SPI Speed ─────────────────────────────────────────────────────────────────
#define SPI_FREQUENCY         40000000   // 40MHz for display (safe for most boards)
#define SPI_READ_FREQUENCY    20000000   // 20MHz for display reads
// SPI_TOUCH_FREQUENCY not needed — touch is on separate SPI bus
