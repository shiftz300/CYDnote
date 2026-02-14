#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>

class FontManager {
private:
    static bool initialized;

public:
    static void init() {
        if (initialized) return;
        initialized = true;
        Serial.println("[FontManager] Using LVGL default CJK font");
    }

    static bool acquireIMEFont() {
        return true;
    }

    static void releaseIMEFont() {
    }

    static const lv_font_t* textFont() {
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

#endif
