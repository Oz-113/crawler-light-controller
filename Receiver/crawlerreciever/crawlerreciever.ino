/*
 * crawlerreciever - LF-RC light receiver
 *
 * Receives the LightCommandPacket of the sender (crawlerlichtanlage) over
 * ESP-NOW and drives the light outputs, the winch H bridge and the servo.
 * config.h holds the channel, protocol id, sender MAC, the pin map and the
 * light count.
 *
 * Light 1 (id LIGHT_DIMMER_ID) is dimmable and uses LEDC PWM, so the "half"
 * mode really is ~50 %. All other lights are plain on / off. The winch is a
 * two-pin H bridge that is only ever driven with one input HIGH at a time.
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
  uint8_t seilwinde;  // 0 stop, 1 forward, 2 reverse
  uint8_t servoangle; // 0-180 degrees
  uint8_t reserved;   // 0
  uint8_t mode[LIGHT_COUNT];
};
// 6 header bytes + one mode byte per light, no padding (all members are uint8_t).
static_assert(sizeof(LightCommandPacket) == 6 + LIGHT_COUNT,
              "LightCommandPacket has to stay identical to the sender");

static const int lightGpio[LIGHT_COUNT] = {
  LIGHT_OUTPUT_1,  LIGHT_OUTPUT_2,  LIGHT_OUTPUT_3,  LIGHT_OUTPUT_4,
  LIGHT_OUTPUT_5,  LIGHT_OUTPUT_6,  LIGHT_OUTPUT_7,  LIGHT_OUTPUT_8,
  LIGHT_OUTPUT_9,  LIGHT_OUTPUT_10, LIGHT_OUTPUT_11, LIGHT_OUTPUT_12
};

static Servo   s_servo;
static uint8_t s_servoAngle = SERVO_GESCHLOSSEN;

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

// Explicit prototypes (don't rely on the Arduino auto-prototype generator).
void lightOutputBegin(uint8_t count);
void lightOutputSet(uint8_t id, uint8_t mode);
void winchSet(uint8_t dir);
void servoSet(uint8_t angle);
void lightOutputsOff();

void lightOutputBegin(uint8_t count) {
  (void)count;
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) {
    if (i == LIGHT_DIMMER_ID) {
      // Pin based LEDC API (ESP32 core 3.x) - it allocates a free channel itself
      // and does not clash with the timer/channel ESP32Servo uses internally.
      ledcAttach(lightGpio[i], LIGHT_PWM_FREQ, LIGHT_PWM_RESOLUTION);
    } else {
      pinMode(lightGpio[i], OUTPUT);
    }
    s_lightMode[i] = MODE_OFF;
    lightOutputSet(i, MODE_OFF);
  }

  // Winch H bridge: both inputs low = coasting / stop.
  pinMode(WINCH_OUTPUT_A, OUTPUT);
  pinMode(WINCH_OUTPUT_B, OUTPUT);
  digitalWrite(WINCH_OUTPUT_A, LOW);
  digitalWrite(WINCH_OUTPUT_B, LOW);

  s_servo.attach(SERVO_OUTPUT);
  s_servo.write(SERVO_GESCHLOSSEN);
  s_servoAngle = SERVO_GESCHLOSSEN;

  Serial.printf("lightOutputBegin(%u): %u lights + winch + servo\n",
                (unsigned)count, (unsigned)LIGHT_COUNT);
}

void lightOutputSet(uint8_t id, uint8_t mode) {
  if (id >= LIGHT_COUNT) return;
  if (mode > MODE_FULL) mode = MODE_OFF;
  s_lightMode[id] = mode;

  if (id == LIGHT_DIMMER_ID) {
    // dimmable output: 0 % / 50 % / 100 %
    uint32_t duty = 0;
    if (mode == MODE_HALF)      duty = LIGHT_PWM_MAX / 2;
    else if (mode == MODE_FULL) duty = LIGHT_PWM_MAX;
    ledcWrite(lightGpio[id], duty);
    return;
  }

  digitalWrite(lightGpio[id], (mode == MODE_OFF) ? LOW : HIGH);
}

// Winch H bridge: never drive both inputs high at the same time.
void winchSet(uint8_t dir) {
  switch (dir) {
    case WINCH_FORWARD:
      digitalWrite(WINCH_OUTPUT_A, HIGH);
      digitalWrite(WINCH_OUTPUT_B, LOW);
      break;
    case WINCH_REVERSE:
      digitalWrite(WINCH_OUTPUT_A, LOW);
      digitalWrite(WINCH_OUTPUT_B, HIGH);
      break;
    case WINCH_STOP:
    default:
      digitalWrite(WINCH_OUTPUT_A, LOW);
      digitalWrite(WINCH_OUTPUT_B, LOW);
      break;
  }
}

void servoSet(uint8_t angle) {
  if (angle > 180) angle = 180;
  s_servoAngle = angle;
  s_servo.write(angle);
}

void lightOutputsOff() {
  for (uint8_t i = 0; i < LIGHT_COUNT; i++) lightOutputSet(i, MODE_OFF);
  winchSet(WINCH_STOP);    // safety: never leave the winch running unattended
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

static uint8_t s_winch = WINCH_STOP;

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

    s_winch = pkt.seilwinde & 0x03;
    winchSet(s_winch);
    servoSet(pkt.servoangle);

    s_linkLost = false;

    if (changed < LIGHT_COUNT) {
      Serial.printf("rx %u: light %u -> mode %u\n",
                    (unsigned)receiver.packetsReceived(),
                    (unsigned)(changed + 1),
                    (unsigned)(pkt.mode[changed] & 0x03));
    } else {
      Serial.printf("rx %u: full refresh (winch %u, servo %u)\n",
                    (unsigned)receiver.packetsReceived(),
                    (unsigned)s_winch, (unsigned)pkt.servoangle);
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
