# Usage & Warning

Chinese version: `README.zh-CN.md`

## Usage

1. This project is built with `PlatformIO`. Install VS Code and the PlatformIO extension first.
2. After connecting your `ESP32-2432S028` board, run:
   - Build: `pio run -e esp32-2432s028r`
   - Flash: `pio run -e esp32-2432s028r -t upload`
   - Serial monitor: `pio device monitor -b 115200`
3. Before first use, upload `data/` to LittleFS if needed:
   - `pio run -e esp32-2432s028r -t uploadfs`

## Warning

- THIS PROJECT **ONLY SUPPORTS** `ESP32-2432S028/ESP32-2432S028R (CYD)` HARDWARE.
- DO NOT USE IT DIRECTLY ON BOARDS WITH DIFFERENT DISPLAY RESOLUTION, TOUCH CONTROLLER, OR PIN MAPPING, OR YOU MAY GET A BLACK SCREEN, BROKEN TOUCH INPUT, OR STORAGE ISSUES.

# CYDnote

CYDnote is a local text note and file manager for the `ESP32-2432S028R` (CYD) board, built with `LVGL + TFT_eSPI + XPT2046 + LittleFS + SdFat`.

## Features

- Single-column file manager (custom UI)
- Drive sidebar: `L:` LittleFS, `D:` SD card (optional)
- Breadcrumb navigation, back to parent, menu actions
- File ops: New (file/folder), Rename, Delete (normal/force recursive), Copy/Paste
- Text editor: filename title, save/back, Pinyin IME(9 & 26)
- Paste progress with ETA and cancel
- FS info (capacity/used)

## Pin Definitions

```ini
# Display (ILI9341)
TFT_MOSI = 13
TFT_SCLK = 14
TFT_CS   = 15
TFT_DC   = 2
TFT_RST  = -1
TFT_BL   = 21

# Touch (XPT2046)
XPT2046_MOSI = 32
XPT2046_MISO = 39
XPT2046_CLK  = 25
XPT2046_CS   = 33

# SD card
SD_CS   = 5
SD_SCK  = 14
SD_MISO = 12
SD_MOSI = 13
SD_Freq = 25MHz
```

## Font Partitions

Font source: `src/font.c` from `SourceHanSansCN-Regular`.

Conversion:

- Range: `0x20-0x7F, 0x3000-0x303F, 0x3040-0x30FF, 0x4E00-0x9FAF, 0x0400-0x04FF`
- Size: `14`
- Bpp: `2-bit-per-pixel`
- Compression: enabled

Partitions by range:

- CJK (main): `0x4E00-0x9FAF` (20912)
- Japanese Kana: `0x3040-0x30FF` (192)
- Cyrillic: `0x0400-0x04FF` (256, incl. extended)

## Filesystem

- Uses `LittleFS` (not SPIFFS)
- `data/` is used to build the LittleFS image
- `L:` maps to LittleFS, `D:` maps to SD (if present)
