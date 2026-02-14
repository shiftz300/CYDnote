// Kafkar.com

#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <esp_heap_caps.h>

// include the installed LVGL- Light and Versatile Graphics Library - https://github.com/lvgl/lvgl
#include <lvgl.h>

// include the installed "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>

// include the installed the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "app_manager.h"
#include "ui/font_manager.h"

// Application manager instance
AppManager* app = nullptr;

// Create a instance of the TFT_eSPI class
TFT_eSPI tft = TFT_eSPI();

// Keep touch on a dedicated SPI host to avoid contention with TFT/SD bus
SPIClass touchscreenSPI = SPIClass(HSPI);
// Use polling mode (no IRQ attach) to avoid interrupt/bus contention on CYD.
XPT2046_Touchscreen touchscreen(XPT2046_CS);

// Display pipeline tuning (stable baseline):
// - Smaller partial chunks reduce per-flush blocking.
// - Optional double buffer improves overlap between render and flush.
#ifndef LVGL_DRAW_BUF_DIV
#define LVGL_DRAW_BUF_DIV 8
#endif
#ifndef LVGL_DOUBLE_BUF
#define LVGL_DOUBLE_BUF 1
#endif
#define DRAW_BUF_PIXELS (SCREEN_WIDTH * SCREEN_HEIGHT / LVGL_DRAW_BUF_DIV)
#define DRAW_BUF_SIZE (DRAW_BUF_PIXELS * (LV_COLOR_DEPTH / 8))

// Touchscreen coordinates: (x, y) and pressure (z)
int x, y, z;
static int last_touch_x = -1;
static int last_touch_y = -1;
static bool touch_has_last = false;

// Touch tuning (lower sensitivity)
static const int TOUCH_MIN_PRESSURE = 220;
static const int TOUCH_DEADZONE_PX = 3;

// DMA-capable LVGL draw buffers (allocated at runtime)
static uint8_t* draw_buf_1 = nullptr;
static uint8_t* draw_buf_2 = nullptr;

static void tft_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
  uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
  uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)px_map, w * h, true);
  tft.endWrite();
  lv_display_flush_ready(disp);
}


// Get the Touchscreen data
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  LV_UNUSED(indev);
  // Checks if Touchscreen was touched, and prints X, Y and Pressure (Z)
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();
    if (p.z < TOUCH_MIN_PRESSURE) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }

    // Calibrate Touchscreen points with map function to the correct width and height
    int mapped_x = map(p.x, 200, 3700, 0, SCREEN_WIDTH - 1);
    int mapped_y = map(p.y, 240, 3800, 0, SCREEN_HEIGHT - 1);
    mapped_x = constrain(mapped_x, 0, SCREEN_WIDTH - 1);
    mapped_y = constrain(mapped_y, 0, SCREEN_HEIGHT - 1);

    // Simple low-pass filter + deadzone to reduce sensitivity and jitter
    if (!touch_has_last) {
      last_touch_x = mapped_x;
      last_touch_y = mapped_y;
      touch_has_last = true;
    } else {
      int filtered_x = (last_touch_x * 7 + mapped_x * 3) / 10;
      int filtered_y = (last_touch_y * 7 + mapped_y * 3) / 10;
      if (abs(filtered_x - last_touch_x) < TOUCH_DEADZONE_PX) filtered_x = last_touch_x;
      if (abs(filtered_y - last_touch_y) < TOUCH_DEADZONE_PX) filtered_y = last_touch_y;
      last_touch_x = filtered_x;
      last_touch_y = filtered_y;
    }
    x = last_touch_x;
    y = last_touch_y;
    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;

    // Set the coordinates
    data->point.x = x;
    data->point.y = y;
  }
  else {
    touch_has_last = false;
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);
  
  // Start LVGL
  lv_init();

  pinMode(21, OUTPUT);
  digitalWrite(21, HIGH);
  
  // Allocate LVGL draw buffer(s) from DMA-capable internal RAM.
  draw_buf_1 = (uint8_t*)heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
  if (!draw_buf_1) {
    Serial.println("[ERROR] Failed to allocate LVGL draw_buf_1");
    while (1) { delay(1000); }
  }
  if (LVGL_DOUBLE_BUF) {
    draw_buf_2 = (uint8_t*)heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!draw_buf_2) {
      Serial.println("[WARN] draw_buf_2 alloc failed, fallback to single buffer");
    }
  }

  // Initialize TFT and create LVGL display object.
  tft.begin();
  tft.setRotation(0);
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, tft_flush_cb);
  lv_display_set_buffers(disp, draw_buf_1, draw_buf_2, DRAW_BUF_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_rotation(disp, ORIENTATION);

  // Start touch SPI after display is ready to reduce startup bus contention.
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(0);
    
  // Initialize an LVGL input device object (Touchscreen)
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);

  // Set the callback function to read Touchscreen input
  lv_indev_set_read_cb(indev, touchscreen_read);

  // Initialize LittleFS for internal flash storage
  delay(100);  // Give SPI time to settle
  if (!esp_littlefs_mounted("spiffs")) {
    if (!LittleFS.begin(true)) {  // true = format if mount fails
      Serial.println("[ERROR] LittleFS mount failed!");
    } else {
      Serial.println("[LittleFS] mounted");
    }
  } else {
    Serial.println("[LittleFS] already mounted");
  }
  FontManager::init();

  // Initialize application manager instead of demo GUI
  app = AppManager::getInstance();
  app->init();
  
  Serial.println("CYDnote initialized");
}

void loop() {
  static uint32_t last_ms = 0;
  uint32_t now_ms = millis();
  if (last_ms == 0) last_ms = now_ms;
  lv_tick_inc(now_ms - last_ms);
  last_ms = now_ms;

  lv_timer_handler();  // let the GUI do its work
  if (app) app->update();  // update app state and handle menu actions
  delay(0);              // keep yielding without adding an extra 1ms frame stall
}

