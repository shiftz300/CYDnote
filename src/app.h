#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include <vector>
#include <algorithm>
#include "config.h"
#include "utils/storage.h"
#include "utils/share.h"

// For readability in AppManager context
using SDHelper = StorageHelper;
#include "ui/files.h"
#include "ui/editor.h"
#include "ui/viewer.h"
#include "ui/menu.h"

class AppManager {
private:
    static constexpr size_t IMAGE_GALLERY_MAX_ITEMS = 128;
    static constexpr size_t IMAGE_GALLERY_SCAN_ENTRY_LIMIT = 80;
    static constexpr size_t IMAGE_GALLERY_SCAN_IMAGE_LIMIT = 64;
    AppMode current_mode;
    FileManager file_manager;
    Editor editor;
    ImageViewer image_viewer;
    MenuManager menu_manager;
    SDHelper* sd_helper;
    ApShareService ap_share;
    String current_filename;
    std::vector<String> image_gallery;
    int image_index;
    
    // Static wrapper for LVGL callbacks
    static AppManager* instance;
    
public:
    AppManager() : current_mode(MODE_FILE_MANAGER), sd_helper(nullptr), image_index(-1) {
        instance = this;
    }
    
    void init() {
        sd_helper = SDHelper::getInstance();
        bool sd_ok = sd_helper && sd_helper->begin();
        if (!sd_ok) Serial.println("[SD] unavailable, D: disabled");
        ap_share.init(sd_helper);
        
        // Create UI components
        file_manager.create(
            sd_ok,
            [this](const char* name){
                String path = String(name ? name : "");
                if (isImageFile(path)) this->showImage(path);
                else this->showEditor(path);
            },
            [this]() -> bool { return this->ap_share.toggle(); },
            [this]() -> bool { return this->ap_share.isRunning(); },
            [this]() -> String { return this->ap_share.statusString(); }
        );
        editor.create([this](){ this->showFileManager(); }, [this](){ this->handleSave(); });
        image_viewer.create(
            [this](){ this->showFileManager(); },
            [this](){ this->showPrevImage(); },
            [this](){ this->showNextImage(); }
        );
        menu_manager.create();
        
        // Show file manager initially
        showFileManager();
    }
    
    void showFileManager() {
        clearImageGalleryCache();
        if (current_mode != MODE_FILE_MANAGER) {
            current_mode = MODE_FILE_MANAGER;
            file_manager.show(LV_SCR_LOAD_ANIM_FADE_IN);
            return;
        }
        file_manager.show(LV_SCR_LOAD_ANIM_NONE);
    }
    
    void showEditor(const String& filename) {
        showEditorRaw(filename);
    }

private:
    void showEditorRaw(const String& filename) {
        clearImageGalleryCache();
        if (current_mode != MODE_EDITOR) {
            current_mode = MODE_EDITOR;
        }
        
        current_filename = filename;
        editor.setTitle(filename);
        editor.setText("");
        String content;
        if (readVirtualFile(filename, content)) editor.setText(content);

        editor.show(LV_SCR_LOAD_ANIM_FADE_IN);
    }

public:
    void showImage(const String& filename) {
        if (current_mode != MODE_IMAGE_VIEWER) {
            current_mode = MODE_IMAGE_VIEWER;
        }
        image_viewer.setTitle(filename);
        image_viewer.setImage(filename);
        buildImageGallery(filename);
        image_viewer.show(LV_SCR_LOAD_ANIM_NONE);
    }
    
