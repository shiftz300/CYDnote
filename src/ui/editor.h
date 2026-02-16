#ifndef EDITOR_H
#define EDITOR_H

#include <lvgl.h>
#include <functional>
#include <string.h>
#include "../config.h"
#include "font_manager.h"
#include "../ime/custom_pinyin_dict_plus.h"

class Editor {
private:
    static constexpr int32_t SCREEN_TRANSITION_MS = 55;
    static constexpr int32_t IME_ANIM_TIME_MS = 90;
    static constexpr int32_t EDITOR_TEXT_SIZE_PX = 14;
    static constexpr int32_t IME_CANDIDATE_H = 28;
    static constexpr int32_t IME_KEYBOARD_H = 132;
    static constexpr int32_t IME_TOTAL_H_FALLBACK = IME_CANDIDATE_H + IME_KEYBOARD_H;
    static constexpr uint8_t IME_PROXY_CAND_MAX = 20;
    static constexpr uint8_t IME_CAND_PER_PAGE = 8;
    static constexpr uint16_t IME_CAND_MAX = 160;
    lv_obj_t* screen;
    lv_obj_t* textarea;
    lv_obj_t* ime;
    lv_obj_t* keyboard;
    lv_obj_t* ime_container;
    lv_obj_t* ime_cand_proxy;
    lv_obj_t* ime_cand_src;
    lv_obj_t* ime_btn;
    lv_obj_t* title_wrap;
    lv_obj_t* title_label;
    lv_obj_t* top_btn;
    lv_obj_t* save_popup;
    lv_timer_t* save_popup_timer;
    char ime_cand_texts[IME_PROXY_CAND_MAX][8];
    const char* ime_cand_map[IME_PROXY_CAND_MAX + 1];
    uint8_t ime_cand_src_idx[IME_PROXY_CAND_MAX];
    bool ime_cand_syncing;
    char ime_compose[16];
    bool ime_is_k9_mode;
    uint16_t ime_cand_count;
    uint16_t ime_cand_page;
    char ime_cands[IME_CAND_MAX][8];
    bool ime_visible;
    bool ime_font_acquired;
    uint32_t ime_cursor_anchor_pos;
    bool ime_cursor_anchor_valid;
    String current_file;
    std::function<void()> on_exit_cb;
    std::function<void()> on_save_cb;

public:
    Editor() : screen(nullptr), textarea(nullptr), ime(nullptr),
               keyboard(nullptr), ime_container(nullptr), ime_cand_proxy(nullptr), ime_cand_src(nullptr), ime_btn(nullptr), title_wrap(nullptr), title_label(nullptr), top_btn(nullptr),
               save_popup(nullptr), save_popup_timer(nullptr),
               ime_cand_syncing(false), ime_is_k9_mode(false), ime_cand_count(0), ime_cand_page(0),
               ime_visible(false), ime_font_acquired(false), ime_cursor_anchor_pos(0), ime_cursor_anchor_valid(false),
               current_file(""), on_exit_cb(nullptr), on_save_cb(nullptr) {
        memset(ime_compose, 0, sizeof(ime_compose));
        memset(ime_cands, 0, sizeof(ime_cands));
        memset(ime_cand_texts, 0, sizeof(ime_cand_texts));
        memset(ime_cand_map, 0, sizeof(ime_cand_map));
        memset(ime_cand_src_idx, 0xFF, sizeof(ime_cand_src_idx));
    }

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

