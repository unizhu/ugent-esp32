//  TFT_eSPI User_Setup.h for ESP32-2432S028R (Cheap Yellow Display)
//
//  Based on witnessmenow/ESP32-Cheap-Yellow-Display configuration:
//    https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display
//
//  KEY FACTS (from CYD hardware documentation):
//    - Display (ILI9341) is on HSPI bus: MOSI=13, SCLK=14, CS=15, DC=2, MISO=12
//    - Touch (XPT2046) is on a SEPARATE VSPI bus: CLK=25, MOSI=32, MISO=39, CS=33, IRQ=36
//    - SD card is on the default VSPI bus: MOSI=23, SCK=18, MISO=19, SS=5
//    - GPIO 12 is TFT MISO (NOT reset!) — the display reset is tied to board reset
//    - Backlight: GPIO 21
//
//  INSTALLATION:
//    Copy this file to: ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h

#define USER_SETUP_INFO "UGENT_CYD_2432S028R"

// ─── Display Driver ───────────────────────────────────────────────────────────
#define ILI9341_2_DRIVER

// ─── Display Dimensions (portrait; rotation handled in software) ──────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── ESP32 Display SPI Pins (HSPI bus) ────────────────────────────────────────
//  The CYD display is wired to HSPI default pins.
//  USE_HSPI_PORT is REQUIRED so TFT_eSPI uses the HSPI hardware.
#define TFT_MISO 12    // Display MISO (GPIO 12 is TFT_SDO on CYD schematic)
#define TFT_MOSI 13    // Display MOSI
#define TFT_SCLK 14    // Display SCK
#define TFT_CS   15    // Chip select
#define TFT_DC    2    // Data/Command
#define TFT_RST  -1    // Reset tied to board reset (not a GPIO)
#define TFT_BL   21    // Backlight

// ─── SPI Port ──────────────────────────────────────────────────────────────────
//  CRITICAL: Display is on HSPI. Touch uses a separate VSPI instance.
#define USE_HSPI_PORT

// ─── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BACKLIGHT_ON HIGH

// ─── Touch ────────────────────────────────────────────────────────────────────
//  DO NOT define TOUCH_CS here!
//  Touch is on a completely separate VSPI bus (CLK=25, MOSI=32, MISO=39, CS=33).
//  Using XPT2046_Touchscreen library with SPIClass(VSPI).
//  If TOUCH_CS is defined, TFT_eSPI will try to touch GPIO 33 on the wrong SPI bus.

// ─── Fonts ────────────────────────────────────────────────────────────────────
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// ─── SPI Speed ────────────────────────────────────────────────────────────────
#define SPI_FREQUENCY         55000000
#define SPI_READ_FREQUENCY    20000000
// SPI_TOUCH_FREQUENCY not needed — touch is on separate VSPI bus
