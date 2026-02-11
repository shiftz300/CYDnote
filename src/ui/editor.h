#ifndef EDITOR_H
#define EDITOR_H

#include <lvgl.h>
#include "../config.h"

class Editor {
private:
    lv_obj_t* screen;
    lv_obj_t* textarea;
    lv_obj_t* ime;
    lv_obj_t* ime_container;
    bool ime_visible;
    String current_file;
    
public:
    Editor() : screen(nullptr), textarea(nullptr), ime(nullptr), 
               ime_visible(false), current_file("") {}
    
    void create() {
        screen = lv_obj_create(NULL);
        lv_obj_set_size(screen, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_COLUMN);
        
        // Create toolbar (title + buttons)
        lv_obj_t* toolbar = lv_obj_create(screen);
        lv_obj_set_size(toolbar, SCREEN_WIDTH, 35);
        lv_obj_set_flex_flow(toolbar, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_all(toolbar, 2, 0);
        
        lv_obj_t* title = lv_label_create(toolbar);
        lv_label_set_text(title, "Editor: ");
        lv_obj_set_flex_grow(title, 0);
        
        // Create menu button
        lv_obj_t* menu_btn = lv_btn_create(toolbar);
        lv_obj_set_size(menu_btn, 50, 30);
        lv_obj_t* menu_label = lv_label_create(menu_btn);
        lv_label_set_text(menu_label, "Menu");
        lv_obj_set_flex_grow(menu_btn, 1);
        
        // Create IME toggle button
        lv_obj_t* ime_btn = lv_btn_create(toolbar);
        lv_obj_set_size(ime_btn, 45, 30);
        lv_obj_t* ime_label = lv_label_create(ime_btn);
        lv_label_set_text(ime_label, "输入");
        
        // Create textarea for text editing
        textarea = lv_textarea_create(screen);
        lv_obj_set_size(textarea, SCREEN_WIDTH, SCREEN_HEIGHT - 100);
        lv_textarea_set_one_line(textarea, false);
        // Set cursor and text properties
        lv_textarea_set_cursor_click_pos(textarea, true);
        lv_obj_set_style_text_color(textarea, lv_color_hex(0x000000), 0);
        
        // Create IME container (collapsible)
        ime_container = lv_obj_create(screen);
        lv_obj_set_size(ime_container, SCREEN_WIDTH, 60);
        lv_obj_set_flex_flow(ime_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_border_color(ime_container, lv_color_hex(0xCCCCCC), 0);
        lv_obj_set_style_border_width(ime_container, 1, 0);
        
        // Create IME Pinyin input method
        ime = lv_ime_pinyin_create(ime_container);
        lv_obj_set_size(ime, SCREEN_WIDTH, 60);
        // NOTE: not calling lv_ime_set_textarea because API may differ between LVGL versions.
        // The IME is still placed in the screen; users can toggle it with toggleIME().
        
        // Initially hide IME
        lv_obj_add_flag(ime_container, LV_OBJ_FLAG_HIDDEN);
        ime_visible = false;
    }
    
    void destroy() {
        if (screen) {
            lv_obj_del(screen);
            screen = nullptr;
        }
    }
    
    void show() {
        if (screen) {
            lv_scr_load(screen);
        }
    }
    
    void toggleIME() {
        ime_visible = !ime_visible;
        if (ime && lv_obj_get_parent(ime)) {
            lv_obj_t* parent = lv_obj_get_parent(ime);
            if (ime_visible) {
                lv_obj_remove_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
    
    bool isIMEVisible() const {
        return ime_visible;
    }
    
    void setText(const String& content) {
        if (textarea) {
            lv_textarea_set_text(textarea, content.c_str());
        }
    }
    
    String getText() {
        if (textarea) {
            return String(lv_textarea_get_text(textarea));
        }
        return "";
    }
    
    void setTitle(const String& filename) {
        current_file = filename;
    }
    
    String getTitle() const {
        return current_file;
    }
};

#endif
