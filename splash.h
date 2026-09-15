/*
 * splash.h - boot frame handling (LF-RC.raw on the flash file system).
 *
 * Two things this module deliberately does differently from the usual example
 * sketches:
 *
 *  1. It never formats anything. "LittleFS.begin(true)" formats when the mount
 *     fails, which silently destroys an already uploaded frame - for example
 *     when the file was written with the older SPIFFS upload tool (both use the
 *     same "spiffs" partition but a different layout).
 *  2. When the frame cannot be shown it says so on the display, not only on the
 *     serial port, so a missing image is never mistaken for a code bug.
 */
#pragma once

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>

#include "config.h"

class SplashScreen {
 public:
  explicit SplashScreen(TFT_eSPI* tft);

  // LittleFS first, then SPIFFS, never a format. Returns true when a file
  // system is mounted.
  bool mountFileSystem();

  // Prints name and size of every file on the mounted file system.
  void listFileSystem();

  // Is SPLASH_FILE present and opened successfully?
  bool frameAvailable();

  // Shows SPLASH_FILE full screen and holds it for "holdMs". If the frame is
  // missing a placeholder with the reason is shown for the same time.
  void show(unsigned long holdMs);

  const char* fileSystemName() const { return _fsName; }

 private:
  fs::File openFrame(const char** nameOfTheFs);
  void drawFallback(const char* reason, unsigned long holdMs);
  void drawCentredLine(const char* text, int16_t centreY, uint16_t colour, uint8_t maxWidthPercent);

  TFT_eSPI*   _tft;
  fs::FS*     _fs;        // file system that is reported as the frame holder
  const char* _fsName;
  bool        _littleOk;  // mount results, also guard the open() calls below
  bool        _spiffsOk;
};
