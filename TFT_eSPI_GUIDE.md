## TFT_eSPI in this project (guide)

Everything the display needs lives in the **library**, not in the sketch: the
driver, the pins and the SPI clock are in a "User_Setup" file that TFT_eSPI
pulls in while compiling. This page lists every value that is in use here and
where it has to be changed when hardware changes.

### Where the settings live (add the folder as a workspace, then these paths are clickable)

| file | role |
|---|---|
| `~/Arduino/libraries/TFT_eSPI/User_Setup_Select.h` | says **which** setup file is active - exactly one `#include` may be uncommented |
| `~/Arduino/libraries/TFT_eSPI/User_Setups/Setup70g_ESP32_S3_ILI9341_LF_RC.h` | the **active** setup for this project (created for the S3 DevKitC + ILI9341) |
| `~/Arduino/libraries/TFT_eSPI/User_Setups/Setup42_ILI9341_ESP32.h` | the old setup (classic ESP32, MOSI 23) - now commented out |
| `~/Arduino/libraries/TFT_eSPI/Processors/TFT_eSPI_ESP32_S3.h` | S3 low level code; line ~83 `SPI_PORT` - **the one line that had to be patched** |
| `~/Arduino/libraries/TFT_eSPI/TFT_eSPI.h` | includes `User_Setup_Select.h` (line 69) and the processor file (line 97) |
| `crawlerlichtanlage.ino` | `#include <TFT_eSPI.h>` (line 33), `TFT_eSPI tft;` (line 42), prints the active pins before `tft.init()` |
| `config.h` | `SCREEN_ROTATION`, and the compile time checks that compare the sketch's own pins with the setup |

The library is **not** part of the git repository (it sits outside it in
`~/Arduino/libraries`), so changes there are invisible to `git diff` - all three
library edits are recorded in this file instead.

### How a setup file is selected

`TFT_eSPI.h` line 69 pulls in `User_Setup_Select.h`. Only one of its
`#include <User_Setups/...>` lines may be active; the rest must stay commented
with `//`. If two are active you get redefinition errors, if none is active the
library falls back to `User_Setup.h` in the library root (which does not exist
here) and the build fails with `#error` messages about missing defines.

Currently active (added at the top of the ESP32 section of
`User_Setup_Select.h`):

```c
#include <User_Setups/Setup70g_ESP32_S3_ILI9341_LF_RC.h> // <<<<< LF-RC: ESP32-S3 DevKitC + ILI9341
```

and the previously active line was commented out:

```c
//#include <User_Setups/Setup42_ILI9341_ESP32.h>           // Setup file for ESP32 and SPI ILI9341 240x320
```

### The active pin map (Setup70g_ESP32_S3_ILI9341_LF_RC.h)

| define | GPIO | note |
|---|---|---|
| `TFT_CS` | 10 | display chip select |
| `TFT_MOSI` | 11 | display data out (SDI) |
| `TFT_SCLK` | 12 | SPI clock |
| `TFT_MISO` | 13 | display data in (SDO), only needed for reads |
| `TFT_DC` | 4 | data/command |
| `TFT_RST` | 5 | reset (`-1` if the module resets itself) |
| `TOUCH_CS` | 7 | XPT2046 touch chip select - **mandatory**, without it `getTouch()`/`calibrateTouch()` cannot work |

| define | value | note |
|---|---|---|
| `ILI9341_DRIVER` | - | 320x240 panel |
| `SPI_FREQUENCY` | 40 MHz | ILI9341 maximum |
| `SPI_READ_FREQUENCY` | 16 MHz | clock while reading the TFT |
| `SPI_TOUCH_FREQUENCY` | 2.5 MHz | the XPT2046 is slow |
| `LOAD_GLCD`, `FONT2/4/6/7/8`, `GFXFF`, `SMOOTH_FONT` | - | fonts used by the UI (`ui.cpp` measures text with font 2/4) |
| `USE_HSPI_PORT` | not defined | SPI2 = the Arduino core's FSPI/VSPI is used; `USE_HSPI_PORT` would switch to SPI3 = HSPI |

`SCREEN_ROTATION 1` in `config.h` (landscape, 320x240). A changed rotation
invalidates the stored touch calibration on purpose, so the arrows appear again.

### Sketch pins that must not collide (config.h)

The sketch itself uses two pins, and `config.h` compares them with the setup at
compile time (`#error` when they collide):

| define | GPIO | note |
|---|---|---|
| `SEILWINDE_INPUT_1` | 6 | was 5 - RST took GPIO 5 |
| `SEILWINDE_INPUT_2` | 8 | was 6 |

GPIO 19 and 20 are deliberately left free: they are the ESP32-S3 native USB
D-/D+ lines. The pins of a classic ESP32 setup, GPIO 22...25, do not exist on
the S3 at all (S3 = 0..21 and 26..48); `config.h` therefore emits a compile time
message if a TFT pin lands in 22...25.

### The S3 library patch (why it was needed)

`Processors/TFT_eSPI_ESP32_S3.h`, TFT_eSPI 2.5.43 said:

```c
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define SPI_PORT FSPI
```

esp32 core 3.x defines `FSPI` as the bus **index** 0, but the ESP-IDF register
macros want the GPSPI **number** (`soc/soc.h`: `REG_SPI_BASE(i)` is `0` for
`i < 2`). `SPI_USER_REG(0)` therefore collapsed to the constant `0x10` and the
first `tft.init()` wrote `SPI_USR_MOSI` (`0x08000000`) to address `0x10`:

```
Guru Meditation Error: Core 1 panic'ed (StoreProhibited)
EXCVADDR: 0x00000010   A9: 0x08000000
```

The fix (identical to TFT_eSPI master) is now in the file:

```c
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    #define SPI_PORT 2   // GPSPI2 - the bus the core's FSPI/SPI object drives
```

Verified in the built binary: all SPI register accesses resolve to
`60024010 <GPSPI2+0x10>` instead of `0x10`. A TFT_eSPI **upgrade that keeps
2.5.43 or older would have to be patched again**, master already contains it.

### Rules for the next display

1. Pick or write a setup that matches the **new** pins and driver, select that
   one line in `User_Setup_Select.h` (and comment the old one out).
2. Keep `TOUCH_CS` pointing at the touch chip select, otherwise the
   calibration at boot cannot run.
3. Set `target_size` in `img_to_565.py` to the new panel size, re-run it and
   upload the `data/` folder again, so `/LF-RC.raw` matches the resolution.
4. Change `SCREEN_ROTATION` in `config.h` if the panel hangs the other way
   round (1 or 3 = landscape).
5. Re-check `SEILWINDE_INPUT_1/2` against the new setup - `config.h` refuses to
   compile when they collide.
6. `ui.cpp`, `lights.cpp`, `link.cpp` and `crawlerlichtanlage.ino` never hard
   code a pixel value, they only ask `tft.width()`/`tft.height()`, so they need
   no change for a different size.
