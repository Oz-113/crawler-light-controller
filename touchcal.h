/*
 * touchcal.h - touch calibration that survives a power cycle.
 *
 * The five calibration words of the XPT2046 (plus the rotation they belong to)
 * are kept in the NVS flash through the Preferences library, so the four arrow
 * touches are only needed once per board / rotation.
 */
#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <TFT_eSPI.h>

#include "config.h"

class TouchCalibration {
 public:
  TouchCalibration();

  // Loads the stored calibration; when nothing (or nothing usable / data from
  // another rotation) is found, the on screen calibration is started and its
  // result is written to flash.
  void begin(TFT_eSPI& tft);

  // Interactive calibration + save + tft.setTouch(). Also used for the
  // "hold a key for 3 seconds" recalibration.
  void run(TFT_eSPI& tft);

  bool loadedFromFlash() const { return _loaded; }

 private:
  bool load(TFT_eSPI& tft);
  bool store();
  static bool plausible(const uint16_t* cal);

  uint16_t _cal[5];
  uint8_t  _rotation;
  bool     _loaded;
};
