// Kafkar.com

#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <esp_littlefs.h>
#include <esp_heap_caps.h>
#include <math.h>

// include the installed LVGL- Light and Versatile Graphics Library - https://github.com/lvgl/lvgl
#include <lvgl.h>

// include the installed "TFT_eSPI" library by Bodmer to interface with the TFT Display - https://github.com/Bodmer/TFT_eSPI
#include <TFT_eSPI.h>

// include the installed the "XPT2046_Touchscreen" library by Paul Stoffregen to use the Touchscreen - https://github.com/PaulStoffregen/XPT2046_Touchscreen
#include <XPT2046_Touchscreen.h>

#include "config.h"
#include "app_manager.h"
#include "ui/font_manager.h"
#include "utils/sd_helper.h"

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
#define LVGL_DRAW_BUF_DIV 12
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
static bool fs_mount_ok = false;
static bool lv_sd_fs_registered = false;

#if defined(CDS)
#define AMBIENT_LIGHT_PIN CDS
#else
#define AMBIENT_LIGHT_PIN -1
#endif

// Auto backlight (CDS/LDR -> TFT backlight PWM)
static const uint8_t BL_MIN_PCT = 25;                // keep minimum readable brightness
static const uint8_t BL_MAX_PCT = 85;                // keep headroom so change is visible
static const uint32_t CDS_SAMPLE_MS = 70;            // smoother response
static const float BL_SMOOTH_ALPHA = 0.22f;          // less jumpy
static const float BL_GAMMA = 1.8f;                  // bright-zone curve
static const bool CDS_INVERT = true;                 // light -> brighter
static const float BL_MAX_STEP_PCT = 2.0f;           // max brightness step per sample
static const float BL_DARK_ZONE = 0.35f;             // 0..35% ambient treated as dark zone
static const float BL_DARK_GAIN = 0.12f;             // dark zone uses only 12% of total range
static const float BL_ULTRA_DARK_ZONE = 0.10f;       // first 10%: extra compressed
static const float BL_ULTRA_DARK_GAIN = 0.02f;       // first 10% uses only 2% of range
static const float BL_DARK_HYSTERESIS_PCT = 1.0f;    // low-light anti-flicker deadband

static bool backlight_pwm_ready = false;
static bool backlight_pwm_alt_ready = false;
static float bl_smoothed_pct = (float)BL_MAX_PCT;
static uint32_t bl_last_sample_ms = 0;
static int cds_obs_min = 4095;
static int cds_obs_max = 0;

static inline void backlightWritePct(uint8_t pct) {
  if (pct < BL_MIN_PCT) pct = BL_MIN_PCT;
  if (pct > BL_MAX_PCT) pct = BL_MAX_PCT;
  uint8_t duty = (uint8_t)((uint32_t)pct * 255U / 100U);
  if (TFT_BACKLIGHT_ON_LEVEL != HIGH) duty = 255U - duty;
  analogWrite(TFT_BACKLIGHT_PIN, duty);
  if (backlight_pwm_alt_ready) analogWrite(TFT_BACKLIGHT_PIN_ALT, duty);
}

static void backlightInit() {
  pinMode(TFT_BACKLIGHT_PIN, OUTPUT);
  backlight_pwm_ready = true;
#if TFT_BACKLIGHT_PIN_ALT >= 0
  if (TFT_BACKLIGHT_PIN_ALT != TFT_BACKLIGHT_PIN) {
    pinMode(TFT_BACKLIGHT_PIN_ALT, OUTPUT);
    backlight_pwm_alt_ready = true;
  }
#endif
  backlightWritePct(BL_MAX_PCT);

#if AMBIENT_LIGHT_PIN >= 0
  pinMode(AMBIENT_LIGHT_PIN, INPUT);
  analogReadResolution(12);
  analogSetPinAttenuation(AMBIENT_LIGHT_PIN, ADC_11db);
#endif
}

