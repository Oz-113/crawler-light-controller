/*
 * crawlerreciever - LF-RC light receiver
 *
 * Receives the LightCommandPacket of the sender (crawlerlichtanlage) over
 * ESP-NOW and hands one mode per light to the light outputs from loop().
 * config.h holds channel, protocol id, sender MAC and the light count.
 */

#include "config.h"
#include <ESP32Servo.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

struct LightCommandPacket {
  uint8_t protocol;   // LINK_PROTOCOL_ID
  uint8_t count;      // number of valid mode[] entries
  uint8_t changed;    // light id that was touched, 0xFF = full refresh
  uint8_t seilwinde;   // 0 off 1 right 2 left
  uint8_t reserved;   // 0
  uint8_t mode[LIGHT_COUNT];
};
const int gpio[LIGHT_COUNT] = 
{
  LIGHT_OUTPUT_1, LIGHT_OUTPUT_2, LIGHT_OUTPUT_3, LIGHT_OUTPUT_4,
  LIGHT_OUTPUT_5, LIGHT_OUTPUT_6, LIGHT_OUTPUT_7, LIGHT_OUTPUT_8,
  LIGHT_OUTPUT_9, LIGHT_OUTPUT_10, LIGHT_OUTPUT_11, LIGHT_OUTPUT_12,
  
  SEILWINDE_OUTPUT_13,
  SEILWINDE_OUTPUT_14, 
  
  SERVO_OUTPUT_15
  
  
}

    // Servo control
    static Servo myServo;
    myServo.attach(gpio[id]);

static_assert(sizeof(LightCommandPacket) == 4 + LIGHT_COUNT,
              "LightCommandPacket has to stay identical to the sender");

static uint8_t s_senderMac[6] = SENDER_MAC;

static portMUX_TYPE       s_packetMux      = portMUX_INITIALIZER_UNLOCKED;
static LightCommandPacket s_pendingPacket;
static volatile bool      s_pendingReady   = false;
static volatile uint8_t   s_pendingChanged = 0xFF;
static volatile uint32_t  s_received       = 0;
static volatile uint32_t  s_dropped        = 0;
static volatile uint32_t  s_lastPacketMs   = 0;

static bool     s_linkLost     = false;
static uint32_t s_reportedDrops = 0;

class EspNowReceiver {
 public:
  EspNowReceiver();

  bool begin();
  bool hasSender() const;

  bool takePacket(LightCommandPacket* out, uint8_t* changedId);

  uint32_t packetsReceived() const;
  uint32_t packetsDropped() const;
  uint32_t lastPacketMs() const;

 private:
  bool _started;
};

// --------------------------------------------- light output seam (yours) -----

static uint8_t s_lightMode[LIGHT_COUNT];

void lightOutputBegin(uint8_t count) {
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    pinMode(gpio[i], OUTPUT);
    s_lightMode[i] = MODE_OFF;
    digitalWrite(gpio[i], MODE_OFF);
  }
    Serial.printf("lightOutputBegin(%u) is still the stub\n", (unsigned)count);
}

void lightOutputSet(uint8_t id, uint8_t mode) {
  if (id >= LIGHT_COUNT) return;
  s_lightMode[id] = mode;

  switch (id) {
    case 12 ... 13: // Seilwinde output
      digitalWrite(gpio[id], mode);
      digitalWrite(gpio[id + 1], !mode); // also set the second output for Seilwinde
      break;
    case 14: // Servo output
      if (mode == MODE_FULL) {
        myServo.write(SERVO_OFFEN); // Move servo to 90 degrees
      } else {
        myServo.write(SERVO_GESCHLOSSEN); // Move servo to 0 degrees
      }
      break;
    default:
      digitalWrite(gpio[id], mode);
      break;
    break;
  }

}

void lightOutputsOff() {
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) lightOutputSet(i, MODE_OFF);
}

// ------------------------------------------------------------- esp-now -------

static bool senderMacConfigured() {
  for (uint8_t i = 0; i < 6; i++) {
    if (s_senderMac[i] != 0x00) return true;
  }
  return false;
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* src = (info != nullptr) ? info->src_addr : nullptr;
#else
void onDataRecv(const uint8_t* src, const uint8_t* data, int len) {
#endif
  if (src == nullptr || data == nullptr || len != (int)sizeof(LightCommandPacket)) {
    s_dropped++;
    return;
  }
  if (senderMacConfigured() && memcmp(src, s_senderMac, 6) != 0) {
    s_dropped++;
    return;
  }

  LightCommandPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));

  if (pkt.protocol != LINK_PROTOCOL_ID || pkt.count == 0 || pkt.count > LIGHT_COUNT) {
    s_dropped++;
    return;
  }

  portENTER_CRITICAL(&s_packetMux);
  s_pendingPacket  = pkt;
  s_pendingChanged = pkt.changed;
  s_pendingReady   = true;
  portEXIT_CRITICAL(&s_packetMux);

  s_received++;
  s_lastPacketMs = millis();
}

