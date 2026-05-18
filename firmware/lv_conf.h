/**
 * LVGL 8.x Configuration for UGENT ESP32 Monitor
 *
 * INSTALLATION:
 *   Copy this file to: <Arduino>/libraries/lv_conf.h
 *   (next to the lvgl/ folder, NOT inside it)
 *
 *   Correct layout:
 *     Arduino/
 *       libraries/
 *         lvgl/
 *         TFT_eSPI/
 *         ...
 *         lv_conf.h   ← THIS FILE
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ==================== DISPLAY ==================== */

/* Color depth: 1 (1-byte-per-pixel), 8, 16, or 32 */
#define LV_COLOR_DEPTH 16

/* Swap the 2 bytes of RGB565 color (useful for SPI displays) */
/* MUST be 1 for ESP32-2432S028R — ILI9341 SPI needs byte swap for correct colors */
#define LV_COLOR_16_SWAP 1

/* Enable more complex drawing routines for screen transparency */
#define LV_COLOR_SCREEN_TRANSP 0

/* ==================== MEMORY ==================== */

/* Size of the graphics buffer (in pixels) — set in config.h as LVGL_BUF_SIZE */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (64U * 1024U)          /* 64KB heap for LVGL objects */
#define LV_MEM_ADR 0
#define LV_MEM_BUF_MAX_NUM 16

/* ==================== HAL SETTINGS ==================== */

/* Default display refresh period (ms) — 20ms for smoother rendering */
#define LV_DISP_DEF_REFR_PERIOD 20

/* Input device read period (ms) — 20ms for responsive touch */
#define LV_INDEV_DEF_READ_PERIOD 20

/* Use a custom tick source (Arduino millis()) */
#define LV_TICK_CUSTOM 1
#if LV_TICK_CUSTOM
    #define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
    #define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())
#endif

/* ==================== FONT ==================== */

/* Built-in fonts: Montserrat with various sizes */
#define LV_FONT_MONTSERRAT_8   0
#define LV_FONT_MONTSERRAT_10  1
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_18  0
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_22  0
#define LV_FONT_MONTSERRAT_24  0
#define LV_FONT_MONTSERRAT_26  0
#define LV_FONT_MONTSERRAT_28  0
#define LV_FONT_MONTSERRAT_30  0
#define LV_FONT_MONTSERRAT_32  0
#define LV_FONT_MONTSERRAT_34  0
#define LV_FONT_MONTSERRAT_36  0
#define LV_FONT_MONTSERRAT_38  0
#define LV_FONT_MONTSERRAT_40  0
#define LV_FONT_MONTSERRAT_42  0
#define LV_FONT_MONTSERRAT_44  0
#define LV_FONT_MONTSERRAT_46  0
#define LV_FONT_MONTSERRAT_48  0

/* Demonstrate special features */
#define LV_FONT_MONTSERRAT_12_SUBPX       0
#define LV_FONT_MONTSERRAT_28_COMPRESSED   0
#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW   0
#define LV_FONT_SIMSUN_16_CJK             0
#define LV_FONT_UNSCII_8                   0
#define LV_FONT_UNSCII_16                  0

/* Default font used when not specified */
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Enable font format support */
#define LV_FONT_FMT_TXT_LARGE 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_FONT_SUBPX     0
#if LV_USE_FONT_SUBPX
    #define LV_FONT_SUBPX_BGR 0
#endif

/* ==================== TEXT ==================== */

#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"

/* ==================== WIDGETS ==================== */

/* Enable all standard LVGL 8.x widgets */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CANVAS     0
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_LABEL      1
#define LV_USE_LINE       1
#define LV_USE_ROLLER     1
#define LV_USE_SLIDER     1
#define LV_USE_SWITCH     1
#define LV_USE_TEXTAREA   1
#define LV_USE_TABLE      0

/* ==================== EXTRA WIDGETS ==================== */

#define LV_USE_CHART      0
#define LV_USE_LED        0
#define LV_USE_LIST       1
#define LV_USE_MSGBOX     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_TABVIEW    1
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0
#define LV_USE_MENU       0
#define LV_USE_KEYBOARD   1

/* ==================== THEMES ==================== */

/* Default theme (used as fallback) */
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
    /* 0: Light mode, 1: Dark mode */
    #define LV_THEME_DEFAULT_DARK 1
    /* Allow grow animations on press */
    #define LV_THEME_DEFAULT_GROW 1
    /* Default transition time [ms] */
    #define LV_THEME_DEFAULT_TRANSITION_TIME 80
#endif

#define LV_USE_THEME_BASIC    1

/* ==================== KEYBOARD ==================== */

#if LV_USE_KEYBOARD
    #define LV_KEYBOARD_DEF_KB_WIDTH 40
#endif

/* ==================== LAYOUTS ==================== */

#define LV_USE_FLEX 1
#define LV_USE_GRID 1

/* ==================== ANIMATION ==================== */

#define LV_USE_ANIMATION 1

/* ==================== LOG ==================== */

#define LV_USE_LOG 1
#if LV_USE_LOG
    /* Log level: TRACE, INFO, WARN, ERROR, USER, NONE */
    #define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
    /* 1: use printf; 0: use custom callback */
    #define LV_LOG_PRINTF 1
    /* Enable/disable tracing of object creations */
    #define LV_LOG_TRACE_MEM_CREATE  0
    #define LV_LOG_TRACE_OBJ_CREATE  0
    #define LV_LOG_TRACE_CHG_LAYER   0
    #define LV_LOG_TRACE_INDEV       0
#endif

/* ==================== ASSERT ==================== */

#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MALLOC        1
#define LV_USE_ASSERT_STYLE         0
#define LV_USE_ASSERT_MEM_INTEGRITY 0
#define LV_USE_ASSERT_OBJ           0

/* ==================== GPU ==================== */

#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/* ==================== MISC ==================== */

#define LV_USE_SNAPSHOT 0
#define LV_USE_SYSMON   0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR   0

#endif /* LV_CONF_H */
