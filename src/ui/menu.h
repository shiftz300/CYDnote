#ifndef MENU_H
#define MENU_H

#include <lvgl.h>
#include "../config.h"

enum MenuAction {
    MENU_SAVE,
    MENU_SAVE_AS,
    MENU_READ_MODE,
    MENU_SERVE_AP,
    MENU_EXIT,
    MENU_NONE
};

class MenuManager {
private:
    lv_obj_t* menu_window;
    lv_obj_t* menu_list;
    bool menu_visible;
    MenuAction last_action;
    
public:
    MenuManager() : menu_window(nullptr), menu_list(nullptr), 
                    menu_visible(false), last_action(MENU_NONE) {}
    
    void create() {
        // Create a modal window for the menu
        menu_window = lv_obj_create(NULL);
        lv_obj_set_size(menu_window, 150, 200);
        lv_obj_align(menu_window, LV_ALIGN_TOP_RIGHT, -5, 40);
        lv_obj_set_style_bg_color(menu_window, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(menu_window, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(menu_window, 1, 0);
        lv_obj_set_style_pad_all(menu_window, 3, 0);
        
        // Create a flex container for menu items
        menu_list = lv_obj_create(menu_window);
        lv_obj_set_size(menu_list, 150, 200);
        lv_obj_set_flex_flow(menu_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_pad_all(menu_list, 2, 0);
        lv_obj_set_style_bg_color(menu_list, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(menu_list, lv_color_hex(0x303030), 0);
        
        // Add menu items
        addMenuItem("Save");
        addMenuItem("Save As");
        addMenuItem("Read Mode");
        addMenuItem("Serve AP");
        addMenuItem("Exit");
        
        // Initially hidden
        lv_obj_add_flag(menu_window, LV_OBJ_FLAG_HIDDEN);
        menu_visible = false;
    }
    
    void toggle() {
        if (menu_visible) {
            lv_obj_add_flag(menu_window, LV_OBJ_FLAG_HIDDEN);
            menu_visible = false;
        } else {
            lv_obj_remove_flag(menu_window, LV_OBJ_FLAG_HIDDEN);
            menu_visible = true;
        }
    }
    
    bool isVisible() const {
        return menu_visible;
    }
    
    MenuAction getLastAction() const {
        return last_action;
    }
    
    void clearAction() {
        last_action = MENU_NONE;
    }
    
    void destroy() {
        if (menu_window) {
            lv_obj_del(menu_window);
            menu_window = nullptr;
            menu_list = nullptr;
        }
    }
    
private:
    void addMenuItem(const char* label) {
        lv_obj_t* btn = lv_btn_create(menu_list);
        lv_obj_set_size(btn, 150, 30);
        lv_obj_set_flex_grow(btn, 0);
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E1E), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x5CB8FF), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
        
        // Set different styles for different buttons
        if (strcmp(label, "Save") == 0) {
            lv_obj_set_user_data(btn, (void*)(intptr_t)MENU_SAVE);
            lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, this);
        } else if (strcmp(label, "Save As") == 0) {
            lv_obj_set_user_data(btn, (void*)(intptr_t)MENU_SAVE_AS);
            lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, this);
        } else if (strcmp(label, "Read Mode") == 0) {
            lv_obj_set_user_data(btn, (void*)(intptr_t)MENU_READ_MODE);
            lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, this);
        } else if (strcmp(label, "Serve AP") == 0) {
            lv_obj_set_user_data(btn, (void*)(intptr_t)MENU_SERVE_AP);
            lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, this);
        } else if (strcmp(label, "Exit") == 0) {
            lv_obj_set_user_data(btn, (void*)(intptr_t)MENU_EXIT);
            lv_obj_add_event_cb(btn, menu_event_cb, LV_EVENT_CLICKED, this);
        }
    }
    
    static void menu_event_cb(lv_event_t* e) {
        MenuManager* mgr = (MenuManager*)lv_event_get_user_data(e);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (!mgr || !btn) return;
        MenuAction act = (MenuAction)(intptr_t)lv_obj_get_user_data(btn);
        mgr->last_action = act;
    }
};

#endif