    void update() {
        // Avoid AP share FS polling while heavy file operations are running.
        if (!file_manager.isFsBusy()) ap_share.update();

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
        uint64_t old_size = 0;
        bool had_old = getVirtualFileSize(current_filename, old_size);
        if (writeVirtualFile(current_filename, content)) {
            uint64_t new_size = 0;
            bool has_new = getVirtualFileSize(current_filename, new_size);
            if (!has_new) new_size = (uint64_t)content.length();
            String from_h = had_old ? formatBytesHuman(old_size) : "0 B";
            String to_h = formatBytesHuman(new_size);
            editor.showSaveSuccessPopup(fileNameOf(current_filename), from_h, to_h);
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
        bool running = toggleShareApService();
        Serial.println(running ? "[ShareAP] started" : "[ShareAP] stopped");
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

    bool isBusy() const {
        return file_manager.isFsBusy();
    }

private:
    bool readVirtualFile(const String& vpath, String& out) {
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        if (drive == 'L') {
            File f = LittleFS.open(path.c_str(), "r");
            if (!f) return false;
            out = "";
            size_t file_size = (size_t)f.size();
            if (file_size > 0) out.reserve(file_size + 1);
            static constexpr size_t CHUNK = 1024;
            char buf[CHUNK + 1];
            while (true) {
                int n = f.read((uint8_t*)buf, CHUNK);
                if (n <= 0) break;
                buf[n] = '\0';
                if (!out.concat(buf, (unsigned int)n)) {
                    f.close();
                    return false;
                }
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

    bool getVirtualFileSize(const String& vpath, uint64_t& out_size) {
        out_size = 0;
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        if (drive == 'L') {
            File f = LittleFS.open(path.c_str(), "r");
            if (!f) return false;
            out_size = (uint64_t)f.size();
            f.close();
            return true;
        }
        if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            FsFile f = sd_helper->getFs().open(path.c_str(), O_RDONLY);
            if (!f.isOpen()) return false;
            out_size = (uint64_t)f.fileSize();
            f.close();
            return true;
        }
        return false;
    }

    String formatBytesHuman(uint64_t bytes) const {
        char buf[32];
        if (bytes >= (1024ULL * 1024ULL * 1024ULL)) {
            float v = (float)bytes / (1024.0f * 1024.0f * 1024.0f);
            snprintf(buf, sizeof(buf), "%.2f GB", v);
        } else if (bytes >= (1024ULL * 1024ULL)) {
            float v = (float)bytes / (1024.0f * 1024.0f);
            snprintf(buf, sizeof(buf), "%.2f MB", v);
        } else if (bytes >= 1024ULL) {
            float v = (float)bytes / 1024.0f;
            snprintf(buf, sizeof(buf), "%.1f KB", v);
        } else {
            snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
        }
        return String(buf);
    }

    bool toggleShareApService() {
        return ap_share.toggle();
    }

    String shareApStatusString() const {
        return ap_share.statusString();
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
        char d = driveOf(vpath);
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            if (d == 'L') {
                if (p == "/littlefs") return "/";
                if (p.startsWith("/littlefs/")) p = p.substring(9);
            } else if (d == 'D') {
                if (p == "/sd") return "/";
                if (p.startsWith("/sd/")) p = p.substring(3);
            }
            return p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        if (p == "/littlefs") return "/";
        if (p.startsWith("/littlefs/")) p = p.substring(9);
        return p;
    }

    String parentPath(const String& path) const {
        if (path == "/") return "";
        int idx = path.lastIndexOf('/');
        if (idx <= 0) return "/";
        return path.substring(0, idx);
    }

    bool isImageFile(const String& path) const {
        int dot = path.lastIndexOf('.');
        if (dot < 0) return false;
        String ext = path.substring(dot + 1);
        ext.toLowerCase();
        return (ext == "jpg" || ext == "jpeg");
    }

    void showPrevImage() {
        if (image_gallery.empty()) return;
        if (image_index < 0) image_index = 0;
        image_index = (image_index - 1 + (int)image_gallery.size()) % (int)image_gallery.size();
        const String& p = image_gallery[(size_t)image_index];
        image_viewer.setTitle(p);
        image_viewer.setImage(p);
    }

    void showNextImage() {
        if (image_gallery.empty()) return;
        if (image_index < 0) image_index = 0;
        image_index = (image_index + 1) % (int)image_gallery.size();
        const String& p = image_gallery[(size_t)image_index];
        image_viewer.setTitle(p);
        image_viewer.setImage(p);
    }

    String fileNameOf(const String& path) const {
        int idx = path.lastIndexOf('/');
        if (idx >= 0 && idx < (int)path.length() - 1) return path.substring(idx + 1);
        return path;
    }

    String buildVPath(char drive, const String& inner) const {
        String p = inner;
        if (!p.startsWith("/")) p = "/" + p;
        return String((char)drive) + ":" + p;
    }

    void buildImageGallery(const String& anchor_vpath) {
        image_gallery.clear();
        image_gallery.reserve(48);
        image_gallery.push_back(anchor_vpath);
        image_index = 0;

        char drive = driveOf(anchor_vpath);
        String inner = innerPathOf(anchor_vpath);
        String dir = parentPath(inner);
        if (dir.length() == 0) dir = "/";

        size_t scanned = 0;

        if (drive == 'L') {
            File d = LittleFS.open(dir.c_str(), "r");
            if (d) {
                while (true) {
                    File e = d.openNextFile();
                    if (!e) break;
                    scanned++;
                    if (e.isDirectory()) {
                        e.close();
                        if (scanned >= IMAGE_GALLERY_SCAN_ENTRY_LIMIT) break;
                        continue;
                    }
                    String name = String(e.name());
                    e.close();
                    if (!isImageFile(name)) continue;
                    String full = dir;
                    if (!full.endsWith("/")) full += "/";
                    full += name;
                    String vp = buildVPath('L', full);
                    if (vp != anchor_vpath) image_gallery.push_back(vp);
                    if (image_gallery.size() >= IMAGE_GALLERY_SCAN_IMAGE_LIMIT) break;
                    if (scanned >= IMAGE_GALLERY_SCAN_ENTRY_LIMIT) break;
                }
                d.close();
            }
        } else if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            FsFile d = sd_helper->getFs().open(dir.c_str(), O_RDONLY);
            if (d.isOpen()) {
                FsFile e;
                while (e.openNext(&d, O_RDONLY)) {
                    scanned++;
                    bool is_dir = e.isDir();
                    char name_buf[128];
                    name_buf[0] = '\0';
                    e.getName(name_buf, sizeof(name_buf) - 1);
                    name_buf[sizeof(name_buf) - 1] = '\0';
                    e.close();
                    if (is_dir) {
                        if (scanned >= IMAGE_GALLERY_SCAN_ENTRY_LIMIT) break;
                        continue;
                    }
                    String name = String(name_buf);
                    if (!isImageFile(name)) continue;
                    String full = dir;
                    if (!full.endsWith("/")) full += "/";
                    full += name;
                    String vp = buildVPath('D', full);
                    if (vp != anchor_vpath) image_gallery.push_back(vp);
                    if (image_gallery.size() >= IMAGE_GALLERY_SCAN_IMAGE_LIMIT) break;
                    if (scanned >= IMAGE_GALLERY_SCAN_ENTRY_LIMIT) break;
                }
                d.close();
            }
        }
        if (image_gallery.size() > IMAGE_GALLERY_MAX_ITEMS) {
            image_gallery.resize(IMAGE_GALLERY_MAX_ITEMS);
        }
    }

    void clearImageGalleryCache() {
        if (image_gallery.empty()) {
            image_index = -1;
            return;
        }
        std::vector<String>().swap(image_gallery);
        image_index = -1;
    }
};

AppManager* AppManager::instance = nullptr;

#endif

