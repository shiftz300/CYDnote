#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <lvgl.h>
#include <functional>

class FileManager {
private:
    lv_obj_t* screen;
    lv_obj_t* list_container;
    std::function<void(const char*)> on_open_cb;
    
public:
    FileManager() : screen(nullptr), list_container(nullptr), on_open_cb(nullptr) {}
    
    // Create the file manager with LVGL file explorer
    void create(std::function<void(const char*)> on_open=nullptr) {
        Serial.println("[FileManager] Creating with lv_file_explorer...");
        on_open_cb = on_open;
        
        Serial.println("[FileManager] Creating screen...");
        screen = lv_obj_create(NULL);
        lv_obj_set_size(screen, 240, 320);

        // Create file explorer widget
        list_container = lv_file_explorer_create(screen);
        lv_file_explorer_set_sort(list_container, LV_EXPLORER_SORT_KIND);
        
        // Open root directory using SPIFFS driver (letter 'S')
        lv_file_explorer_open_dir(list_container, "S:/");
        
        // Add event callback for file selection
        lv_obj_add_event_cb(list_container, file_explorer_event_cb, LV_EVENT_VALUE_CHANGED, this);
        
        Serial.println("[FileManager] Creation complete");
    }

    void destroy() {
        if (screen) {
            lv_obj_del(screen);
            screen = nullptr;
            list_container = nullptr;
        }
    }

    void show() {
        if (screen) {
            Serial.println("[FileManager] Loading screen");
            lv_scr_load(screen);
        }
    }

private:
    static void file_explorer_event_cb(lv_event_t* e) {
        lv_event_code_t code = lv_event_get_code(e);
        lv_obj_t* explorer = (lv_obj_t*)lv_event_get_target(e);
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        
        if (!explorer || !fm) return;
        
        if (code == LV_EVENT_VALUE_CHANGED) {
            const char* sel_fn = lv_file_explorer_get_selected_file_name(explorer);
            if (sel_fn) {
                Serial.print("[FileManager] File selected: ");
                Serial.println(sel_fn);
                if (fm->on_open_cb) {
                    fm->on_open_cb(sel_fn);
                }
            }
        }
    }
};

#endif
