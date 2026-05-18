/**
 * UGENT ESP32 Monitor — Configuration
 *
 * Hardware: ESP32-2432S028R (2.8" 320x240 ILI9341 + XPT2046)
 * Framework: Arduino + LVGL 8.x + TFT_eSPI
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// ─── Firmware ────────────────────────────────────────────────────────────────
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_NAME    "UGENT Monitor"

// ─── Display ─────────────────────────────────────────────────────────────────
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 240

// TFT (ILI9341) SPI pins — matches User_Setup.h from vendor
#define TFT_MOSI  13
#define TFT_SCLK  14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   12
#define TFT_BL    21

// Touch (XPT2046) SPI pins
#define TOUCH_DOUT 39   // MISO
#define TOUCH_DIN  32   // MOSI
#define TOUCH_CS   33
#define TOUCH_CLK  25

// Touch calibration (from vendor LVGL example)
#define TOUCH_CAL_0   526
#define TOUCH_CAL_1  3443
#define TOUCH_CAL_2   750
#define TOUCH_CAL_3  3377

// Display rotation: 1 = landscape (320x240)
#define TFT_ROTATION 1

// LVGL draw buffer size (in pixels) — use partial buffer to save RAM
// screenWidth * 10 as per vendor example
#define LVGL_BUF_SIZE (SCREEN_WIDTH * 10)

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
#define MAX_MESSAGE_LEN  256

// ─── Limits ───────────────────────────────────────────────────────────────────
#define MAX_TASKS_DISPLAY      20
#define MAX_MESSAGES_HISTORY   20
#define MAX_WIFI_NETWORKS      15

// ─── UI Theme Colors (Catppuccin Mocha-inspired dark theme) ───────────────────
// Using RGB565 format for LVGL
// Base: #1e1e2e → RGB565
#define COLOR_BG           0x18E5   // Base (dark blue-gray)
#define COLOR_SURFACE0     0x2108   // Surface0 (slightly lighter)
#define COLOR_SURFACE1     0x294A   // Surface1
#define COLOR_OVERLAY0     0x318C   // Overlay0
#define COLOR_TEXT          0xFFFF   // Text (white)
#define COLOR_SUBTEXT      0xB5B6   // Subtext1 (light gray)
#define COLOR_ACCENT       0x049F   // Blue accent (#89b4fa → RGB565)
#define COLOR_ACCENT_GREEN 0x07E4   // Green (#a6e3a1)
#define COLOR_ACCENT_RED   0x049F   // Red (#f38ba8 → RGB565 approx)
#define COLOR_ACCENT_YELLOW 0xADA0  // Yellow (#f9e2af)
#define COLOR_RUNNING      0x07E4   // Green for running status
#define COLOR_COMPLETED    0x049F   // Blue for completed
#define COLOR_FAILED       0xF800   // Red for failed
#define COLOR_PENDING      0xB5B6   // Gray for pending
#define COLOR_TAB_ACTIVE   0x049F   // Blue for active tab
#define COLOR_TAB_INACTIVE 0x318C   // Gray for inactive tab

#endif // CONFIG_H