// -------------------------------------------------------------- receiver -----

EspNowReceiver::EspNowReceiver() {
  _started = false;
}

bool EspNowReceiver::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);

  for (uint8_t attempt = 0; attempt < 5; attempt++) {
    if (esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE) == ESP_OK) break;
    delay(50);
  }

  if (esp_now_init() != ESP_OK) {
    Serial.println(F("esp_now_init() failed"));
    _started = false;
    return false;
  }
  _started = true;

  esp_now_register_recv_cb(onDataRecv);

  if (!senderMacConfigured()) {
    Serial.println(F("SENDER_MAC is empty - packets from every sender are accepted"));
    return true;
  }

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, s_senderMac, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  peer.ifidx   = WIFI_IF_STA;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println(F("esp_now_add_peer() failed"));
    return false;
  }
  return true;
}

bool EspNowReceiver::hasSender() const {
  return senderMacConfigured();
}

bool EspNowReceiver::takePacket(LightCommandPacket* out, uint8_t* changedId) {
  if (out == nullptr || !s_pendingReady) return false;

  portENTER_CRITICAL(&s_packetMux);
  if (!s_pendingReady) {
    portEXIT_CRITICAL(&s_packetMux);
    return false;
  }
  *out = s_pendingPacket;
  if (changedId != nullptr) *changedId = s_pendingChanged;
  s_pendingReady = false;
  portEXIT_CRITICAL(&s_packetMux);
  return true;
}

uint32_t EspNowReceiver::packetsReceived() const { return s_received; }
uint32_t EspNowReceiver::packetsDropped()  const { return s_dropped; }
uint32_t EspNowReceiver::lastPacketMs()    const { return s_lastPacketMs; }

static EspNowReceiver receiver;

// ---------------------------------------------------------------- setup ------

void setup() {
  Serial.begin(115200);
  delay(150);
  Serial.println();
  Serial.println(F("=== LF-RC light receiver ==="));

  lightOutputBegin(LIGHT_COUNT);

  if (!receiver.begin()) {
    Serial.println(F("ESP-NOW did not start - restarting in 5 s"));
    delay(5000);
    ESP.restart();
  }

  Serial.print(F("Local MAC:  "));
  Serial.println(WiFi.macAddress());

  Serial.print(F("Sender MAC: "));
  for (uint8_t i = 0; i < 6; i++) {
    Serial.printf("%02X%s", s_senderMac[i], (i < 5) ? ":" : "");
  }
  Serial.println(receiver.hasSender() ? "" : "  (empty - every sender accepted)");

  Serial.printf("Channel %u, protocol 0x%02X, %u lights\n",
                (unsigned)ESPNOW_CHANNEL, (unsigned)LINK_PROTOCOL_ID, (unsigned)LIGHT_COUNT);
  Serial.println(F("Waiting for packets..."));
}

// ----------------------------------------------------------------- loop ------

void loop() {
  LightCommandPacket pkt;
  uint8_t changed = 0xFF;

  if (receiver.takePacket(&pkt, &changed)) {
    uint8_t count = pkt.count;
    if (count > LIGHT_COUNT) count = LIGHT_COUNT;

    for (uint8_t i = 0; i < count; i++) {
      uint8_t mode = (uint8_t)(pkt.mode[i] & 0x03);
      if (mode > MODE_FULL) mode = MODE_OFF;
      lightOutputSet(i, mode);
    }

    s_linkLost = false;

    if (changed < LIGHT_COUNT) {
      Serial.printf("rx %u: light %u -> mode %u\n",
                    (unsigned)receiver.packetsReceived(),
                    (unsigned)(changed + 1),
                    (unsigned)(pkt.mode[changed] & 0x03));
    } else {
      Serial.printf("rx %u: full refresh\n", (unsigned)receiver.packetsReceived());
    }
  } else if (!s_linkLost && receiver.lastPacketMs() != 0 &&
             (millis() - receiver.lastPacketMs()) > LINK_TIMEOUT_MS) {
    s_linkLost = true;
    lightOutputsOff();
    Serial.println(F("no packets from the sender - outputs off"));
  }

  if (receiver.packetsDropped() != s_reportedDrops) {
    s_reportedDrops = receiver.packetsDropped();
    Serial.printf("dropped packets: %u\n", (unsigned)s_reportedDrops);
  }

  delay(2);
}