static void backlightAutoUpdate() {
  if (!backlight_pwm_ready) return;
#if AMBIENT_LIGHT_PIN < 0
  return;
#else
  uint32_t now = millis();
  if (now - bl_last_sample_ms < CDS_SAMPLE_MS) return;
  bl_last_sample_ms = now;

  int raw = analogRead(AMBIENT_LIGHT_PIN);
  if (raw < 0) return;
  if (CDS_INVERT) raw = 4095 - raw;

  // Sliding observation window so brightness reacts to "current environment",
  // not old extreme values from minutes ago.
  if (raw < cds_obs_min) cds_obs_min = raw;
  else if (cds_obs_min < 4095) cds_obs_min++;

  if (raw > cds_obs_max) cds_obs_max = raw;
  else if (cds_obs_max > 0) cds_obs_max--;

  int span = cds_obs_max - cds_obs_min;
  float t = 0.0f;
  if (span >= 20) {
    t = (float)(raw - cds_obs_min) / (float)span;
  } else {
    // Not enough ambient variation yet; fallback to full-range rough mapping.
    t = (float)raw / 4095.0f;
  }
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;

  float span_pct = (float)(BL_MAX_PCT - BL_MIN_PCT);
  float target_pct = (float)BL_MIN_PCT;
  if (t <= BL_ULTRA_DARK_ZONE) {
    // First 10%: heavily compressed to avoid visible jumps in near-dark.
    float tu = t / BL_ULTRA_DARK_ZONE;         // 0..1
    float yu = tu * tu;                         // flatten near zero
    target_pct += yu * (span_pct * BL_ULTRA_DARK_GAIN);
  } else if (t <= BL_DARK_ZONE) {
    // Compress dark region: ambient changes have very small brightness impact.
    float td = (t - BL_ULTRA_DARK_ZONE) / (BL_DARK_ZONE - BL_ULTRA_DARK_ZONE); // 0..1
    float yd = td * td;
    target_pct += (span_pct * BL_ULTRA_DARK_GAIN) +
                  yd * (span_pct * (BL_DARK_GAIN - BL_ULTRA_DARK_GAIN));
  } else {
    // Above dark zone: use remaining range with smooth non-linear growth.
    float tb = (t - BL_DARK_ZONE) / (1.0f - BL_DARK_ZONE); // 0..1
    float yb = powf(tb, BL_GAMMA);
    target_pct += (span_pct * BL_DARK_GAIN) + yb * (span_pct * (1.0f - BL_DARK_GAIN));
  }

  // Low-light anti-flicker: ignore tiny brightness changes in dark zone.
  if (t <= BL_DARK_ZONE && fabsf(target_pct - bl_smoothed_pct) < BL_DARK_HYSTERESIS_PCT) {
    return;
  }
  float delta = (target_pct - bl_smoothed_pct) * BL_SMOOTH_ALPHA;
  if (delta > BL_MAX_STEP_PCT) delta = BL_MAX_STEP_PCT;
  if (delta < -BL_MAX_STEP_PCT) delta = -BL_MAX_STEP_PCT;
  bl_smoothed_pct += delta;
  int final_pct = (int)(bl_smoothed_pct + 0.5f);
  backlightWritePct((uint8_t)final_pct);
#endif
}

typedef struct {
  FsFile file;
} LvSdFsFile;

static String lv_sd_norm_path(const char* p) {
  if (!p || p[0] == '\0') return "/";
  String s = String(p);
  if (!s.startsWith("/")) s = "/" + s;
  return s;
}

static bool lv_sd_ready_cb(lv_fs_drv_t* drv) {
  LV_UNUSED(drv);
  StorageHelper* sh = StorageHelper::getInstance();
  return sh && sh->isInitialized();
}

static void* lv_sd_open_cb(lv_fs_drv_t* drv, const char* path, lv_fs_mode_t mode) {
  LV_UNUSED(drv);
  StorageHelper* sh = StorageHelper::getInstance();
  if (!sh || !sh->isInitialized()) return nullptr;

  oflag_t flags = O_RDONLY;
  if (mode == LV_FS_MODE_WR) flags = O_WRONLY | O_CREAT | O_TRUNC;
  else if (mode == (LV_FS_MODE_RD | LV_FS_MODE_WR)) flags = O_RDWR | O_CREAT;

  LvSdFsFile* f = new LvSdFsFile();
  String p = lv_sd_norm_path(path);
  f->file = sh->getFs().open(p.c_str(), flags);
  if (!f->file.isOpen()) {
    delete f;
    return nullptr;
  }
  return f;
}