        title_wrap = lv_obj_create(toolbar);
        lv_obj_set_flex_grow(title_wrap, 1);
        lv_obj_set_height(title_wrap, lv_pct(100));
        lv_obj_set_flex_flow(title_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(title_wrap, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(title_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(title_wrap, 0, 0);
        lv_obj_set_style_pad_all(title_wrap, 0, 0);
        lv_obj_set_scroll_dir(title_wrap, LV_DIR_HOR);
        lv_obj_set_scrollbar_mode(title_wrap, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(title_wrap, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(title_wrap, LV_OBJ_FLAG_SCROLL_MOMENTUM);
        lv_obj_add_flag(title_wrap, LV_OBJ_FLAG_SCROLL_ELASTIC);

        title_label = lv_label_create(title_wrap);
        lv_label_set_text(title_label, "Editor");
        lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(title_label, FontManager::textFont(), 0);
        lv_label_set_long_mode(title_label, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(title_label, LV_SIZE_CONTENT);
        lv_obj_set_style_text_align(title_label, LV_TEXT_ALIGN_LEFT, 0);

        lv_obj_t* right_wrap = lv_obj_create(toolbar);
        lv_obj_set_size(right_wrap, LV_SIZE_CONTENT, lv_pct(100));
        lv_obj_set_flex_flow(right_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_opa(right_wrap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(right_wrap, 0, 0);
        lv_obj_set_style_pad_all(right_wrap, 0, 0);
        lv_obj_set_style_pad_column(right_wrap, 2, 0);
        lv_obj_clear_flag(right_wrap, LV_OBJ_FLAG_SCROLLABLE);

        top_btn = lv_btn_create(right_wrap);
        lv_obj_set_size(top_btn, 28, 26);
        styleActionButton(top_btn);
        lv_obj_add_event_cb(top_btn, top_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* top_label = lv_label_create(top_btn);
        lv_label_set_text(top_label, LV_SYMBOL_UP);
        lv_obj_set_style_text_font(top_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(top_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(top_label);

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
        lv_obj_add_event_cb(textarea, textarea_cursor_anchor_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(textarea, textarea_cursor_anchor_event_cb, LV_EVENT_VALUE_CHANGED, this);
        ime_cursor_anchor_pos = lv_textarea_get_cursor_pos(textarea);
        ime_cursor_anchor_valid = true;

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
        static const char * k9_map[] = {
            "2", "3", "4", "\n",
            "5", "6", "7", "\n",
            "8", "9", "0", "\n",
            "ABC", LV_SYMBOL_LEFT, LV_SYMBOL_RIGHT, LV_SYMBOL_BACKSPACE, ""
        };
        lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
        lv_keyboard_set_map(keyboard, LV_KEYBOARD_MODE_USER_1, k9_map, nullptr);
        lv_ime_pinyin_set_keyboard(ime, keyboard);
        lv_ime_pinyin_set_dict(ime, g_pinyin_dict_plus);
        lv_ime_pinyin_set_mode(ime, LV_IME_PINYIN_MODE_K26);
        lv_obj_t* cand_panel = lv_ime_pinyin_get_cand_panel(ime);
        if (cand_panel) {
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x111111), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x1A1A1A), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x202020), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x151515), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(cand_panel, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(cand_panel, LV_OPA_TRANSP, LV_PART_ITEMS);
            lv_obj_set_style_border_width(cand_panel, 0, LV_PART_MAIN);
            lv_obj_set_style_border_width(cand_panel, 0, LV_PART_ITEMS);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DISABLED);
        }
        applyIMEFonts();

        lv_obj_add_flag(ime_container, LV_OBJ_FLAG_HIDDEN);
        ime_visible = false;
    }

    void destroy() {
        closeSavePopup();
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
            ime_cand_proxy = nullptr;
            ime_cand_src = nullptr;
            ime_btn = nullptr;
            title_wrap = nullptr;
            title_label = nullptr;
            top_btn = nullptr;
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
        if (!textarea) return;
        lv_textarea_set_text(textarea, content.c_str());
        moveCursorAndViewToStart();
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
        if (title_wrap) lv_obj_scroll_to_x(title_wrap, 0, LV_ANIM_OFF);
    }

    String getTitle() const {
        return current_file;
    }

    void showSaveSuccessPopup(const String& filename, const String& from_size, const String& to_size) {
        closeSavePopup();
        if (!screen) return;

        save_popup = lv_obj_create(screen);
        lv_obj_add_flag(save_popup, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(save_popup, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(save_popup, 210);
        lv_obj_set_height(save_popup, LV_SIZE_CONTENT);
        lv_obj_align(save_popup, LV_ALIGN_TOP_MID, 0, 44);
        lv_obj_set_style_bg_color(save_popup, lv_color_hex(0x0E0E0E), 0);
        lv_obj_set_style_border_color(save_popup, lv_color_hex(0x4A4A4A), 0);
        lv_obj_set_style_border_width(save_popup, 1, 0);
        lv_obj_set_style_radius(save_popup, 6, 0);
        lv_obj_set_style_pad_all(save_popup, 6, 0);
        lv_obj_set_style_pad_row(save_popup, 2, 0);
        lv_obj_set_flex_flow(save_popup, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(save_popup, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(save_popup);
        lv_label_set_text(title, "Saved");
        lv_obj_set_style_text_color(title, lv_color_hex(0xBFDFFF), 0);
        lv_obj_set_style_text_font(title, FontManager::iconFont(), 0);

        lv_obj_t* file_lbl = lv_label_create(save_popup);
        lv_label_set_text(file_lbl, filename.c_str());
        lv_label_set_long_mode(file_lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(file_lbl, lv_pct(100));
        lv_obj_set_style_text_color(file_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(file_lbl, FontManager::textFont(), 0);

        String size_line = from_size + " -> " + to_size;
        lv_obj_t* size_lbl = lv_label_create(save_popup);
        lv_label_set_text(size_lbl, size_line.c_str());
        lv_obj_set_style_text_color(size_lbl, lv_color_hex(0xC8C8C8), 0);
        lv_obj_set_style_text_font(size_lbl, FontManager::textFont(), 0);

        save_popup_timer = lv_timer_create(save_popup_timer_cb, 1600, this);
        if (save_popup_timer) lv_timer_set_repeat_count(save_popup_timer, 1);
    }

private:
    void closeSavePopup() {
        if (save_popup_timer) {
            lv_timer_del(save_popup_timer);
            save_popup_timer = nullptr;
        }
        if (save_popup) {
            lv_obj_del(save_popup);
            save_popup = nullptr;
        }
    }

    static void save_popup_timer_cb(lv_timer_t* t) {
        if (!t) return;
        Editor* ed = (Editor*)lv_timer_get_user_data(t);
        if (!ed) return;
        ed->closeSavePopup();
    }

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

    static void textarea_cursor_anchor_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed || !ed->textarea) return;
        ed->ime_cursor_anchor_pos = lv_textarea_get_cursor_pos(ed->textarea);
        ed->ime_cursor_anchor_valid = true;
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

    static void top_btn_event_cb(lv_event_t* e) {
        Editor* ed = (Editor*)lv_event_get_user_data(e);
        if (!ed) return;
        ed->moveCursorAndViewToStart();
    }

    static void async_scroll_top_cb(void* user_data) {
        Editor* ed = (Editor*)user_data;
        if (!ed || !ed->textarea) return;
        lv_obj_scroll_to_y(ed->textarea, 0, LV_ANIM_OFF);
    }

    void moveCursorAndViewToStart() {
        if (!textarea) return;
        lv_textarea_set_cursor_pos(textarea, 0);
        lv_obj_scroll_to_y(textarea, 0, LV_ANIM_OFF);
        ime_cursor_anchor_pos = 0;
        ime_cursor_anchor_valid = true;
        lv_async_call(async_scroll_top_cb, this);
    }

};

#endif
