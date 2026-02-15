#include "font_provider.h"
#include <stdint.h>

extern "C" {
#if defined(__GNUC__)
extern const lv_font_t font __attribute__((weak));
#else
extern const lv_font_t font;
#endif
}

const lv_font_t* getCompiledUserFont() {
#if defined(__GNUC__)
    if ((uintptr_t)&font == 0U) return nullptr;
#endif
    return &font;
}

