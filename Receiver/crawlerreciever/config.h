#pragma once

#define ESPNOW_CHANNEL         1              // both ESP32 must sit on the same channel
#define LINK_PROTOCOL_ID       0xA5           // receiver checks this first byte

#define LIGHT_COUNT            12             // 3 x 4 keys on the sender side
#define LINK_TIMEOUT_MS        10000UL        // no packet for this long -> outputs off

// Mode values as they travel over the wire (must match LightPanel::LightMode)
#define MODE_OFF               0
#define MODE_HALF              1              // 50 % - only used by the dimmer output
#define MODE_FULL              2

// Which light output is dimmable (field 1 on the sender). It is driven with PWM
// so MODE_HALF really is 50 %. Every other light is a plain on / off output.
#define LIGHT_DIMMER_ID        0

// PWM used for the dimmable output
#define LIGHT_PWM_FREQ         1000           // Hz
#define LIGHT_PWM_RESOLUTION   8              // bits -> duty 0..255
#define LIGHT_PWM_MAX          ((1 << LIGHT_PWM_RESOLUTION) - 1)

#define SERVO_OFFEN            90             // Servo open position
#define SERVO_GESCHLOSSEN      0              // Servo closed position

#define SENDER_MAC           { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

#define LIGHT_OUTPUT_1         4             // GPIO pin for light 1
#define LIGHT_OUTPUT_2         5             // GPIO pin for light 2
#define LIGHT_OUTPUT_3         6             // GPIO pin for light 3
#define LIGHT_OUTPUT_4         7             // GPIO pin for light 4
#define LIGHT_OUTPUT_5         15             // GPIO pin for light 5
#define LIGHT_OUTPUT_6         16             // GPIO pin for light 6
#define LIGHT_OUTPUT_7         17             // GPIO pin for light 7
#define LIGHT_OUTPUT_8         18             // GPIO pin for light 8
#define LIGHT_OUTPUT_9         8             // GPIO pin for light 9
#define LIGHT_OUTPUT_10        3             // GPIO pin for light 10
#define LIGHT_OUTPUT_11        46             // GPIO pin for light 11
#define LIGHT_OUTPUT_12        9             // GPIO pin for light 12

// Seilwinde (winch) H bridge: A and B are never HIGH at the same time.
#define WINCH_OUTPUT_A         10            // GPIO pin for H-bridge input A (old output 13)
#define WINCH_OUTPUT_B         11            // GPIO pin for H-bridge input B (old output 14)

// Direction codes as they travel in pkt.seilwinde
#define WINCH_STOP             0
#define WINCH_FORWARD          1
#define WINCH_REVERSE          2

#define SERVO_OUTPUT           12            // GPIO pin for the servo (old output 15)
