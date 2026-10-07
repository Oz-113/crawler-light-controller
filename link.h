/*
 * link.h - ESP-NOW sender side of the light controller.
 *
 * Every touch is sent as one small packet that contains the touched light plus
 * the complete panel state, so a receiver that missed a packet can be corrected
 * by simply using the mode[] array of the next one.
 */
#pragma once

#include <Arduino.h>

#include "config.h"
#include "lights.h"

// The current winch / H-bridge command is owned by the sketch (loop()) and read
// here while a packet is built: 0 stop, 1 forward, 2 reverse.
extern uint8_t seilwindeState;

// Wire format (keep in sync with the receiver sketch, this struct is the contract):
//   byte 0: protocol   = LINK_PROTOCOL_ID
//   byte 1: count      = number of valid entries in mode[]
//   byte 2: changed    = light id that was touched, 0xFF = complete refresh
//   byte 3: seilwinde  = winch direction (0 stop, 1 forward, 2 reverse)
//   byte 4: servoangle = servo target in degrees (0..180)
//   byte 5: reserved   = 0
//   byte 6..6+LIGHT_COUNT-1: mode[i], 2 bit value per light (0 off, 1 half, 2 full)
// The receiver mirrors this layout and static_asserts 6 + LIGHT_COUNT bytes.
struct LightCommandPacket {
  uint8_t protocol;
  uint8_t count;
  uint8_t changed;
  uint8_t seilwinde;   // 0 off 1 right 2 left
  uint8_t servoangle;   // 0-180 degrees
  uint8_t reserved;
  uint8_t mode[LIGHT_COUNT];
};

class EspNowLink {
 public:
  EspNowLink();

  // WiFi station mode + esp_now_init() + (optional) peer registration.
  bool begin();

  // True when a receiver MAC address was configured in config.h.
  bool hasReceiver() const;

  // Changed LED state of every light; 0xFF in LightPanel terms, no packet
  // without a receiver is sent (it is only reported on the serial port).
  bool sendPanel(const LightPanel& panel, uint8_t changedId);

  // 0 = unknown, 1 = last packet acknowledged, 2 = last packet failed
  uint8_t lastPacketStatus() const;

  uint32_t packetsSent() const;
  uint32_t packetsFailed() const;

 private:
  bool _started;
  bool _warnedNoReceiver;
};
