/*
 * config.h - every tunable value of the LF-RC wireless light controller.
 *
 * Nothing in this sketch hard codes the panel size: the UI (ui.cpp) derives all
 * coordinates from tft.width()/tft.height(). When the TFT is swapped you only
 * have to
 *   1. pick the matching TFT_eSPI setup in User_Setup_Select.h (pins + driver),
 *   2. regenerate the intro frame with img_to_565.py for the new resolution,
 *   3. change SCREEN_ROTATION here if the panel hangs the other way round.
 * The 3x4 key grid follows along automatically and so do the label fonts.
 */
#pragma once

#include <Arduino.h>

// ------------------------------------------------------------------ display --
// 1 or 3 = landscape. The ILI9341 setup used here (User_Setups/Setup42_ILI9341_ESP32.h)
// is 240x320 in portrait, so landscape gives the 320x240 of /LF-RC.raw.
#define SCREEN_ROTATION        1

// ---------------------------------------------------------------- light grid --
#define GRID_COLS              3
#define GRID_ROWS              4
#define LIGHT_COUNT            (GRID_COLS * GRID_ROWS)   // 12 lights

// ------------------------------------------------------------ boot animation --
#define SPLASH_FILE            "/LF-RC.raw"   // lives in LittleFS (the data/ folder)
#define SPLASH_TIME_MS         4000UL         // keep the intro frame on screen this long
#define SPLASH_BUFFER_BYTES    8192           // heap buffer used to stream the frame

// -------------------------------------------------------------- touch / cal ----
#define TOUCH_THRESHOLD        600            // XPT2046 z (pressure) threshold
#define TOUCH_LONGPRESS_MS     1000UL         // hold longer than this -> touch calibration
#define TOUCH_SETTLE_MS        60             // ignore contacts shorter than this (debounce)
#define CAL_PREFS_NAMESPACE    "lfrc"         // NVS namespace for the calibration data
#define CAL_PREFS_KEY          "touchcal"
#define CAL_DATA_VERSION       1              // bump this to force a fresh calibration

// ------------------------------------------------------------------ esp-now ----
#define ESPNOW_CHANNEL         1              // both ESP32 must sit on the same channel
#define LINK_PROTOCOL_ID       0xA5           // receiver checks this first byte

// !!! FILL IN THE MAC ADDRESS OF THE RECEIVER ESP32 !!!
// While it stays all zero the link only logs what it *would* send; the UI and the
// local light states keep working, so the controller can be tested on its own.
#define RECEIVER_MAC           { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// ------------------------------------------------------------------ colours ----
// 16 bit RGB565 values. 0x0000 black, 0x8410 mid grey, ...
#define COL_BACKGROUND         0x0000
#define COL_GRID               0x8410

#define COL_OFF_FILL           0x1082         // mode 0 (0b00) off          - dark grey
#define COL_OFF_TEXT           0x7BEF
#define COL_HALF_FILL          0xFFFF         // mode 1 (0b01) half bright  - WHITE
#define COL_HALF_TEXT          0x0000
#define COL_FULL_FILL          0x07E0         // mode 2 (0b10) full bright  - GREEN
#define COL_FULL_TEXT          0x0000

#define COL_CAL_FG             0xF81F         // calibration arrows
#define COL_CAL_BG             0x0000
#define COL_CAL_TEXT           0xFFFF
