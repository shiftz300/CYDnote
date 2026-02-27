#ifndef VIEWER_H
#define VIEWER_H

#include <Arduino.h>
#include <lvgl.h>
#include <functional>
#include "../config.h"
#include "fonts.h"

class ImageViewer {
private:
    static constexpr int32_t SCREEN_TRANSITION_MS = 55;
    static constexpr uint32_t SWIPE_DEBOUNCE_MS = 220;
    static constexpr int16_t SWIPE_MIN_DX = 28;
    lv_obj_t* screen;
    lv_obj_t* back_btn;
    lv_obj_t* image;
    lv_obj_t* hint_label;
    std::function<void()> on_back_cb;
    std::function<void()> on_prev_cb;
    std::function<void()> on_next_cb;
    uint32_t last_swipe_ms;
    int16_t touch_start_x;
    int16_t touch_start_y;
    bool touch_tracking;
    bool image_loading;

public:
    ImageViewer() : screen(nullptr), back_btn(nullptr), image(nullptr), hint_label(nullptr),
                    on_back_cb(nullptr), on_prev_cb(nullptr), on_next_cb(nullptr), last_swipe_ms(0),
                    touch_start_x(0), touch_start_y(0), touch_tracking(false), image_loading(false) {}

    void create(std::function<void()> on_back = nullptr,
                std::function<void()> on_prev = nullptr,
                std::function<void()> on_next = nullptr) {
        on_back_cb = on_back;
        on_prev_cb = on_prev;
        on_next_cb = on_next;
        screen = lv_obj_create(NULL);
        lv_obj_set_size(screen, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(screen, 0, 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(screen, LV_OBJ_FLAG_CLICKABLE);

        image = lv_image_create(screen);
        lv_obj_set_size(image, lv_pct(100), lv_pct(100));
        lv_image_set_inner_align(image, LV_IMAGE_ALIGN_CONTAIN);
        lv_obj_center(image);
        lv_obj_add_flag(image, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(image, image_touch_event_cb, LV_EVENT_PRESSED, this);
        lv_obj_add_event_cb(image, image_touch_event_cb, LV_EVENT_RELEASED, this);
        lv_obj_add_event_cb(image, image_touch_event_cb, LV_EVENT_PRESS_LOST, this);

        hint_label = lv_label_create(screen);
        lv_label_set_text(hint_label, "Select an image");
        lv_obj_set_style_text_font(hint_label, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(hint_label, lv_color_hex(0xA0A0A0), 0);
        lv_obj_align(hint_label, LV_ALIGN_CENTER, 0, 0);

        back_btn = lv_btn_create(screen);
        lv_obj_set_size(back_btn, 28, 26);
        styleActionButton(back_btn);
        lv_obj_set_style_bg_opa(back_btn, LV_OPA_40, LV_STATE_DEFAULT);
        lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 4, 4);
        lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* back_label = lv_label_create(back_btn);
        lv_label_set_text(back_label, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(back_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(back_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(back_label);
        lv_obj_move_foreground(back_btn);
    }

    void show(lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE) {
        if (!screen) return;
        if (anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(screen);
        else lv_screen_load_anim(screen, anim, SCREEN_TRANSITION_MS, 0, false);
    }

    void setTitle(const String& filename) {
        LV_UNUSED(filename);
    }

    void setImage(const String& vpath) {
        if (!image || !hint_label) return;
        if (image_loading) return;
        image_loading = true;
        if (vpath.length() == 0) {
            lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(hint_label, "Empty path");
            lv_image_set_src(image, nullptr);
            image_loading = false;
            return;
        }
        lv_image_header_t header;
        lv_result_t info_res = lv_image_decoder_get_info(vpath.c_str(), &header);
        if (info_res == LV_RESULT_OK) {
            lv_image_set_src(image, vpath.c_str());
            lv_obj_add_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_image_set_src(image, nullptr);
            lv_obj_clear_flag(hint_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(hint_label, "Unsupported image");
        }
        image_loading = false;
    }

private:
    void styleActionButton(lv_obj_t* btn) {
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E1E), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x5CB8FF), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    }

    static void back_btn_event_cb(lv_event_t* e) {
        ImageViewer* viewer = (ImageViewer*)lv_event_get_user_data(e);
        if (viewer && viewer->on_back_cb) viewer->on_back_cb();
    }

    static void image_touch_event_cb(lv_event_t* e) {
        ImageViewer* viewer = (ImageViewer*)lv_event_get_user_data(e);
        if (!viewer) return;
        lv_event_code_t code = lv_event_get_code(e);
        lv_indev_t* indev = lv_indev_active();
        if (!indev) return;

        lv_point_t p;
        lv_indev_get_point(indev, &p);

        if (code == LV_EVENT_PRESSED) {
            viewer->touch_start_x = p.x;
            viewer->touch_start_y = p.y;
            viewer->touch_tracking = true;
            return;
        }

        if (code == LV_EVENT_PRESS_LOST) {
            viewer->touch_tracking = false;
            return;
        }

        if (code != LV_EVENT_RELEASED || !viewer->touch_tracking) return;
        viewer->touch_tracking = false;

        int16_t dx = (int16_t)(p.x - viewer->touch_start_x);
        int16_t dy = (int16_t)(p.y - viewer->touch_start_y);
        int16_t adx = dx >= 0 ? dx : (int16_t)-dx;
        int16_t ady = dy >= 0 ? dy : (int16_t)-dy;
        if (adx < SWIPE_MIN_DX) return;
        if (adx <= ady) return;

        uint32_t now = lv_tick_get();
        if ((now - viewer->last_swipe_ms) < SWIPE_DEBOUNCE_MS) return;
        viewer->last_swipe_ms = now;

        if (dx < 0) {
            if (viewer->on_next_cb) viewer->on_next_cb();
        } else {
            if (viewer->on_prev_cb) viewer->on_prev_cb();
        }
    }
};

#endif
