#include "splash.h"

SplashScreen::SplashScreen(TFT_eSPI* tft) {
  _tft      = tft;
  _fs       = nullptr;
  _fsName   = "none";
  _littleOk = false;
  _spiffsOk = false;
}

// ------------------------------------------------------------------ mounting --
bool SplashScreen::mountFileSystem() {
  _fs       = nullptr;
  _fsName   = "none";

  // 1. LittleFS, exactly as written by the LittleFS data upload tool. The mount
  //    is done without formatOnFail on purpose: formatting here would delete an
  //    already uploaded frame.
  _littleOk = LittleFS.begin(false);
  if (_littleOk) {
    _fs     = &LittleFS;
    _fsName = "LittleFS";
    Serial.println(F("LittleFS mounted (no format was needed)"));
    if (LittleFS.exists(SPLASH_FILE)) return true;
    Serial.printf("%s is not in LittleFS, trying SPIFFS as well\n", SPLASH_FILE);
  } else {
    Serial.println(F("LittleFS could not be mounted"));
  }

  // 2. SPIFFS, for frames that were uploaded with the older SPIFFS tool. Same
  //    flash partition, different file system layout.
  _spiffsOk = SPIFFS.begin(false);
  if (_spiffsOk) {
    Serial.println(F("SPIFFS mounted - the frame was uploaded with the old SPIFFS tool."));
    Serial.println(F("Everything works, but re-upload the data folder with the LittleFS tool."));
    if (_fs == nullptr) {
      _fs     = &SPIFFS;
      _fsName = "SPIFFS";
    }
    return true;
  }
  Serial.println(F("SPIFFS could not be mounted either"));

  if (_fs == nullptr) {
    Serial.println(F("No frame file system available. Nothing was formatted."));
    return false;
  }
  return true;                            // LittleFS is up, just without the frame
}

void SplashScreen::listFileSystem() {
  if (_fs == nullptr) {
    Serial.println(F("No file system to list"));
    return;
  }

  fs::File root = _fs->open("/");
  if (!root || !root.isDirectory()) {
    Serial.printf("Root of %s cannot be opened\n", _fsName);
    return;
  }

  Serial.printf("Files on %s:\n", _fsName);
  fs::File entry = root.openNextFile();
  while (entry) {
    Serial.printf("  %s  %u bytes\n", entry.name(), (unsigned)entry.size());
    entry = root.openNextFile();
  }
  root.close();
}

// ------------------------------------------------------------------- reading --
fs::File SplashScreen::openFrame(const char** nameOfTheFs) {
  // Prefer the file system that was mounted, then try the other one if it is
  // mounted too (a half migrated board can still find its frame).
  if (_fs != nullptr) {
    fs::File file = _fs->open(SPLASH_FILE, "r");
    if (file) {
      *nameOfTheFs = _fsName;
      return file;
    }
  }

  if (_littleOk && _fs != &LittleFS) {
    fs::File file = LittleFS.open(SPLASH_FILE, "r");
    if (file) {
      *nameOfTheFs = "LittleFS";
      return file;
    }
  }

  if (_spiffsOk && _fs != &SPIFFS) {
    fs::File file = SPIFFS.open(SPLASH_FILE, "r");
    if (file) {
      *nameOfTheFs = "SPIFFS";
      return file;
    }
  }

  *nameOfTheFs = nullptr;
  return fs::File();
}

bool SplashScreen::frameAvailable() {
  const char* fsName = nullptr;
  fs::File file = openFrame(&fsName);
  if (!file) return false;
  file.close();
  return true;
}

