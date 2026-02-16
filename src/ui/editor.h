#ifndef EDITOR_H
#define EDITOR_H

#include <lvgl.h>
#include <functional>
#include "../config.h"
#include "font_manager.h"
#include "../ime/custom_pinyin_dict_plus.h"
#include "../ime/ime_mru.h"

class Editor {
private:
    static constexpr int32_t SCREEN_TRANSITION_MS = 55;
    static constexpr int32_t IME_ANIM_TIME_MS = 90;
    static constexpr int32_t EDITOR_TEXT_SIZE_PX = 14;
    static constexpr int32_t IME_CANDIDATE_H = 64;
    static constexpr int32_t IME_KEYBOARD_H = 132;
    static constexpr int32_t IME_TOTAL_H_FALLBACK = IME_CANDIDATE_H + IME_KEYBOARD_H;
    lv_obj_t* screen;
    lv_obj_t* textarea;
    lv_obj_t* ime;
    lv_obj_t* keyboard;
    lv_obj_t* ime_container;
    lv_obj_t* ime_btn;
    lv_obj_t* title_label;
    bool ime_visible;
    bool ime_font_acquired;
    String current_file;
    std::function<void()> on_exit_cb;
    std::function<void()> on_save_cb;

public:
    Editor() : screen(nullptr), textarea(nullptr), ime(nullptr),
               keyboard(nullptr), ime_container(nullptr), ime_btn(nullptr), title_label(nullptr),
               ime_visible(false), ime_font_acquired(false), current_file(""), on_exit_cb(nullptr), on_save_cb(nullptr) {}

