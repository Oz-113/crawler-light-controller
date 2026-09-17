/*
 * lights.h - the light model.
 *
 * The panel is a std::vector of Light entries. A Light only knows which lamp it
 * is (id, 0 based) and in which of the three 2 bit modes it currently is.
 */
#pragma once

#include <Arduino.h>
#include <vector>

// Three states, all of them fit into 2 bits of the ESP-NOW packet.
enum LightMode : uint8_t {
  LIGHT_OFF  = 0,   // 0b00
  LIGHT_HALF = 1,   // 0b01
  LIGHT_FULL = 2    // 0b10
};
enum LightModeBIN : uint8_t {
  LIGHT_BIN_OFF  = 0,   // 0b00
  LIGHT_BIN_FULL = 1    // 0b10
};

static const uint8_t LIGHT_MODE_COUNT = 3;
static const uint8_t LIGHT_MODE_BIN_COUNT = 2;


struct Light {
  uint8_t   id;     // 0 based, the screen shows id + 1
  LightMode mode;
};

class LightPanel {
 public:
  LightPanel();

  // Creates "count" lights, all off. Call once in setup().
  void begin(uint8_t count);

  uint8_t       count() const;
  const Light&  light(uint8_t id) const;
  LightMode     mode(uint8_t id) const;

  bool setMode(uint8_t id, LightMode mode);

  // off -> half -> full -> off, returns the new mode.
  LightMode cycle(uint8_t id);

  void setAll(LightMode mode);

 private:
  std::vector<Light> _lights;
};