// ------------------------------------------------------------------- drawing --
void SplashScreen::show(unsigned long holdMs) {
  const uint32_t start = millis();

  const char* fsName = nullptr;
  fs::File frame = openFrame(&fsName);

  if (!frame) {
    Serial.printf("Boot frame %s was not found.\n", SPLASH_FILE);
    Serial.println(F("Upload the data folder to the ESP32 (LittleFS data upload) and restart."));
    drawFallback(_fs ? "frame missing" : "no file system", holdMs);
    return;
  }

  const int16_t w        = _tft->width();
  const int16_t h        = _tft->height();
  const size_t  rowBytes = (size_t)w * 2;
  const size_t  expected = rowBytes * (size_t)h;

  Serial.printf("Boot frame %s: %u bytes from %s (expected %u for %dx%d)\n",
                SPLASH_FILE, (unsigned)frame.size(), fsName,
                (unsigned)expected, w, h);

  size_t rowsPerChunk = SPLASH_BUFFER_BYTES / rowBytes;
  if (rowsPerChunk == 0) rowsPerChunk = 1;
  if (rowsPerChunk > (size_t)h) rowsPerChunk = (size_t)h;

  uint16_t* chunk = (uint16_t*)malloc(rowsPerChunk * rowBytes);
  if (chunk == NULL) {
    Serial.println(F("Not enough RAM for the intro frame buffer"));
    frame.close();
    drawFallback("out of RAM for frame", holdMs);
    return;
  }

  _tft->setSwapBytes(false);              // the file bytes are already in panel order

  int16_t y = 0;
  while (y < h) {
    size_t rows = rowsPerChunk;
    if ((size_t)(h - y) < rows) rows = (size_t)(h - y);

    size_t want = rows * rowBytes;
    if (frame.read((uint8_t*)chunk, want) != want) {
      Serial.printf("Frame ends after %d of %d lines - file is shorter than the screen\n",
                    (int)y, (int)h);
      break;
    }

    _tft->pushImage(0, y, w, (int32_t)rows, chunk);
    y += (int16_t)rows;
  }

  free(chunk);
  frame.close();

  uint32_t elapsed = millis() - start;
  if (elapsed < holdMs) delay(holdMs - elapsed);
  Serial.printf("Intro frame displayed (%u ms)\n", (unsigned)(millis() - start));
}

void SplashScreen::drawCentredLine(const char* text, int16_t centreY, uint16_t colour,
                                   uint8_t maxWidthPercent) {
  // Fonts 1, 2 and 4 are the ones that carry letters; pick the largest that
  // still fits the requested share of the screen width.
  static const uint8_t fonts[] = { 1, 2, 4 };
  const int16_t limit = (int16_t)((int32_t)_tft->width() * maxWidthPercent / 100);

  uint8_t best = fonts[0];
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t font  = fonts[i];
    int16_t fontH = _tft->fontHeight(font);
    int16_t fontW = _tft->textWidth(text, font);
    if (fontH <= 0 || fontW <= 0) continue;
    if (fontW <= limit) best = font; else break;
  }

  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(colour, COL_BACKGROUND);
  _tft->setTextSize(1);
  _tft->drawString(text, _tft->width() / 2, centreY, best);
  _tft->setTextDatum(TL_DATUM);
}

void SplashScreen::drawFallback(const char* reason, unsigned long holdMs) {
  const int16_t w = _tft->width();
  const int16_t h = _tft->height();

  _tft->fillScreen(COL_BACKGROUND);
  _tft->drawRect(3, 3, w - 6, h - 6, COL_GRID);

  drawCentredLine("LF-RC", h * 32 / 100, COL_FULL_FILL, 80);
  drawCentredLine(reason, h * 55 / 100, COL_HALF_FILL, 90);

  char line[64];
  if (_fs != nullptr) {
    snprintf(line, sizeof(line), "%s: no %s", _fsName, SPLASH_FILE);
    drawCentredLine(line, h * 70 / 100, COL_OFF_TEXT, 95);
  } else {
    drawCentredLine("LittleFS / SPIFFS did not mount", h * 70 / 100, COL_OFF_TEXT, 95);
  }
  drawCentredLine("upload data folder, then reset", h * 84 / 100, COL_OFF_TEXT, 95);

  Serial.printf("Showing the replacement intro screen for %u ms\n", (unsigned)holdMs);
  delay(holdMs);
}
