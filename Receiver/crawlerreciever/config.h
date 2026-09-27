#pragma once

#define ESPNOW_CHANNEL         1              // both ESP32 must sit on the same channel
#define LINK_PROTOCOL_ID       0xA5           // receiver checks this first byte

#define LIGHT_COUNT            13             // 3 x 4 keys on the sender side
#define LINK_TIMEOUT_MS        10000UL        // no packet for this long -> outputs off

#define MODE_OFF               0
#define MODE_HALF              1
#define MODE_FULL              2

#define SERVO_OFFEN            90             // Servo open position
#define SERVO_GESCHLOSSEN      0              // Servo closed position

#define SENDER_MAC           { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#define LIGHT_OUTPUT_1         12             // GPIO pin for light 1
#define LIGHT_OUTPUT_2         13             // GPIO pin for light 2
#define LIGHT_OUTPUT_3         14             // GPIO pin for light 3
#define LIGHT_OUTPUT_4         15             // GPIO pin for light 4
#define LIGHT_OUTPUT_5         16             // GPIO pin for light 5
#define LIGHT_OUTPUT_6         17             // GPIO pin for light 6
#define LIGHT_OUTPUT_7         18             // GPIO pin for light 7
#define LIGHT_OUTPUT_8         19             // GPIO pin for light 8
#define LIGHT_OUTPUT_9         20             // GPIO pin for light 9
#define LIGHT_OUTPUT_10        21             // GPIO pin for light 10
#define LIGHT_OUTPUT_11        22             // GPIO pin for light 11
#define LIGHT_OUTPUT_12        23             // GPIO pin for light 12

#define SEILWINDE_OUTPUT_13    25             // GPIO pin for Seilwinde output 13
