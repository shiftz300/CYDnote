 #ifndef APP_MANAGER_H
#define APP_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
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
        sd_helper = SDHelper::getInstance();
        bool sd_ok = sd_helper && sd_helper->begin();
        if (!sd_ok) Serial.println("[SD] unavailable, D: disabled");
        
        // Create UI components
        file_manager.create(sd_ok, [this](const char* name){ this->showEditor(String(name)); });
        editor.create([this](){ this->showFileManager(); }, [this](){ this->handleSave(); });
        menu_manager.create();
        
        // Show file manager initially
        showFileManager();
    }
    
    void showFileManager() {
        if (current_mode != MODE_FILE_MANAGER) {
            current_mode = MODE_FILE_MANAGER;
            file_manager.show(LV_SCR_LOAD_ANIM_FADE_IN);
            return;
        }
        file_manager.show(LV_SCR_LOAD_ANIM_NONE);
    }
    
    void showEditor(const String& filename) {
        if (current_mode != MODE_EDITOR) {
            current_mode = MODE_EDITOR;
        }
        
        current_filename = filename;
        editor.setTitle(filename);
        
        // Load file by virtual drive path, e.g. L:/docs/a.txt or D:/notes.txt
        String content;
        if (readVirtualFile(filename, content)) {
            editor.setText(content);
        } else {
            editor.setText(""); // New file
        }

        editor.show(LV_SCR_LOAD_ANIM_FADE_IN);
    }
    
    void update() {
        // Check menu actions
        MenuAction action = menu_manager.getLastAction();
        if (action != MENU_NONE) {
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
        if (current_filename.isEmpty()) {
            current_filename = "L:/note.txt";
        }
        String content = editor.getText();
        if (writeVirtualFile(current_filename, content)) {
            Serial.println("File saved: " + current_filename);
        } else {
            Serial.println("Save failed!");
        }
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

private:
    bool readVirtualFile(const String& vpath, String& out) {
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        if (drive == 'L') {
            File f = LittleFS.open(path.c_str(), "r");
            if (!f) return false;
            out = "";
            while (f.available()) {
                int c = f.read();
                if (c < 0) break;
                out += (char)c;
            }
            f.close();
            return true;
        }
        if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            return sd_helper->readFile(path.c_str(), out);
        }
        return false;
    }

    bool writeVirtualFile(const String& vpath, const String& data) {
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        if (drive == 'L') {
            String parent = parentPath(path);
            if (parent.length() > 0 && !LittleFS.exists(parent.c_str())) {
                LittleFS.mkdir(parent.c_str());
            }
            File f = LittleFS.open(path.c_str(), "w");
            if (!f) return false;
            size_t w = f.write((const uint8_t*)data.c_str(), data.length());
            f.close();
            return w == data.length();
        }
        if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            return sd_helper->writeFile(path.c_str(), data);
        }
        return false;
    }

    char driveOf(const String& vpath) const {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            char d = vpath.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
            return d;
        }
        return 'L';
    }

    String innerPathOf(const String& vpath) const {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            return p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        return p;
    }

    String parentPath(const String& path) const {
        if (path == "/") return "";
        int idx = path.lastIndexOf('/');
        if (idx <= 0) return "/";
        return path.substring(0, idx);
    }
};

AppManager* AppManager::instance = nullptr;

#endif

