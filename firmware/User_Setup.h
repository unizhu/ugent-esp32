//  TFT_eSPI User_Setup.h for ESP32-2432S028R (Cheap Yellow Display)
//
//  INSTALLATION:
//    Copy this file to:
//      ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//
//    Also copy lv_conf.h to:
//      ~/Documents/Arduino/libraries/lv_conf.h
//
//  Based on Waveshare vendor configuration (proven working):
//    - ILI9341_2_DRIVER
//    - NO USE_HSPI_PORT (ESP32 GPIO matrix routes VSPI to any pins)
//    - TOUCH_CS 33 defined (TFT_eSPI inits GPIO 33; TFT_Touch also uses it)
//    - SPI 40MHz (safe across all board revisions)
//
//  IMPORTANT:
//    Do NOT define USE_HSPI_PORT! HSPI default MISO = GPIO 12 = TFT_RST.
//    This conflict causes color corruption (grey vertical lines on some colors).
//    ESP32 GPIO matrix allows VSPI to use pins 13/14/15 without any conflict.

#define USER_SETUP_INFO "UGENT_CYD_2432S028R"

// ─── Display Driver ───────────────────────────────────────────────────────────
#define ILI9341_2_DRIVER

// ─── Display Dimensions (portrait; rotation handled in software) ──────────────
#define TFT_WIDTH  240
#define TFT_HEIGHT 320

// ─── ESP32 Display SPI Pins ───────────────────────────────────────────────────
//  These match the HSPI default pins (13/14/15) but VSPI can use them too
//  via the ESP32 GPIO matrix — no USE_HSPI_PORT needed.
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  12
#define TFT_BL   21

// ─── Backlight ────────────────────────────────────────────────────────────────
#define TFT_BACKLIGHT_ON HIGH

// ─── Touch ────────────────────────────────────────────────────────────────────
//  Define TOUCH_CS so TFT_eSPI initializes GPIO 33 during begin().
//  Actual touch reads are done by TFT_Touch library (bit-banged on 33,25,32,39).
#define TOUCH_CS 33

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
//  40MHz is reliable across all ESP32-2432S028R board revisions.
//  Vendor uses 55-65MHz but some boards have longer traces.
#define SPI_FREQUENCY         40000000
#define SPI_READ_FREQUENCY    20000000
#define SPI_TOUCH_FREQUENCY   2000000
