/**
 * UGENT ESP32 Monitor — Configuration
 *
 * Hardware: ESP32-2432S028R (2.8" 320x240 ILI9341 + XPT2046)
 * Framework: Arduino + LVGL 8.x + TFT_eSPI
 *
 * CONFIGURATION: Use the vendor's exact User_Setup.h from:
 *   2.8inch_ESP32-2432S028R/1-Demo/Demo_Arduino/3_4-4_2.8-LVGL_Arduino/
 *   TFT_eSPI bottom layer replacement file/User_Setup.h
 *   Copy to ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// ─── ESP32 Core Compatibility ─────────────────────────────────────────────────
// Arduino-ESP32 Core 3.x removed ledcSetup/ledcAttachPin in favor of ledcAttach
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    #define UGENT_LEDC_NEW_API 1
#else
    #define UGENT_LEDC_NEW_API 0
#endif

// ─── Firmware ────────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION "1.2.0"
#define FIRMWARE_NAME    "UGENT Monitor"

// ─── Display ─────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// Display pins — configured in TFT_eSPI's User_Setup.h (vendor version)
// TFT_MOSI=13, TFT_SCLK=14, TFT_CS=15, TFT_DC=2, TFT_RST=12, TFT_BL=21

// Touch pins — used by TFT_Touch library (bit-banged GPIO)
// Same as vendor LVGL example: CS=33, CLK=25, DIN=32, DOUT=39
// Touch calibration: setCal(526, 3443, 750, 3377, 320, 240, 1)
// Backlight PWM (GPIO 21 on ESP32-2432S028R)
#define LCD_BL_PIN            21
#define BACKLIGHT_PWM_CHANNEL 0
#define BACKLIGHT_PWM_FREQ    5000   // 5 kHz
#define BACKLIGHT_PWM_RES     8      // 8-bit resolution (0–255)

// ─── LEDC Compatibility Helpers ───────────────────────────────────────────────
// Provide unified API for both ESP32 Core 2.x and 3.x

static inline void ugent_ledc_init(uint8_t pin, uint8_t channel, uint32_t freq, uint8_t res) {
#if UGENT_LEDC_NEW_API
    // Core 3.x: ledcAttach(pin, freq, res) — auto channel management
    ledcAttach(pin, freq, res);
#else
    // Core 2.x: explicit channel setup
    ledcSetup(channel, freq, res);
    ledcAttachPin(pin, channel);
#endif
}

static inline void ugent_ledc_write(uint8_t pin, uint8_t channel, uint32_t duty) {
#if UGENT_LEDC_NEW_API
    // Core 3.x: ledcWrite uses pin directly
    ledcWrite(pin, duty);
#else
    // Core 2.x: ledcWrite uses channel
    ledcWrite(channel, duty);
#endif
}

// LVGL draw buffer size (in pixels)
// 10 lines × 320 pixels with double buffering for flicker-free rendering
#define LVGL_BUF_SIZE (SCREEN_WIDTH * 10)

// ─── Display rotation ─────────────────────────────────────────────────────────
// Rotation 1 = landscape (320×240)
#define TFT_ROTATION 1
#define ROTATION    TFT_ROTATION  // Alias used by main .ino

// ─── NVS Storage Keys ────────────────────────────────────────────────────────
#define NVS_NAMESPACE "ugent"

// WiFi
#define NVS_KEY_WIFI_SSID     "wifi_ssid"
#define NVS_KEY_WIFI_PASS     "wifi_pass"

// UGENT server
#define NVS_KEY_UGENT_HOST    "ugent_host"
#define NVS_KEY_UGENT_PORT    "ugent_port"
#define NVS_KEY_UGENT_APIKEY  "ugent_apikey"

// Display
#define NVS_KEY_BRIGHTNESS    "brightness"

// ─── UGENT Server API Paths ────────────────────────────────────────────────────
#define UGENT_STATUS_PATH   "/v1/ugent/status"
#define UGENT_EVENTS_PATH   "/v1/ugent/events"
#define UGENT_SSE_PATH      "/v1/ugent/events"
#define UGENT_COMMANDS_PATH "/v1/ugent/commands"

// ─── Defaults ─────────────────────────────────────────────────────────────────
#define DEFAULT_UGENT_HOST   ""
#define DEFAULT_UGENT_PORT   8765
#define DEFAULT_UGENT_APIKEY ""
#define DEFAULT_BRIGHTNESS   80   // 0–100

// ─── Timing ──────────────────────────────────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS   10000  // 10 seconds
#define WIFI_RECONNECT_DELAY_MS   5000   // 5 seconds
#define STATUS_POLL_INTERVAL_MS   5000   // 5 seconds
#define HTTP_TIMEOUT_MS           5000   // 5 seconds
#define SSE_RECONNECT_BASE_MS     1000   // 1 second base, exponential backoff
#define SSE_RECONNECT_MAX_MS      30000  // 30 seconds max
#define LVGL_TICK_PERIOD_MS       5      // LVGL tick handler interval
#define SCREENSAVER_TIMEOUT_MS    60000  // Auto-dim after 60 seconds

// ─── Strings ──────────────────────────────────────────────────────────────────
#define MAX_SSID_LEN     32
#define MAX_PASS_LEN     64
#define MAX_HOST_LEN     64
#define MAX_APIKEY_LEN   128
#define MAX_VERSION_LEN  16
#define MAX_TASK_NAME    40
#define MAX_STATUS_STR   16
#define MAX_MESSAGE_LEN  128
#define MAX_SSE_DATA_LEN  256

// ─── Limits ───────────────────────────────────────────────────────────────────
#define MAX_TASKS_DISPLAY      8
#define MAX_MESSAGES_HISTORY   8
#define MAX_WIFI_NETWORKS      15

// ─── UI Theme Colors (Catppuccin Mocha-inspired dark theme) ───────────────────
// Using lv_color_hex() in ui_theme.h — these RGB565 defines are for reference
#define COLOR_BG           0x18E5   // Base (dark blue-gray)
#define COLOR_SURFACE0     0x2108   // Surface0 (slightly lighter)
#define COLOR_SURFACE1     0x294A   // Surface1
#define COLOR_OVERLAY0     0x318C   // Overlay0
#define COLOR_TEXT          0xFFFF   // Text (white)
#define COLOR_SUBTEXT      0xB5B6   // Subtext1 (light gray)
#define COLOR_ACCENT       0x049F   // Blue accent
#define COLOR_ACCENT_GREEN 0x07E4   // Green
#define COLOR_ACCENT_RED   0xF455   // Red
#define COLOR_ACCENT_YELLOW 0xADA0  // Yellow
#define COLOR_RUNNING      0x07E4   // Green for running status
#define COLOR_COMPLETED    0x049F   // Blue for completed
#define COLOR_FAILED       0xF800   // Red for failed
#define COLOR_PENDING      0xB5B6   // Gray for pending
#define COLOR_TAB_ACTIVE   0x049F   // Blue for active tab
#define COLOR_TAB_INACTIVE 0x318C   // Gray for inactive tab

#endif // CONFIG_H
