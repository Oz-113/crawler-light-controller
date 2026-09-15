#include "ui.h"

#include "config.h"

Ui::Ui() {
  _tft   = nullptr;
  _panel = nullptr;
  _w     = 0;
  _h     = 0;
}

void Ui::begin(TFT_eSPI* tft, const LightPanel* panel) {
  _tft   = tft;
  _panel = panel;
  layout();
}

void Ui::layout() {
  if (_tft == nullptr) return;

  _w = _tft->width();
  _h = _tft->height();

  _cells.clear();
  _cells.reserve(LIGHT_COUNT);

  for (uint8_t id = 0; id < LIGHT_COUNT; id++) {
    uint8_t col = id % GRID_COLS;
    uint8_t row = id / GRID_COLS;

    // Map the key edges (not the key size) so the leftover pixels are spread
    // evenly and the grid never leaves a gap at the right/bottom border.
    int16_t x0 = (int16_t)((int32_t)_w * col / GRID_COLS);
    int16_t y0 = (int16_t)((int32_t)_h * row / GRID_ROWS);
    int16_t x1 = (int16_t)((int32_t)_w * (col + 1) / GRID_COLS);
    int16_t y1 = (int16_t)((int32_t)_h * (row + 1) / GRID_ROWS);

    UiCell c;
    c.id   = id;
    c.x    = x0;
    c.y    = y0;
    c.w    = x1 - x0;
    c.h    = y1 - y0;
    c.cx   = c.x + c.w / 2;
    c.cy   = c.y + c.h / 2;
    c.font = pickFont(c.w, c.h);
    _cells.push_back(c);
  }
}

uint8_t Ui::pickFont(int16_t w, int16_t h) const {
  // Stock fonts in ascending size: 1 = 8 px, 2 = 16 px, 4 = 26 px,
  // 6 / 7 / 8 = 48..75 px, all of them contain the digits we need.
  static const uint8_t candidates[] = { 1, 2, 4, 6, 7, 8 };
  const uint8_t candidateCount = (uint8_t)(sizeof(candidates) / sizeof(candidates[0]));

  uint8_t best = candidates[0];
  for (uint8_t i = 0; i < candidateCount; i++) {
    uint8_t font     = candidates[i];
    int16_t fontH    = _tft->fontHeight(font);
    int16_t fontW    = _tft->textWidth("12", font);   // widest label is two digits
    if (fontH <= 0 || fontW <= 0) continue;
    if (fontH <= (h * 6) / 10 && fontW <= (w * 7) / 10) {
      best = font;
    } else {
      break;                                          // fonts only get bigger
    }
  }
  return best;
}

void Ui::drawAll() {
  if (_tft == nullptr) return;
  _tft->fillScreen(COL_BACKGROUND);
  for (size_t i = 0; i < _cells.size(); i++) drawCell(_cells[i].id);
}

void Ui::drawCell(uint8_t id) {
  if (_tft == nullptr || _panel == nullptr) return;
  if (id >= _cells.size()) return;

  const UiCell& c = _cells[id];

  uint16_t fill = COL_OFF_FILL;
  uint16_t text = COL_OFF_TEXT;
  coloursFor(_panel->mode(id), &fill, &text);

  char label[4];
  snprintf(label, sizeof(label), "%u", (unsigned)(c.id + 1));

  // The key is simply "filled" according to its mode, the border doubles as the
  // grid line, the number sits in the middle. No sprite needed.
  _tft->fillRect(c.x, c.y, c.w, c.h, fill);
  _tft->drawRect(c.x, c.y, c.w, c.h, COL_GRID);

  _tft->setTextDatum(MC_DATUM);
  _tft->setTextColor(text, fill);
  _tft->setTextSize(1);
  _tft->drawString(label, c.cx, c.cy, c.font);
  _tft->setTextDatum(TL_DATUM);        // leave the global text state predictable
}

int8_t Ui::cellAt(int16_t x, int16_t y) const {
  if (_cells.empty()) return -1;
  if (x < 0 || y < 0 || x >= _w || y >= _h) return -1;

  uint8_t col = (uint8_t)((int32_t)x * GRID_COLS / _w);
  uint8_t row = (uint8_t)((int32_t)y * GRID_ROWS / _h);
  if (col >= GRID_COLS) col = GRID_COLS - 1;
  if (row >= GRID_ROWS) row = GRID_ROWS - 1;

  int8_t id = (int8_t)(row * GRID_COLS + col);
  if (id >= (int8_t)_cells.size()) return -1;
  return id;
}

uint8_t Ui::cellCount() const {
  return (uint8_t)_cells.size();
}

void Ui::coloursFor(LightMode mode, uint16_t* fill, uint16_t* text) {
  switch (mode) {
    case LIGHT_HALF:
      *fill = COL_HALF_FILL;
      *text = COL_HALF_TEXT;
      break;
    case LIGHT_FULL:
      *fill = COL_FULL_FILL;
      *text = COL_FULL_TEXT;
      break;
    default:                            // LIGHT_OFF
      *fill = COL_OFF_FILL;
      *text = COL_OFF_TEXT;
      break;
  }
}
