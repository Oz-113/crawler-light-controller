/*
 * ui.h - modular, resolution independent key grid.
 *
 * Ui does not know anything about the panel it runs on: the rectangles of the
 * GRID_COLS x GRID_ROWS keys are cut out of tft.width() x tft.height() with
 * integer edge interpolation, so the whole grid always fills the screen exactly
 * and every number is centred inside its own key. Fonts are picked per key by
 * asking TFT_eSPI how big a two digit label renders, therefore swapping the
 * display only needs the image files to be regenerated.
 */
#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <vector>

#include "lights.h"

struct UiCell {
  uint8_t id;            // light id (0 based); the drawn number is id + 1
  int16_t x, y, w, h;    // rectangle in screen pixels
  int16_t cx, cy;        // centre of the rectangle
  uint8_t font;          // biggest stock font that still fits this key
};

class Ui {
 public:
  Ui();

  void begin(TFT_eSPI* tft, const LightPanel* panel);
  void layout();                          // recompute the rectangles (call after a display change)

  void drawAll();                         // background + every key
  void drawCell(uint8_t id);              // repaint just one key (no flicker)

  int8_t  cellAt(int16_t x, int16_t y) const;   // -1 when outside the grid
  uint8_t cellCount() const;

  int16_t width() const { return _w; }
  int16_t height() const { return _h; }

 private:
  uint8_t pickFont(int16_t w, int16_t h) const;
  static void coloursFor(LightMode mode, uint16_t* fill, uint16_t* text);

  TFT_eSPI*           _tft;
  const LightPanel*   _panel;
  std::vector<UiCell> _cells;
  int16_t             _w;
  int16_t             _h;
};
