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
 * Touch
 *   short tap on a key : off -> half bright -> full bright -> off, the new
 *                        state is repainted and sent over ESP-NOW
 *   hold a key 3 s     : repeat the touch calibration (saved to flash again)
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

LightPanel       panel;
Ui               ui;
// Note: not "link" - that name is taken by the POSIX link() in <unistd.h>.
EspNowLink       lightLink;
TouchCalibration touchCal;
SplashScreen     splash(&tft);

// ------------------------------------------------------------ touch states ---
static bool     s_touching  = false;
static uint32_t s_pressMs   = 0;
static int8_t   s_pressCell = -1;
// Set after a calibration so the finger that is still on the glass is swallowed
// instead of being turned into a light command.
static bool     s_ignoreUntilRelease = false;

// ------------------------------------------------------------ prototypes -----
static void handleTouch();
static void onLightTouched(uint8_t id);

// ---------------------------------------------------------------- setup ------
void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println(F("=== LF-RC wireless light controller (sender) ==="));

  panel.begin(LIGHT_COUNT);

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
  Serial.println(F("Ready - tap a key to cycle off / half / full"));
}

// ----------------------------------------------------------------- loop ------
void loop() {
  handleTouch();
  delay(5);
}

// -------------------------------------------------------- boot animation -----
// Moved into the SplashScreen module (splash.h/.cpp): mounting of the frame file
// system, the streaming of /LF-RC.raw and the replacement screen when the frame
// is missing.

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
  } else if (!down && s_touching) {
    // finger released - decide what the gesture meant
    s_touching        = false;
    uint32_t held     = now - s_pressMs;
    int8_t   released = s_pressCell;
    s_pressCell       = -1;

    if (s_ignoreUntilRelease) {
      s_ignoreUntilRelease = false;     // leftover touch from the calibration
      return;
    }

    if (held >= TOUCH_LONGPRESS_MS) {

      Serial.println("longpress");
    if(released==0){
        if(panel.mode(released) == 2){
      LightMode mode = panel.cycle(released);
      
      Serial.println("panelmode was 2");
      }else{
     LightMode mode = panel.cycle(released);
     mode = panel.cycle(released);
     
      Serial.println("panelmode was 0");
      }
      ui.drawCell(released);

  bool queued = lightLink.sendPanel(panel, released);
    
    }else{
      Serial.println(F("Long press - recalibrating the touch screen"));
      touchCal.run(tft);
      s_ignoreUntilRelease = true;
      tft.fillScreen(COL_BACKGROUND);
      ui.drawAll();
      }
    } 
    
    else if (held >= TOUCH_SETTLE_MS && released >= 0) {
      onLightTouched((uint8_t)released);
    
    }
  }
}

// A key was tapped: cycle its mode, repaint it and tell the other ESP32.
static void onLightTouched(uint8_t id) {
  if(id==0){
    if(panel.mode(id) == 1){
      LightMode mode = panel.cycle(id);
     mode = panel.cycle(id); 
      }else{
     LightMode mode = panel.cycle(id);
     
     
      }
      
    }else{
     LightMode mode = panel.cycle(id);
    }
  ui.drawCell(id);

  bool queued = lightLink.sendPanel(panel, id);
 /* Serial.printf("Light %u -> mode %u (%s)\n",
                (unsigned)(id + 1), (unsigned)mode,
                queued ? "packet queued" : "no receiver configured");
*/
}
