#ifndef CONFIG_H
#define CONFIG_H

// Display Configuration
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

// LVGL Display Rotation (landscape mode)
#define ORIENTATION LV_DISPLAY_ROTATION_0

// Touchscreen Configuration
#define XPT2046_MOSI 32  // T_DIN
#define XPT2046_MISO 39  // T_OUT
#define XPT2046_CLK 25   // T_CLK
#define XPT2046_CS 33    // T_CS

// SD Card Configuration
#define SD_CS 5
#define SD_SPI_SPEED 25000000
#define SD_ALLOW_FALLBACK_SPEEDS 0
#define SD_SCK 14
#define SD_MISO 12
#define SD_MOSI 13

// Backlight / status LED
#define TFT_BACKLIGHT_PIN 21
#define TFT_BACKLIGHT_ON_LEVEL HIGH
// Set to -1 to disable status LED support.
#define STATUS_LED_PIN -1
#define STATUS_LED_ON_LEVEL HIGH
// RGB status LED pins. Set all to -1 to disable RGB LED.
#ifndef RGB_LED_R
#define RGB_LED_R -1
#endif
#ifndef RGB_LED_G
#define RGB_LED_G -1
#endif
#ifndef RGB_LED_B
#define RGB_LED_B -1
#endif
#ifndef RGB_LED_ON_LEVEL
#define RGB_LED_ON_LEVEL HIGH
#endif

// Application Modes
enum AppMode {
    MODE_FILE_MANAGER,
    MODE_EDITOR,
    MODE_CONFIG
};

#endif