static lv_fs_res_t lv_sd_close_cb(lv_fs_drv_t* drv, void* file_p) {
  LV_UNUSED(drv);
  LvSdFsFile* f = (LvSdFsFile*)file_p;
  if (!f) return LV_FS_RES_INV_PARAM;
  if (f->file.isOpen()) f->file.close();
  delete f;
  return LV_FS_RES_OK;
}

static lv_fs_res_t lv_sd_read_cb(lv_fs_drv_t* drv, void* file_p, void* buf, uint32_t btr, uint32_t* br) {
  LV_UNUSED(drv);
  LvSdFsFile* f = (LvSdFsFile*)file_p;
  if (!f || !br) return LV_FS_RES_INV_PARAM;
  int32_t n = f->file.read((uint8_t*)buf, btr);
  if (n < 0) {
    *br = 0;
    return LV_FS_RES_UNKNOWN;
  }
  *br = (uint32_t)n;
  return LV_FS_RES_OK;
}

static lv_fs_res_t lv_sd_write_cb(lv_fs_drv_t* drv, void* file_p, const void* buf, uint32_t btw, uint32_t* bw) {
  LV_UNUSED(drv);
  LvSdFsFile* f = (LvSdFsFile*)file_p;
  if (!f || !bw) return LV_FS_RES_INV_PARAM;
  int32_t n = f->file.write((const uint8_t*)buf, btw);
  if (n < 0) {
    *bw = 0;
    return LV_FS_RES_UNKNOWN;
  }
  *bw = (uint32_t)n;
  return LV_FS_RES_OK;
}

static lv_fs_res_t lv_sd_seek_cb(lv_fs_drv_t* drv, void* file_p, uint32_t pos, lv_fs_whence_t whence) {
  LV_UNUSED(drv);
  LvSdFsFile* f = (LvSdFsFile*)file_p;
  if (!f) return LV_FS_RES_INV_PARAM;
  bool ok = false;
  if (whence == LV_FS_SEEK_SET) ok = f->file.seekSet(pos);
  else if (whence == LV_FS_SEEK_CUR) ok = f->file.seekCur((int32_t)pos);
  else if (whence == LV_FS_SEEK_END) ok = f->file.seekEnd((int32_t)pos);
  return ok ? LV_FS_RES_OK : LV_FS_RES_UNKNOWN;
}

static lv_fs_res_t lv_sd_tell_cb(lv_fs_drv_t* drv, void* file_p, uint32_t* pos_p) {
  LV_UNUSED(drv);
  LvSdFsFile* f = (LvSdFsFile*)file_p;
  if (!f || !pos_p) return LV_FS_RES_INV_PARAM;
  *pos_p = (uint32_t)f->file.curPosition();
  return LV_FS_RES_OK;
}

static void register_lvgl_sd_fs_driver() {
  if (lv_sd_fs_registered) return;
  lv_fs_drv_t* drv = (lv_fs_drv_t*)lv_malloc(sizeof(lv_fs_drv_t));
  if (!drv) {
    Serial.println("[SD] lv_fs driver alloc failed");
    return;
  }
  lv_fs_drv_init(drv);
  drv->letter = 'D';
  drv->ready_cb = lv_sd_ready_cb;
  drv->open_cb = lv_sd_open_cb;
  drv->close_cb = lv_sd_close_cb;
  drv->read_cb = lv_sd_read_cb;
  drv->write_cb = lv_sd_write_cb;
  drv->seek_cb = lv_sd_seek_cb;
  drv->tell_cb = lv_sd_tell_cb;
  lv_fs_drv_register(drv);
  lv_sd_fs_registered = true;
}

enum StatusLedState {
  LED_BOOTING = 0,
  LED_READY,
  LED_BUSY,
  LED_ERROR
};

static StatusLedState led_state = LED_BOOTING;

static inline void statusLedWriteMono(bool on) {
#if STATUS_LED_PIN >= 0
  digitalWrite(STATUS_LED_PIN, on ? STATUS_LED_ON_LEVEL : (STATUS_LED_ON_LEVEL == HIGH ? LOW : HIGH));
#else
  (void)on;
#endif
}

