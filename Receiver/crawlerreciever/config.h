#pragma once

#define ESPNOW_CHANNEL         1              // both ESP32 must sit on the same channel
#define LINK_PROTOCOL_ID       0xA5           // receiver checks this first byte

#define LIGHT_COUNT            12             // 3 x 4 keys on the sender side
#define LINK_TIMEOUT_MS        10000UL        // no packet for this long -> outputs off

#define MODE_OFF               0
#define MODE_HALF              1
#define MODE_FULL              2

#define SENDER_MAC           { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
