/*
 * crawlerlichtanlage - LF-RC wireless light controller (sender)
 *
 * Boot sequence
 *   1. The frame file system is mounted (LittleFS, or SPIFFS when the frame was
 *      uploaded with the older tool; nothing is ever formatted) and
 *      /LF-RC.raw is streamed full screen for 4 s. If the frame cannot be found
 *      a placeholder with the reason is shown for the same time.
 *   2. The touch calibration is restored from flash (NVS). Only the very first
 *      start (or a changed rotation) asks for the four calibration arrows.
 *   3. The screen is split into GRID_COLS x GRID_ROWS keys, one per light.
 *
 * Touch (one key per light, the number drawn on a key is its id + 1)
 *   field 1  (id 0)  : tap toggles full <-> off, holding >= DIMMER_HALF_MS dims
 *                      to 50 %, any press while at 50 % switches it off again
 *   field 2  (id 1)  : Taster - on while the finger is down, off on release
 *   fields 3-11      : Schalter - each tap toggles the light on / off
 *   field 12 (id 11) : Schalter for light 12, which also moves the servo
 *   hold a key 3 s   : repeat the touch calibration (saved to flash again)
 *
 * Winch (Seilwinde)
 *   The two physical inputs drive the H bridge on the receiver; the direction is
 *   sent on every change and a stop command is sent as soon as both are
 *   released. The whole panel is re-sent as a heartbeat so the receiver does not
 *   time out and switch the lights off while nothing is touched.
 *
 * Files: config.h, lights.*, ui.*, link.*, touchcal.*, splash.*  (see README.md)
 */

#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <TFT_eSPI.h>

#include "config.h"
#include "lights.h"
#include "ui.h"
#include "link.h"
#include "touchcal.h"
#include "splash.h"

TFT_eSPI tft = TFT_eSPI();

// Current winch / H-bridge command from the two physical inputs. link.cpp reads
// it through the extern declaration in link.h.
uint8_t seilwindeState = 0;   // 0 stop, 1 forward, 2 reverse

LightPanel       panel;
Ui               ui;
// Note: not "link" - that name is taken by the POSIX link() in <unistd.h>.
EspNowLink       lightLink;
TouchCalibration touchCal;
SplashScreen     splash(&tft);

// ------------------------------------------------------------ key behaviour ---
enum FieldKind : uint8_t {
  FIELD_DIMMER,     // field 1  (id 0)
  FIELD_MOMENTARY,  // field 2  (id 1) - Taster
  FIELD_SWITCH      // fields 3..12 (id 2..11) - Schalter
};

// ------------------------------------------------------------ touch states ---
static bool     s_touching  = false;
static uint32_t s_pressMs   = 0;
static int8_t   s_pressCell = -1;
// Set after a calibration so the finger that is still on the glass is swallowed
// instead of being turned into a light command.
static bool     s_ignoreUntilRelease = false;
static uint32_t s_lastSendMs = 0;

// ------------------------------------------------------------ prototypes -----
static void handleTouch();
static void processRelease(uint8_t id, uint32_t heldMs);
static void applyMode(uint8_t id, LightMode mode);
static FieldKind fieldKindOf(uint8_t id);
static uint8_t readSeilwinde();

// ---------------------------------------------------------------- setup ------
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println(F("=== LF-RC wireless light controller (sender) ==="));

  panel.begin(LIGHT_COUNT);

  pinMode(SEILWINDE_INPUT_1, INPUT_PULLUP);
  pinMode(SEILWINDE_INPUT_2, INPUT_PULLUP);

  // ---- display ----
  tft.init();
  tft.setRotation(SCREEN_ROTATION);
  tft.setSwapBytes(false);              // /LF-RC.raw is little endian RGB565
  tft.fillScreen(COL_BACKGROUND);
  Serial.printf("Panel: %d x %d, rotation %u\n",
                tft.width(), tft.height(), (unsigned)SCREEN_ROTATION);

  // ---- flash file system with the boot frame ----
  // LittleFS first, then SPIFFS, and never a format, so an already uploaded
  // frame (with either tool) is preserved. If the frame cannot be shown, the
  // splash module draws a readable placeholder instead of silently skipping it.
  splash.mountFileSystem();
  splash.listFileSystem();
  splash.show(SPLASH_TIME_MS);          // full screen intro frame, 4 seconds
  tft.fillScreen(COL_BACKGROUND);

  // ---- touch calibration (flash backed) ----
  touchCal.begin(tft);
  s_ignoreUntilRelease = true;          // don't let the calibration touch leak in

  // ---- the key grid ----
  ui.begin(&tft, &panel);
  if (ui.cellCount() != LIGHT_COUNT) {
    Serial.printf("Warning: %u keys laid out, expected %u\n",
                  (unsigned)ui.cellCount(), (unsigned)LIGHT_COUNT);
  }
  ui.drawAll();

  // ---- radio ----
  lightLink.begin();
  Serial.print(F("Local MAC: "));
  Serial.println(WiFi.macAddress());
  Serial.println(F("Ready - 1 dimmer, 2 taster, 3-11 switch, 12 switch + servo"));

  seilwindeState = readSeilwinde();
  s_lastSendMs = millis();
}

