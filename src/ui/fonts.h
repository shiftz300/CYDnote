#ifndef FONTS_H
#define FONTS_H

#include <Arduino.h>
#include <lvgl.h>
#include "../font/provider.h"

class FontManager {
private:
    static bool initialized;
    static lv_font_t text_font_with_fallback;
    static bool text_font_ready;

    static const lv_font_t* customFont() {
        return getCompiledUserFont();
    }

public:
    static void init() {
        if (initialized) return;
        initialized = true;

        const lv_font_t* cf = customFont();
        if (cf) {
            text_font_with_fallback = *cf;
            text_font_with_fallback.fallback = &lv_font_montserrat_14;
            text_font_ready = true;
        } else {
            text_font_ready = false;
            Serial.println("[FontManager] font.c not found, fallback to built-in");
        }
    }

    static bool acquireIMEFont() {
        return true;
    }

    static void releaseIMEFont() {
    }

    static const lv_font_t* textFont() {
        if (text_font_ready) return &text_font_with_fallback;
#if LV_FONT_SOURCE_HAN_SANS_SC_14_CJK
        return &lv_font_source_han_sans_sc_14_cjk;
#else
        return &lv_font_montserrat_14;
#endif
    }

    static const lv_font_t* imeFont() {
        return textFont();
    }

    static const lv_font_t* iconFont() {
        return &lv_font_montserrat_14;
    }
};

bool FontManager::initialized = false;
lv_font_t FontManager::text_font_with_fallback = {};
bool FontManager::text_font_ready = false;

#endif
