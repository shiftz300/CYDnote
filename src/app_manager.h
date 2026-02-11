 #ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include <lvgl.h>
#include "config.h"
#include "utils/sd_helper.h"

// For readability in AppManager context
using SDHelper = StorageHelper;
#include "ui/file_manager.h"
#include "ui/editor.h"
#include "ui/menu.h"

class AppManager {
private:
    AppMode current_mode;
    FileManager file_manager;
    Editor editor;
    MenuManager menu_manager;
    SDHelper* sd_helper;
    String current_filename;
    
    // Static wrapper for LVGL callbacks
    static AppManager* instance;
    
public:
    AppManager() : current_mode(MODE_FILE_MANAGER), sd_helper(nullptr) {
        instance = this;
    }
    
    void init() {
        Serial.println("[AppManager] Initializing...");
        sd_helper = SDHelper::getInstance();
        
        // Create UI components
        Serial.println("[AppManager] Creating FileManager...");
        file_manager.create([this](const char* name){ this->showEditor(String(name)); });
        Serial.println("[AppManager] FileManager created");
        
        Serial.println("[AppManager] Creating Editor...");
        editor.create();
        Serial.println("[AppManager] Editor created");
        
        Serial.println("[AppManager] Creating MenuManager...");
        menu_manager.create();
        Serial.println("[AppManager] MenuManager created");
        
        // Show file manager initially
        Serial.println("[AppManager] Showing FileManager...");
        showFileManager();
        Serial.println("[AppManager] Init complete");
    }
    
    void showFileManager() {
        if (current_mode != MODE_FILE_MANAGER) {
            editor.destroy();
            current_mode = MODE_FILE_MANAGER;
            file_manager.create([this](const char* name){ this->showEditor(String(name)); });
        }
        file_manager.show();
    }
    
    void showEditor(const String& filename) {
        if (current_mode != MODE_EDITOR) {
            file_manager.destroy();
            current_mode = MODE_EDITOR;
            editor.create();
        }
        
        current_filename = filename;
        editor.setTitle(filename);
        
        // Try to load file from SD card
        String content;
        if (sd_helper->readFile(filename.c_str(), content)) {
            editor.setText(content);
        } else {
            editor.setText(""); // New file
        }
        
        editor.show();
    }
    
    void update() {
        // Check menu actions
        MenuAction action = menu_manager.getLastAction();
        if (action != MENU_NONE) {
            Serial.print("[AppManager] Menu action: ");
            Serial.println((int)action);
            switch (action) {
                case MENU_SAVE:
                    handleSave();
                    break;
                case MENU_SAVE_AS:
                    handleSaveAs();
                    break;
                case MENU_READ_MODE:
                    handleReadMode();
                    break;
                case MENU_SERVE_AP:
                    handleServeAP();
                    break;
                case MENU_SERVE_BT:
                    handleServeBT();
                    break;
                case MENU_EXIT:
                    handleExit();
                    break;
                default:
                    break;
            }
            menu_manager.clearAction();
        }
    }
    
    void handleSave() {
        String content = editor.getText();
        if (sd_helper->writeFile(current_filename.c_str(), content)) {
            Serial.println("File saved: " + current_filename);
        } else {
            Serial.println("Save failed!");
        }
        menu_manager.toggle();
    }
    
    void handleSaveAs() {
        Serial.println("Save As not yet implemented");
        menu_manager.toggle();
    }
    
    void handleReadMode() {
        Serial.println("Read mode not yet implemented");
        menu_manager.toggle();
    }
    
    void handleServeAP() {
        Serial.println("AP server not yet implemented");
        menu_manager.toggle();
    }
    
    void handleServeBT() {
        Serial.println("BT server not yet implemented");
        menu_manager.toggle();
    }
    
    void handleExit() {
        showFileManager();
        menu_manager.toggle();
    }
    
    void toggleMenu() {
        menu_manager.toggle();
    }
    
    void toggleIME() {
        if (current_mode == MODE_EDITOR) {
            editor.toggleIME();
        }
    }
    
    static AppManager* getInstance() {
        if (!instance) {
            instance = new AppManager();
        }
        return instance;
    }
    
    AppMode getCurrentMode() const {
        return current_mode;
    }
};

AppManager* AppManager::instance = nullptr;

#endif