// ----------------------------------------------------------------- loop ------
void loop() {
  handleTouch();

  // The two physical inputs drive the H bridge: send right away when the
  // direction changes and go back to "stop" as soon as neither is pressed.
  uint8_t winch = readSeilwinde();
  if (winch != seilwindeState) {
    seilwindeState = winch;
    Serial.printf("Seilwinde -> %u\n", (unsigned)seilwindeState);
    lightLink.sendPanel(panel, 0xFF);
    s_lastSendMs = millis();
  }

  // Heartbeat: keep the receiver alive (its LINK_TIMEOUT_MS) while idle.
  if (millis() - s_lastSendMs >= LINK_HEARTBEAT_MS) {
    lightLink.sendPanel(panel, 0xFF);
    s_lastSendMs = millis();
  }

  delay(5);
}

// Read the two winch inputs. A depressed input is pulled to GND (INPUT_PULLUP),
// so LOW == pressed. Both pressed (or neither) is treated as a stop, which is
// the only safe state for an H bridge.
static uint8_t readSeilwinde() {
  bool in1 = digitalRead(SEILWINDE_INPUT_1) == LOW;
  bool in2 = digitalRead(SEILWINDE_INPUT_2) == LOW;
  if (in1 == in2) return 0;
  return in1 ? 1 : 2;
}

// --------------------------------------------------------- touch handling ----
static void handleTouch() {
  uint16_t x = 0;
  uint16_t y = 0;
  bool     down = (tft.getTouch(&x, &y, TOUCH_THRESHOLD) != 0);
  uint32_t now  = millis();

  if (down && !s_touching) {
    // finger down - remember which key and when
    s_touching  = true;
    s_pressMs   = now;
    s_pressCell = s_ignoreUntilRelease ? -1 : ui.cellAt((int16_t)x, (int16_t)y);

    // field 2 is a Taster: it switches on the moment the finger is down
    if (!s_ignoreUntilRelease && s_pressCell >= 0 &&
        fieldKindOf((uint8_t)s_pressCell) == FIELD_MOMENTARY) {
      applyMode((uint8_t)s_pressCell, LIGHT_FULL);
    }
  } else if (!down && s_touching) {
    // finger released - decide what the gesture meant
    s_touching = false;
    uint32_t held     = now - s_pressMs;
    int8_t   released = s_pressCell;
    s_pressCell       = -1;

    if (s_ignoreUntilRelease) {
      s_ignoreUntilRelease = false;     // leftover touch from the calibration
      return;
    }
    if (released < 0) return;

    // field 2 (Taster) always returns to its original (off) state on release
    if (fieldKindOf((uint8_t)released) == FIELD_MOMENTARY) {
      applyMode((uint8_t)released, LIGHT_OFF);
      return;
    }

    if (held < TOUCH_SETTLE_MS) return;  // debounce very short contacts
    processRelease((uint8_t)released, held);
  }
}

static FieldKind fieldKindOf(uint8_t id) {
  if (id == FIELD_DIMMER_ID)    return FIELD_DIMMER;
  if (id == FIELD_MOMENTARY_ID) return FIELD_MOMENTARY;
  return FIELD_SWITCH;
}

// Repaint one key and tell the receiver about the new panel state.
static void applyMode(uint8_t id, LightMode mode) {
  panel.setMode(id, mode);
  ui.drawCell(id);
  lightLink.sendPanel(panel, id);
  s_lastSendMs = millis();
}

static void processRelease(uint8_t id, uint32_t heldMs) {
  if (fieldKindOf(id) == FIELD_DIMMER) {
    // tap: off <-> full ; hold: -> half ; any press while half -> off
    LightMode cur = panel.mode(id);
    LightMode next;
    if (cur == LIGHT_HALF) {
      next = LIGHT_OFF;
    } else if (heldMs >= DIMMER_HALF_MS) {
      next = LIGHT_HALF;
    } else {
      next = (cur == LIGHT_OFF) ? LIGHT_FULL : LIGHT_OFF;
    }
    applyMode(id, next);
    return;
  }

  // FIELD_SWITCH (fields 3..12): a long hold recalibrates, a tap toggles.
  if (heldMs >= TOUCH_CALIB_HOLD_MS) {
    Serial.println(F("Long press - recalibrating the touch screen"));
    touchCal.run(tft);
    s_ignoreUntilRelease = true;
    tft.fillScreen(COL_BACKGROUND);
    ui.drawAll();
    s_lastSendMs = millis();
    return;
  }
  applyMode(id, (panel.mode(id) == LIGHT_OFF) ? LIGHT_FULL : LIGHT_OFF);
}
