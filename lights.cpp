#include "lights.h"

LightPanel::LightPanel() {
  // empty until begin() is called
}

void LightPanel::begin(uint8_t count) {
  _lights.clear();
  _lights.reserve(count);
  for (uint8_t i = 0; i < count; i++) {
    Light l;
    l.id   = i;
    l.mode = LIGHT_OFF;
    _lights.push_back(l);
  }
}

uint8_t LightPanel::count() const {
  return (uint8_t)_lights.size();
}

const Light& LightPanel::light(uint8_t id) const {
  static const Light dummy = { 0, LIGHT_OFF };
  if (id >= _lights.size()) return dummy;
  return _lights[id];
}

LightMode LightPanel::mode(uint8_t id) const {
  return light(id).mode;
}

bool LightPanel::setMode(uint8_t id, LightMode mode) {
  if (id >= _lights.size()) return false;

  if(id==0){
    
  if (mode >= LIGHT_MODE_COUNT) return false;  
    }else{
  if (mode >= LIGHT_MODE_BIN_COUNT) return false;
    }
  if (_lights[id].mode == mode) return false;
  _lights[id].mode = mode;
  return true;
}

LightMode LightPanel::cycle(uint8_t id) {
  if (id >= _lights.size()) return LIGHT_OFF;
  uint8_t next = ((uint8_t)_lights[id].mode + 1) % LIGHT_MODE_COUNT;
  _lights[id].mode = (LightMode)next;
  return _lights[id].mode;
}

void LightPanel::setAll(LightMode mode) {
  if (mode >= LIGHT_MODE_COUNT) return;
  for (size_t i = 0; i < _lights.size(); i++) _lights[i].mode = mode;
}
