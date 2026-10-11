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
#define LIGHT_COUNT            12   // one key per light (3 x 4 = 12 keys)

// --------------------------------------------------------- key behaviour ------
// The number drawn on a key is its id + 1, laid out left to right, top to bottom.
#define FIELD_DIMMER_ID        0    // field 1  : tap = full/off, long press = 50 %
#define FIELD_MOMENTARY_ID     1    // field 2  : Taster - on while held, off on release
#define FIELD_SERVO_ID         11   // field 12 : Schalter - toggles light 12 AND the servo

// Servo positions sent when field 12 toggles (keep in sync with the receiver's
// SERVO_OFFEN / SERVO_GESCHLOSSEN).
#define SERVO_ANGLE_OFF        0
#define SERVO_ANGLE_ON         90

// The sender re-sends the whole panel at least this often so the receiver does
// not hit LINK_TIMEOUT_MS and switch the lights off between touches.
#define LINK_HEARTBEAT_MS      500UL

// ------------------------------------------------------------ boot animation --
#define SPLASH_FILE            "/LF-RC.raw"   // lives in LittleFS (the data/ folder)
#define SPLASH_TIME_MS         4000UL         // keep the intro frame on screen this long
#define SPLASH_BUFFER_BYTES    8192           // heap buffer used to stream the frame

// -------------------------------------------------------------- touch / cal ----
#define TOUCH_THRESHOLD        600            // XPT2046 z (pressure) threshold
#define DIMMER_HALF_MS         800UL          // field 1: hold longer than this -> 50 %
#define TOUCH_CALIB_HOLD_MS    3000UL         // hold a switch key this long -> touch calibration
#define TOUCH_SETTLE_MS        60             // ignore contacts shorter than this (debounce)
#define CAL_PREFS_NAMESPACE    "lfrc"         // NVS namespace for the calibration data
#define CAL_PREFS_KEY          "touchcal"
#define CAL_DATA_VERSION       1              // bump this to force a fresh calibration

#define SEILWINDE_INPUT_1  5             // GPIO pin for Seilwinde input 1
#define SEILWINDE_INPUT_2  6             // GPIO pin for Seilwinde input 2

// ------------------------------------------------------------------ esp-now ----
#define ESPNOW_CHANNEL         1              // both ESP32 must sit on the same channel
#define LINK_PROTOCOL_ID       0xA5           // receiver checks this first byte

// !!! FILL IN THE MAC ADDRESS OF THE RECEIVER ESP32 !!!
// While it stays all zero the link only logs what it *would* send; the UI and the
// local light states keep working, so the controller can be tested on its own.
#define RECEIVER_MAC           { 0x44, 0xB1, 0x76, 0xCD, 0x60, 0xF0 }

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