static inline void statusLedWriteRgb(bool r, bool g, bool b) {
#if RGB_LED_R >= 0
  digitalWrite(RGB_LED_R, r ? RGB_LED_ON_LEVEL : (RGB_LED_ON_LEVEL == HIGH ? LOW : HIGH));
#endif
#if RGB_LED_G >= 0
  digitalWrite(RGB_LED_G, g ? RGB_LED_ON_LEVEL : (RGB_LED_ON_LEVEL == HIGH ? LOW : HIGH));
#endif
#if RGB_LED_B >= 0
  digitalWrite(RGB_LED_B, b ? RGB_LED_ON_LEVEL : (RGB_LED_ON_LEVEL == HIGH ? LOW : HIGH));
#endif
}

static void statusLedInit() {
#if STATUS_LED_PIN >= 0
  pinMode(STATUS_LED_PIN, OUTPUT);
  statusLedWriteMono(false);
#endif
#if RGB_LED_R >= 0
  pinMode(RGB_LED_R, OUTPUT);
#endif
#if RGB_LED_G >= 0
  pinMode(RGB_LED_G, OUTPUT);
#endif
#if RGB_LED_B >= 0
  pinMode(RGB_LED_B, OUTPUT);
#endif
#if (RGB_LED_R >= 0) || (RGB_LED_G >= 0) || (RGB_LED_B >= 0)
  statusLedWriteRgb(false, false, false);
#endif
}

static void statusLedSetState(StatusLedState s) {
  led_state = s;
}

static void statusLedUpdate() {
  uint32_t t = millis();
#if (RGB_LED_R >= 0) || (RGB_LED_G >= 0) || (RGB_LED_B >= 0)
  // RGB LED mapping:
  // booting: yellow blink, ready: green, busy: cyan blink, error: red fast blink
  switch (led_state) {
    case LED_BOOTING:
      statusLedWriteRgb(((t / 150) % 2) == 0, ((t / 150) % 2) == 0, false);
      break;
    case LED_READY:
      statusLedWriteRgb(false, true, false);
      break;
    case LED_BUSY:
      statusLedWriteRgb(false, ((t / 300) % 2) == 0, ((t / 300) % 2) == 0);
      break;
    case LED_ERROR:
      statusLedWriteRgb(((t / 80) % 2) == 0, false, false);
      break;
  }
#elif STATUS_LED_PIN >= 0
  switch (led_state) {
    case LED_BOOTING:
      statusLedWriteMono(((t / 150) % 2) == 0);
      break;
    case LED_READY:
      statusLedWriteMono(true);
      break;
    case LED_BUSY:
      statusLedWriteMono(((t / 300) % 2) == 0);
      break;
    case LED_ERROR:
      statusLedWriteMono(((t / 80) % 2) == 0);
      break;
  }
#endif
}

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

  backlightInit();
  statusLedInit();
  statusLedSetState(LED_BOOTING);
  
  // Initialize LittleFS for internal flash storage
  delay(100);  // Give SPI time to settle
  if (!esp_littlefs_mounted("spiffs")) {
    if (!LittleFS.begin(true)) {  // true = format if mount fails
      Serial.println("[ERROR] LittleFS mount failed!");
      fs_mount_ok = false;
    } else {
      Serial.println("[LittleFS] mounted");
      fs_mount_ok = true;
    }
  } else {
    Serial.println("[LittleFS] already mounted");
    fs_mount_ok = true;
  }
  FontManager::init();

  // Allocate LVGL draw buffer(s) from DMA-capable internal RAM.
  // Do this after font load to maximize contiguous heap for lv_binfont parser.
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

  // Initialize application manager instead of demo GUI
  app = AppManager::getInstance();
  app->init();
  register_lvgl_sd_fs_driver();
  statusLedSetState(fs_mount_ok ? LED_READY : LED_ERROR);
  
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
  if (!fs_mount_ok) statusLedSetState(LED_ERROR);
  else if (app && app->isBusy()) statusLedSetState(LED_BUSY);
  else statusLedSetState(LED_READY);
  statusLedUpdate();
  backlightAutoUpdate();
  delay(0);              // keep yielding without adding an extra 1ms frame stall
}
