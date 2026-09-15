#include "touchcal.h"

// Layout of the NVS blob: version, rotation, then the five TFT_eSPI words.
struct CalBlob {
  uint8_t  version;
  uint8_t  rotation;
  uint16_t cal[5];
};

TouchCalibration::TouchCalibration() {
  for (uint8_t i = 0; i < 5; i++) _cal[i] = 0;
  _rotation = SCREEN_ROTATION;
  _loaded   = false;
}

bool TouchCalibration::plausible(const uint16_t* cal) {
  // The usable XPT2046 range is roughly 100..4000 and the two axes have to
  // differ by a healthy amount, otherwise the values are garbage.
  for (uint8_t i = 0; i < 4; i++) {
    if (cal[i] < 100 || cal[i] > 4000) return false;
  }
  if (abs((int)cal[0] - (int)cal[1]) < 500) return false;   // x0 / x1
  if (abs((int)cal[2] - (int)cal[3]) < 500) return false;   // y0 / y1
  return true;
}

bool TouchCalibration::load(TFT_eSPI& tft) {
  Preferences prefs;
  if (!prefs.begin(CAL_PREFS_NAMESPACE, true)) {          // read only
    Serial.println("No calibration in flash (preferences unavailable)");
    return false;
  }

  CalBlob blob;
  size_t  len = prefs.getBytesLength(CAL_PREFS_KEY);

  if (len != sizeof(CalBlob)) {
    prefs.end();
    Serial.println("No calibration in flash yet");
    return false;
  }

  if (prefs.getBytes(CAL_PREFS_KEY, &blob, sizeof(blob)) != sizeof(blob)) {
    prefs.end();
    Serial.println("Calibration data could not be read");
    return false;
  }
  prefs.end();

  if (blob.version != CAL_DATA_VERSION) {
    Serial.println("Calibration was made with an older layout, redoing it");
    return false;
  }
  if (blob.rotation != SCREEN_ROTATION) {
    Serial.printf("Calibration belongs to rotation %u, screen is %u now\n",
                  (unsigned)blob.rotation, (unsigned)SCREEN_ROTATION);
    return false;
  }

  if (!plausible(blob.cal)) {
    Serial.println("Stored calibration looks broken, redoing it");
    return false;
  }

  for (uint8_t i = 0; i < 5; i++) _cal[i] = blob.cal[i];
  tft.setTouch(_cal);

  Serial.printf("Touch calibration loaded: %u, %u, %u, %u, %u\n",
                (unsigned)_cal[0], (unsigned)_cal[1], (unsigned)_cal[2],
                (unsigned)_cal[3], (unsigned)_cal[4]);
  return true;
}

bool TouchCalibration::store() {
  Preferences prefs;
  if (!prefs.begin(CAL_PREFS_NAMESPACE, false)) {         // read/write
    Serial.println("Calibration cannot be saved (preferences unavailable)");
    return false;
  }

  CalBlob blob;
  blob.version  = CAL_DATA_VERSION;
  blob.rotation = SCREEN_ROTATION;
  for (uint8_t i = 0; i < 5; i++) blob.cal[i] = _cal[i];

  size_t written = prefs.putBytes(CAL_PREFS_KEY, &blob, sizeof(blob));
  prefs.end();

  if (written != sizeof(blob)) {
    Serial.println("Calibration could not be written to flash");
    return false;
  }
  Serial.println("Calibration stored in flash.");
  return true;
}

void TouchCalibration::run(TFT_eSPI& tft) {
  tft.fillScreen(COL_CAL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextFont(2);
  tft.setTextColor(COL_CAL_TEXT, COL_CAL_BG);
  tft.setCursor(6, 6);
  tft.println("Touch calibration");
  tft.setTextFont(1);
  tft.println();
  tft.println("Touch the arrows exactly");
  tft.println("on their tips.");
  Serial.println("Touch calibration started - touch the four arrows.");

  uint16_t cal[5];
  tft.calibrateTouch(cal, COL_CAL_FG, COL_CAL_BG, 15);

  for (uint8_t i = 0; i < 5; i++) _cal[i] = cal[i];
  _rotation = SCREEN_ROTATION;
  _loaded   = true;

  tft.setTouch(_cal);
  store();

  tft.fillScreen(COL_CAL_BG);
  tft.setTextFont(2);
  tft.setTextColor(COL_CAL_TEXT, COL_CAL_BG);
  tft.setCursor(6, 6);
  tft.println("Calibration done.");
  delay(700);

  Serial.printf("Calibration values: { %u, %u, %u, %u, %u }\n",
                (unsigned)_cal[0], (unsigned)_cal[1], (unsigned)_cal[2],
                (unsigned)_cal[3], (unsigned)_cal[4]);
}

void TouchCalibration::begin(TFT_eSPI& tft) {
  if (load(tft)) return;

  Serial.println("Running touch calibration (once).");
  run(tft);
}
