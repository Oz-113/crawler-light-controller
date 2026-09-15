# LF-RC wireless light controller (sender)

ESP32 + TFT_eSPI touch panel that shows a 3x4 grid of light keys, cycles each
light through **off / half bright / full bright** (values 0, 1, 2 - two bits) and
sends the new state to a second ESP32 over **ESP-NOW**.

## Boot behaviour

1. `/LF-RC.raw` (already in LittleFS) is streamed to the screen **full screen for 4 s**.
2. The touch calibration is restored from flash (NVS). On the very first start -
   or after a rotation change - the four calibration arrows are shown once and
   the result is written permanently to flash.
3. The screen is divided into `GRID_COLS x GRID_ROWS` (3x4 = 12) keys, every key
   carries its number in the middle and is **filled** according to its mode:

   | mode | value | colour |
   |------|-------|--------|
   | off | 0 (`0b00`) | dark grey |
   | half bright | 1 (`0b01`) | orange |
   | full bright | 2 (`0b10`) | yellow |

## Touch gestures

| gesture | effect |
|---------|--------|
| tap a key | cycle off -> half -> full -> off, repaint that key, send the packet |
| hold a key >= 3 s | run the touch calibration again (saved to flash) |

## Files

| file | content |
|------|---------|
| `crawlerlichtanlage_lucas.ino` | boot sequence (splash, calibration, grid) and the touch state machine |
| `config.h` | **all tunables**: rotation, grid size, timings, colours, receiver MAC, protocol id |
| `lights.h/.cpp` | `LightMode` (0/1/2) and `LightPanel` = `std::vector<Light>` with mode cycling |
| `ui.h/.cpp` | resolution independent 3x4 key grid, per key font fitting, partial repaint |
| `link.h/.cpp` | ESP-NOW sender, packet layout, send statistics |
| `touchcal.h/.cpp` | `Preferences` (NVS) backed XPT2046 calibration |
| `splash.h/.cpp` | mounts the frame file system (LittleFS → SPIFFS, never formats) and shows `/LF-RC.raw` |
| `img_to_565.py` | converts `source_frames/*` to RGB565 `.raw` files in `data/` |
| `data/LF-RC.raw` | 320x240x2 = 153.600 bytes intro frame |

## The intro frame does not show up?

The splash module is deliberately stubborn about this, read the serial monitor
(115200 baud) - it prints what happened:

| serial output | meaning / fix |
|---|---|
| `Boot frame /LF-RC.raw: 153600 bytes from LittleFS` | all good |
| `SPIFFS mounted - the frame was uploaded with the old SPIFFS tool.` | the file is fine, but it was written with the SPIFFS data upload tool; re-upload with the LittleFS tool |
| `LittleFS could not be mounted` + `SPIFFS could not be mounted either` | nothing valid is in the data partition, upload the `data` folder again |
| `Boot frame /LF-RC.raw was not found.` + `Files on LittleFS: ...` | the partition mounted, but the file is not there / has another name |

**Nothing is ever formatted by the sketch any more.** An earlier version used
`LittleFS.begin(true)`, and that `true` means "format when the mount fails" -
which deletes an uploaded frame written with a different file system layout.
If that already happened, just upload the `data` folder again:

* Arduino IDE 2 + "ESP32 LittleFS Data Upload" style tool: *Tools → ESP32
  LittleFS Data Upload*, or
* `arduino-cli upload -p COMx --fqbn esp32:esp32:esp32 --input-dir data` style
  upload of the `data` folder.

When the frame cannot be shown, the panel does not stay black: a border plus
`LF-RC`, the reason and `upload data folder, then reset` are drawn for the same
4 seconds, so a missing image can not be mistaken for broken code.

## Swapping the display later

The UI never hard codes a pixel value: every rectangle comes from
`tft.width()`/`tft.height()` and the number font is chosen per key by asking
TFT_eSPI how large a two digit label renders, so a different panel only needs

1. the matching TFT_eSPI setup in `libraries/TFT_eSPI/User_Setup_Select.h`
   (currently `User_Setups/Setup42_ILI9341_ESP32.h`, ILI9341 on VSPI:
   MOSI 23, SCLK 18, CS 15, DC 2, RST 4, MISO 19, TOUCH_CS 21),
2. `target_size` in `img_to_565.py` set to the new panel size plus
   re-running it (and uploading the `data/` folder again),
3. `SCREEN_ROTATION` in `config.h` if the new panel is mounted differently
   (1 or 3 = landscape). A changed rotation invalidates the stored calibration
   on purpose, so the arrows appear once again.

No change to `ui.cpp`, `lights.cpp`, `link.cpp` or the `.ino` is required.

## ESP-NOW receiver contract

`RECEIVER_MAC` in `config.h` is intentionally left blank (`0x00, ...`). Until it
is filled in, the controller still toggles and repaints locally and only logs
`no receiver configured` on the serial port (115200 baud).

The packet is the struct `LightCommandPacket` from `link.h`:

```c
#pragma pack(push, 1)   // the Arduino struct is byte packed already: 4 * uint8_t + 12 * uint8_t
struct LightCommandPacket {
  uint8_t protocol;   // 0xA5 == LINK_PROTOCOL_ID
  uint8_t count;      // number of valid mode[] entries
  uint8_t changed;    // light id that was touched, 0xFF = full refresh
  uint8_t reserved;   // 0
  uint8_t mode[12];   // 2 bit mode per light: 0 off, 1 half, 2 full
};
```

Receiver side: `esp_now_register_recv_cb`, check `protocol == 0xA5`, then use
`mode[]` (the whole panel state is always included, so a lost packet heals
itself with the next one). Both boards have to use the same Wi-Fi channel,
`ESPNOW_CHANNEL` (1 by default).

## Build / upload

* Board: `ESP32 Dev Module` (esp32 core 3.x).
* Flash the filesystem once so `/LF-RC.raw` exists (Arduino IDE 2 with the
  LittleFS data upload tool, or `arduino-cli upload ... --input-dir data`).
* The sketch also works without LittleFS: it just skips the intro frame.
* Serial monitor at 115200 baud prints the panel size, the calibration values,
  the local MAC and every light change.