    void create(std::function<void()> on_exit = nullptr, std::function<void()> on_save = nullptr) {
        on_exit_cb = on_exit;
        on_save_cb = on_save;
        screen = lv_obj_create(NULL);
        lv_obj_set_size(screen, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(screen, 0, 0);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_set_style_pad_row(screen, 0, 0);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* toolbar = lv_obj_create(screen);
        lv_obj_set_width(toolbar, lv_pct(100));
        lv_obj_set_height(toolbar, 35);
        lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(toolbar, 2, 0);
        lv_obj_set_style_pad_column(toolbar, 2, 0);
        lv_obj_set_style_margin_all(toolbar, 0, 0);
        lv_obj_set_style_radius(toolbar, 4, 0);
        lv_obj_set_style_clip_corner(toolbar, true, 0);
        lv_obj_set_style_bg_color(toolbar, lv_color_hex(0x101010), 0);
        lv_obj_set_style_border_color(toolbar, lv_color_hex(0x303030), 0);
        lv_obj_clear_flag(toolbar, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* left_wrap = lv_obj_create(toolbar);
        lv_obj_set_size(left_wrap, LV_SIZE_CONTENT, lv_pct(100));
        lv_obj_set_flex_flow(left_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_opa(left_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(left_wrap, 0, 0);
        lv_obj_set_style_pad_all(left_wrap, 0, 0);
        lv_obj_set_style_pad_column(left_wrap, 0, 0);
        lv_obj_clear_flag(left_wrap, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* exit_btn = lv_btn_create(left_wrap);
        lv_obj_set_size(exit_btn, 28, 26);
        styleActionButton(exit_btn);
        lv_obj_add_event_cb(exit_btn, exit_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* exit_label = lv_label_create(exit_btn);
        lv_label_set_text(exit_label, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(exit_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(exit_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(exit_label);

        lv_obj_t* center_wrap = lv_obj_create(toolbar);
        lv_obj_set_flex_grow(center_wrap, 1);
        lv_obj_set_height(center_wrap, lv_pct(100));
        lv_obj_set_flex_flow(center_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(center_wrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(center_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(center_wrap, 0, 0);
        lv_obj_set_style_pad_all(center_wrap, 0, 0);
        lv_obj_clear_flag(center_wrap, LV_OBJ_FLAG_SCROLLABLE);

        title_label = lv_label_create(center_wrap);
        lv_label_set_text(title_label, "Editor");
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(title_label, FontManager::textFont(), 0);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(title_label, lv_pct(100));
        lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* right_wrap = lv_obj_create(toolbar);
        lv_obj_set_size(right_wrap, LV_SIZE_CONTENT, lv_pct(100));
        lv_obj_set_flex_flow(right_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_opa(right_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right_wrap, 0, 0);
        lv_obj_set_style_pad_all(right_wrap, 0, 0);
        lv_obj_set_style_pad_column(right_wrap, 2, 0);
        lv_obj_clear_flag(right_wrap, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* save_btn = lv_btn_create(right_wrap);
        lv_obj_set_size(save_btn, 28, 26);
        styleActionButton(save_btn);
        lv_obj_add_event_cb(save_btn, save_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* save_label = lv_label_create(save_btn);
        lv_label_set_text(save_label, LV_SYMBOL_SAVE);
        lv_obj_set_style_text_font(save_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(save_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(save_label);

        ime_btn = lv_btn_create(right_wrap);
        lv_obj_set_size(ime_btn, 28, 26);
        styleActionButton(ime_btn);
        lv_obj_add_event_cb(ime_btn, ime_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* ime_label = lv_label_create(ime_btn);
        lv_label_set_text(ime_label, LV_SYMBOL_KEYBOARD);
        lv_obj_set_style_text_font(ime_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(ime_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(ime_label);

        textarea = lv_textarea_create(screen);
        lv_obj_set_width(textarea, lv_pct(100));
        lv_obj_set_flex_grow(textarea, 1);
        lv_textarea_set_one_line(textarea, false);
        lv_textarea_set_cursor_click_pos(textarea, true);
        lv_obj_set_style_bg_color(textarea, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(textarea, lv_color_hex(0x303030), 0);
        lv_obj_set_style_text_color(textarea, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_margin_all(textarea, 0, 0);
        lv_obj_set_style_radius(textarea, 4, 0);
        lv_obj_set_style_pad_all(textarea, 4, 0);
        lv_obj_set_style_text_font(textarea, FontManager::textFont(), 0);
        lv_obj_set_style_text_line_space(textarea, -4, 0);
        lv_obj_set_style_bg_color(textarea, lv_color_hex(0xFFFFFF), LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(textarea, LV_OPA_70, LV_PART_CURSOR);
        lv_obj_set_style_width(textarea, 1, LV_PART_CURSOR);
        int32_t editor_cursor_h = EDITOR_TEXT_SIZE_PX;
        if (editor_cursor_h < 4) editor_cursor_h = 4;
        lv_obj_set_style_height(textarea, editor_cursor_h, LV_PART_CURSOR);

        ime_container = lv_obj_create(screen);
        lv_obj_set_width(ime_container, lv_pct(100));
        lv_obj_set_height(ime_container, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(ime_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(ime_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(ime_container, lv_color_hex(0x303030), 0);
        lv_obj_set_style_border_width(ime_container, 0, 0);
        lv_obj_set_style_margin_all(ime_container, 0, 0);
        lv_obj_set_style_radius(ime_container, 4, 0);
        lv_obj_set_style_pad_all(ime_container, 0, 0);
        lv_obj_clear_flag(ime_container, LV_OBJ_FLAG_SCROLLABLE);

        ime = lv_ime_pinyin_create(ime_container);
        lv_obj_set_width(ime, lv_pct(100));
        lv_obj_set_height(ime, IME_CANDIDATE_H);
        lv_obj_set_style_bg_color(ime, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(ime, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(ime, lv_color_hex(0x111111), LV_PART_ITEMS);
        lv_obj_set_style_text_color(ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_font(ime, FontManager::imeFont(), LV_PART_MAIN);
        lv_obj_set_style_text_font(ime, FontManager::imeFont(), LV_PART_ITEMS);

        keyboard = lv_keyboard_create(ime_container);
        lv_obj_set_width(keyboard, lv_pct(100));
        lv_obj_set_height(keyboard, IME_KEYBOARD_H);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x1A1A1A), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x222222), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x2A2A2A), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x202020), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(keyboard, lv_color_hex(0x111111), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(keyboard, lv_color_hex(0xB8B8B8), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x303030), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x3A3A3A), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x4A4A4A), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x3A3A3A), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(keyboard, lv_color_hex(0x242424), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_text_font(keyboard, FontManager::imeFont(), LV_PART_MAIN);
        lv_obj_set_style_text_font(keyboard, FontManager::imeFont(), LV_PART_ITEMS);
        lv_keyboard_set_textarea(keyboard, textarea);
        lv_ime_pinyin_set_keyboard(ime, keyboard);
        lv_ime_pinyin_set_dict(ime, g_pinyin_dict_plus);
        lv_ime_pinyin_set_mode(ime, LV_IME_PINYIN_MODE_K26);
        ImeMru::getInstance().init();
        lv_obj_t* cand_panel = lv_ime_pinyin_get_cand_panel(ime);
        lv_obj_add_event_cb(keyboard, ime_keyboard_mru_event_cb, LV_EVENT_VALUE_CHANGED, this);
        if (cand_panel) {
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x111111), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x1A1A1A), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x202020), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x151515), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_add_event_cb(cand_panel, ime_cand_mru_event_cb, LV_EVENT_VALUE_CHANGED, this);
        }
        applyIMEFonts();

        lv_obj_add_flag(ime_container, LV_OBJ_FLAG_HIDDEN);
        ime_visible = false;
    }

    void destroy() {
        if (ime_font_acquired) {
            FontManager::releaseIMEFont();
            ime_font_acquired = false;
        }
        if (screen) {
            lv_obj_del(screen);
            screen = nullptr;
            textarea = nullptr;
            ime = nullptr;
            keyboard = nullptr;
            ime_container = nullptr;
            ime_btn = nullptr;
            title_label = nullptr;
        }
    }

    void show(lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE) {
        if (screen) {
            if(anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(screen);
            else lv_screen_load_anim(screen, anim, SCREEN_TRANSITION_MS, 0, false);
        }
    }

    void toggleIME() {
        setIMEVisible(!ime_visible);
    }

    bool isIMEVisible() const {
        return ime_visible;
    }

    void setText(const String& content) {
        if (textarea) lv_textarea_set_text(textarea, content.c_str());
    }

    String getText() {
        if (textarea) return String(lv_textarea_get_text(textarea));
        return "";
    }

    void setTitle(const String& filename) {
        current_file = filename;
        if (!title_label) return;
        int idx = filename.lastIndexOf('/');
        String name = (idx >= 0 && idx < (int)filename.length() - 1) ? filename.substring(idx + 1) : filename;
        if (name.length() == 0) name = "Editor";
        lv_label_set_text(title_label, name.c_str());
    }

    String getTitle() const {
        return current_file;
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
        lv_obj_set_style_shadow_width(btn, 0, 0);
    }

    void setIMEVisible(bool visible) {
        ime_visible = visible;
        if (!ime_container) return;
        lv_anim_del(ime_container, ime_translate_y_anim_cb);

        int32_t h = lv_obj_get_height(ime_container);
        if (h <= 0) h = IME_TOTAL_H_FALLBACK;  // fallback for first layout pass

        if (ime_visible) {
            if (!ime_font_acquired) ime_font_acquired = FontManager::acquireIMEFont();
            lv_obj_remove_flag(ime_container, LV_OBJ_FLAG_HIDDEN);
            applyIMEFonts();
            lv_obj_set_style_translate_y(ime_container, h, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, ime_container);
            lv_anim_set_exec_cb(&a, ime_translate_y_anim_cb);
            lv_anim_set_values(&a, h, 0);
            lv_anim_set_time(&a, IME_ANIM_TIME_MS);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, ime_container);
            lv_anim_set_exec_cb(&a, ime_translate_y_anim_cb);
            lv_anim_set_values(&a, 0, h);
            lv_anim_set_time(&a, IME_ANIM_TIME_MS);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
            lv_anim_set_user_data(&a, this);
            lv_anim_set_completed_cb(&a, ime_hide_anim_done_cb);
            lv_anim_start(&a);
        }
    }

    static void ime_translate_y_anim_cb(void* obj, int32_t v) {
        lv_obj_set_style_translate_y((lv_obj_t*)obj, v, 0);
    }

    static void ime_hide_anim_done_cb(lv_anim_t* a) {
        Editor* ed = (Editor*)lv_anim_get_user_data(a);
        if (!ed || !ed->ime_container) return;
        lv_obj_add_flag(ed->ime_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_translate_y(ed->ime_container, 0, 0);
        if (ed->ime_font_acquired) {
            FontManager::releaseIMEFont();
            ed->ime_font_acquired = false;
        }
    }

    void applyTextFontRecursive(lv_obj_t* root, const lv_font_t* font) {
        if (!root || !font) return;
        lv_obj_set_style_text_font(root, font, LV_PART_MAIN);
        lv_obj_set_style_text_font(root, font, LV_PART_ITEMS);
        uint32_t count = lv_obj_get_child_count(root);
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* child = lv_obj_get_child(root, i);
            applyTextFontRecursive(child, font);
        }
    }

    void applyIMEFonts() {
        const lv_font_t* font = FontManager::imeFont();
        if (!font || !ime_container) return;
        applyTextFontRecursive(ime_container, font);
    }

    static void ime_btn_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed) return;
        ed->toggleIME();
    }

    static void exit_btn_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed) return;
        if (ed->on_exit_cb) {
            ed->on_exit_cb();
        }
    }

    static void save_btn_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed) return;
        if (ed->on_save_cb) {
            ed->on_save_cb();
        }
    }

    static void ime_keyboard_mru_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed || !ed->ime) return;
        lv_obj_t* cand_panel = lv_ime_pinyin_get_cand_panel(ed->ime);
        ImeMru::getInstance().applyToCandidatePanel(cand_panel);
    }

    static void ime_cand_mru_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        lv_obj_t* panel = (lv_obj_t*)lv_event_get_target(e);
        if (!ed || !panel) return;
        uint32_t id = lv_buttonmatrix_get_selected_button(panel);
        const char* txt = lv_buttonmatrix_get_button_text(panel, id);
        ImeMru::getInstance().learnFromUtf8(txt);
        ImeMru::getInstance().applyToCandidatePanel(panel);
    }
};

#endif
