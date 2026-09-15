#include "link.h"

#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  // ESP32 core 3.x reports the peer address through a wifi_tx_info_t struct
  #define LF_LINK_SEND_CB(cbName) void cbName(const wifi_tx_info_t* txInfo, esp_now_send_status_t status)
#else
  // ESP32 core 2.x passes the raw MAC address
  #define LF_LINK_SEND_CB(cbName) void cbName(const uint8_t* macAddr, esp_now_send_status_t status)
#endif

static uint8_t         s_receiverMac[6] = RECEIVER_MAC;
static volatile uint8_t s_lastStatus    = 0;   // 0 unknown, 1 ok, 2 failed
static volatile uint32_t s_sent         = 0;
static volatile uint32_t s_failed       = 0;

LF_LINK_SEND_CB(lfOnDataSent) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  (void)txInfo;
#else
  (void)macAddr;
#endif
  if (status == ESP_NOW_SEND_SUCCESS) {
    s_lastStatus = 1;
    s_sent++;
  } else {
    s_lastStatus = 2;
    s_failed++;
  }
}

EspNowLink::EspNowLink() {
  _started           = false;
  _warnedNoReceiver  = false;
}

bool EspNowLink::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();                       // ESP-NOW does not need a connection

  // Both boards have to be on the same channel. A station that never connects
  // keeps the channel we set here.
  esp_err_t chErr = esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  if (chErr != ESP_OK) {
    Serial.printf("Could not set WiFi channel %u (%d)\n", (unsigned)ESPNOW_CHANNEL, (int)chErr);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init() failed");
    _started = false;
    return false;
  }
  _started = true;

  esp_now_register_send_cb(lfOnDataSent);

  if (!hasReceiver()) {
    Serial.println("No receiver MAC configured in config.h - packets are only logged.");
    return true;
  }

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, s_receiverMac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("esp_now_add_peer() failed");
    return false;
  }

  Serial.print("ESP-NOW peer: ");
  for (uint8_t i = 0; i < 6; i++) {
    Serial.printf("%02X%s", s_receiverMac[i], (i < 5) ? ":" : "\n");
  }
  return true;
}

bool EspNowLink::hasReceiver() const {
  for (uint8_t i = 0; i < 6; i++) {
    if (s_receiverMac[i] != 0x00) return true;
  }
  return false;
}

bool EspNowLink::sendPanel(const LightPanel& panel, uint8_t changedId) {
  LightCommandPacket pkt;
  pkt.protocol = LINK_PROTOCOL_ID;
  pkt.count    = panel.count();
  pkt.changed  = changedId;
  pkt.reserved = 0;
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    // Every mode is 0..2, so it fits in the low two bits.
    pkt.mode[i] = (i < panel.count()) ? ((uint8_t)panel.mode(i) & 0x03) : 0;
  }

  if (!_started || !hasReceiver()) {
    if (!_warnedNoReceiver) {
      _warnedNoReceiver = true;
      Serial.println("Packet not sent: put the receiver MAC into RECEIVER_MAC in config.h.");
    }
    return false;
  }

  esp_err_t result = esp_now_send(s_receiverMac, (uint8_t*)&pkt, sizeof(pkt));
  if (result != ESP_OK) {
    Serial.printf("esp_now_send() error %d\n", (int)result);
    return false;
  }
  return true;
}

uint8_t EspNowLink::lastPacketStatus() const {
  return s_lastStatus;
}

uint32_t EspNowLink::packetsSent() const {
  return s_sent;
}

uint32_t EspNowLink::packetsFailed() const {
  return s_failed;
}
