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

## Online flashing website

This repository now includes a minimal web flasher page at `webflash/index.html`.

It offers two flashing modes:

- **Preserve data update**: writes `bootloader.bin`, `partitions.bin`, and `firmware.bin` from the `latest` release only, leaving the LittleFS data partition untouched
- **Full restore**: also writes `littlefs.bin` from the `latest` release to `0x310000`, replacing any existing on-device local data

Setup steps:

1. Make sure GitHub Actions has published the firmware artifacts to the repository's `latest` release
2. Publish the `webflash/` directory as-is:
   - `webflash/index.html`
   - `webflash/manifest-preserve.json`
   - `webflash/manifest-full.json`
3. Host that directory on any HTTPS static hosting service such as GitHub Pages, Netlify, or Vercel

If you want Netlify to deploy directly from this repository, a root-level `netlify.toml` is included:

- Netlify can publish the `webflash/` directory directly
- The page loads firmware binaries from the repository's `latest` release assets
- No firmware build step is required inside Netlify
- The page also offers an optional `ghproxy` mirror toggle for faster firmware downloads in mainland China
- The page keeps a simple log area, shows the firmware asset build date from release asset timestamps, and can back up either the on-device LittleFS partition or the full 4MB flash as a downloadable `.bin` file

Notes:

- Use Chrome or Edge because the site depends on Web Serial
- The preserve-data mode is only safe when the partition layout has not changed
- If GitHub's direct downloads are slow in your region, enable the `ghproxy` mirror toggle in the page
- `ghproxy` only proxies the firmware download URLs; release metadata checks still use the GitHub API, and the page auto-switches to `ghproxy` download links if GitHub API metadata reads fail (for example, `403`)
- The page checks the `latest` release assets on load; if `littlefs.bin` is missing it automatically disables the full-restore option
- If the base firmware assets (`bootloader.bin`, `partitions.bin`, `firmware.bin`) are missing, the page blocks flashing entirely
- If the browser cannot reach the GitHub API metadata endpoint, the page falls back to preserve-data mode only
- The page shows the build date from the firmware asset upload/update time because the reusable `latest` release record date may be older than the current binaries
- Device backup supports either the LittleFS data partition or a full raw 4MB flash dump
- The current board configuration is classic `ESP32`, not an `ESP32-S3` OPI PSRAM target

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
