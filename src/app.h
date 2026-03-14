#ifndef APP_H
#define APP_H

#include <Arduino.h>
#include <LittleFS.h>
#include <lvgl.h>
#include <vector>
#include <algorithm>
#include "config.h"
#include "utils/format.h"
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
    static constexpr size_t READ_CHUNK_SIZE = 1536;
    static constexpr size_t MAX_UTF8_CONTINUATION_BYTES = 3;
    AppMode current_mode;
    FileManager file_manager;
    Editor editor;
    ImageViewer image_viewer;
    MenuManager menu_manager;
    SDHelper* sd_helper;
    ApShareService ap_share;
    String current_filename;
    std::vector<String> image_gallery;
    std::vector<size_t> read_chunk_offsets;
    int image_index;
    size_t read_chunk_index;
    size_t read_chunk_bytes;
    uint64_t read_chunk_file_size;

    // Static wrapper for LVGL callbacks
    static AppManager* instance;

public:
    AppManager() : current_mode(MODE_FILE_MANAGER), sd_helper(nullptr), image_index(-1), read_chunk_index(0), read_chunk_bytes(0), read_chunk_file_size(0) {
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
        editor.setReadChunkNavigationCallbacks(
            [this](){ this->showPrevReadChunk(); },
            [this](){ this->showNextReadChunk(); }
        );
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

        clearReadChunkState();
        current_filename = filename;
        editor.setTitle(filename);
        editor.setText("");
        editor.setReadChunkMode(false);
        editor.setReadChunkNavigationState(false, false);
        uint64_t file_size = 0;
        bool should_use_chunk_mode = getVirtualFileSize(filename, file_size) && file_size > READ_CHUNK_SIZE;
        if (should_use_chunk_mode) {
            read_chunk_file_size = file_size;
            read_chunk_offsets.push_back(0);
            read_chunk_index = 0;
            if (!loadReadChunkAtIndex(false)) {
                clearReadChunkState();
                editor.setReadChunkMode(false);
                editor.setReadChunkNavigationState(false, false);
            }
        }

        if (!editor.isReadChunkMode()) {
            String content;
            if (readVirtualFile(filename, content)) editor.setText(content);
        }

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

        bool save_success = false;
        if (editor.isReadChunkMode()) {
            // Need to save only the changed chunk
            save_success = updateVirtualFileChunk(current_filename, read_chunk_offsets[read_chunk_index], read_chunk_bytes, content);
            if (save_success) {
                // Update chunk bytes if length changed
                read_chunk_bytes = content.length();
                // We should re-calculate file size after patching
                uint64_t final_size = 0;
                getVirtualFileSize(current_filename, final_size);
                read_chunk_file_size = final_size;
                // Truncate cached offsets after the current one, since they have shifted
                if (read_chunk_index + 1 < read_chunk_offsets.size()) {
                    read_chunk_offsets.resize(read_chunk_index + 1);
                }
            }
        } else {
            save_success = writeVirtualFile(current_filename, content);
        }

        if (save_success) {
            uint64_t new_size = 0;
            bool has_new = getVirtualFileSize(current_filename, new_size);
            if (!has_new) new_size = (uint64_t)content.length();
            String from_h = had_old ? FormatUtil::formatBytesHuman(old_size) : "0 B";
            String to_h = FormatUtil::formatBytesHuman(new_size);
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
    void clearReadChunkState() {
        read_chunk_offsets.clear();
        read_chunk_index = 0;
        read_chunk_bytes = 0;
        read_chunk_file_size = 0;
    }

    void showPrevReadChunk() {
        if (!editor.isReadChunkMode()) return;
        if (read_chunk_index == 0) return;
        read_chunk_index--;
        loadReadChunkAtIndex(true);
    }

    void showNextReadChunk() {
        if (!editor.isReadChunkMode()) return;
        if (read_chunk_offsets.empty()) return;
        if (read_chunk_index >= read_chunk_offsets.size()) return;
        uint64_t current_offset64 = (uint64_t)read_chunk_offsets[read_chunk_index];
        if (read_chunk_bytes == 0 || current_offset64 >= read_chunk_file_size) return;
        if ((uint64_t)read_chunk_bytes > (read_chunk_file_size - current_offset64)) return;
        uint64_t next_offset64 = current_offset64 + (uint64_t)read_chunk_bytes;
        if (next_offset64 > (uint64_t)SIZE_MAX || next_offset64 >= read_chunk_file_size) return;
        size_t next_offset = (size_t)next_offset64;
        if (read_chunk_index + 1 < read_chunk_offsets.size()) {
            read_chunk_index++;
            loadReadChunkAtIndex(false);
            return;
        }
        read_chunk_offsets.push_back(next_offset);
        read_chunk_index = read_chunk_offsets.size() - 1;
        if (!loadReadChunkAtIndex(false)) {
            read_chunk_offsets.pop_back();
            read_chunk_index = read_chunk_offsets.empty() ? 0 : (read_chunk_offsets.size() - 1);
        }
    }

    bool loadReadChunkAtIndex(bool anchor_end) {
        if (read_chunk_offsets.empty() || read_chunk_index >= read_chunk_offsets.size()) return false;
        String content;
        size_t bytes_read = 0;
        size_t offset = read_chunk_offsets[read_chunk_index];
        if (!readVirtualFileChunk(current_filename, offset, READ_CHUNK_SIZE, content, bytes_read)) return false;
        if (bytes_read == 0 && read_chunk_file_size > 0) return false;
        trimIncompleteUtf8Tail(content, bytes_read, ((uint64_t)offset + (uint64_t)bytes_read) < read_chunk_file_size);
        if (bytes_read == 0 && read_chunk_file_size > 0) return false;

        read_chunk_bytes = bytes_read;
        bool has_prev = read_chunk_index > 0;
        bool has_next = ((uint64_t)offset + (uint64_t)read_chunk_bytes) < read_chunk_file_size;

        editor.setTitle(current_filename);
        editor.setReadChunkMode(true);
        editor.setReadChunkText(content, has_prev, has_next, anchor_end);
        return true;
    }

    // Return the expected UTF-8 codepoint width from a lead byte, or 0 if the lead byte is invalid.
    size_t utf8CodepointBytes(uint8_t lead) const {
        if ((lead & 0x80U) == 0) return 1;
        if (lead >= 0xC2U && lead <= 0xDFU) return 2;
        if ((lead & 0xF0U) == 0xE0U) return 3;
        if (lead >= 0xF0U && lead <= 0xF4U) return 4;
        return 0;
    }

    // Remove any incomplete UTF-8 sequence at the end of a chunk and keep bytes_read in sync with the trimmed content.
    void trimIncompleteUtf8Tail(String& content, size_t& bytes_read, bool has_more) {
        if (!has_more || bytes_read == 0) return;
        const char* data = content.c_str();
        if (!data) return;

        size_t end = bytes_read;
        size_t cont_bytes = 0;
        while (cont_bytes < end && cont_bytes < MAX_UTF8_CONTINUATION_BYTES) {
            uint8_t c = (uint8_t)data[end - 1 - cont_bytes];
            if ((c & 0xC0U) != 0x80U) break;
            cont_bytes++;
        }

        if (cont_bytes == 0) {
            size_t expected = utf8CodepointBytes((uint8_t)data[end - 1]);
            // Trim a lone multibyte lead byte that landed at the chunk boundary.
            if (expected > 1 && end > 0) end--;
        } else if (cont_bytes < end) {
            size_t lead_index = end - 1 - cont_bytes;
            size_t expected = utf8CodepointBytes((uint8_t)data[lead_index]);
            // Trim incomplete or invalid UTF-8 tail bytes before switching chunks.
            if (expected == 0 || expected != (cont_bytes + 1)) end = lead_index;
        } else {
            end = 0;
        }

        if (end < bytes_read) {
            content.remove(end);
            bytes_read = end;
        }
    }

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
                if (!out.concat(buf, static_cast<unsigned int>(n))) {
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

    bool readVirtualFileChunk(const String& vpath, size_t offset, size_t max_bytes, String& out, size_t& bytes_read) {
        bytes_read = 0;
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        if (drive == 'L') {
            File f = LittleFS.open(path.c_str(), "r");
            if (!f) return false;
            out = "";
            size_t file_size = (size_t)f.size();
            if (offset >= file_size) {
                f.close();
                return true;
            }
            if (!f.seek(offset, SeekSet)) {
                f.close();
                return false;
            }
            size_t target = file_size - offset;
            if (target > max_bytes) target = max_bytes;
            if (target > 0) out.reserve(target + 1);
            static constexpr size_t CHUNK = 512;
            char buf[CHUNK + 1];
            while (bytes_read < target) {
                size_t want = target - bytes_read;
                if (want > CHUNK) want = CHUNK;
                int n = f.read((uint8_t*)buf, want);
                if (n <= 0) break;
                buf[n] = '\0';
                if (!out.concat(buf, static_cast<unsigned int>(n))) {
                    f.close();
                    return false;
                }
                bytes_read += (size_t)n;
            }
            f.close();
            return true;
        }
        if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            return sd_helper->readFileChunk(path.c_str(), offset, max_bytes, out, bytes_read);
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

    bool updateVirtualFileChunk(const String& vpath, size_t chunk_offset, size_t chunk_old_len, const String& new_content) {
        char drive = driveOf(vpath);
        String path = innerPathOf(vpath);
        String tmp_path = path + ".tmp";
        
        if (drive == 'L') {
            File fin = LittleFS.open(path.c_str(), "r");
            if (!fin) return false;
            File fout = LittleFS.open(tmp_path.c_str(), "w");
            if (!fout) { fin.close(); return false; }
            
            size_t copied = 0;
            static constexpr size_t CHUNK = 512;
            uint8_t buf[CHUNK];
            while (copied < chunk_offset) {
                size_t want = chunk_offset - copied;
                if (want > CHUNK) want = CHUNK;
                int n = fin.read(buf, want);
                if (n <= 0) break;
                fout.write(buf, n);
                copied += n;
            }
            
            fout.write((const uint8_t*)new_content.c_str(), new_content.length());
            
            if (fin.seek(chunk_offset + chunk_old_len, SeekSet)) {
                while (true) {
                    int n = fin.read(buf, CHUNK);
                    if (n <= 0) break;
                    fout.write(buf, n);
                }
            }
            fin.close();
            fout.close();
            
            LittleFS.remove(path.c_str());
            LittleFS.rename(tmp_path.c_str(), path.c_str());
            return true;
        }
        if (drive == 'D' && sd_helper && sd_helper->isInitialized()) {
            FsFile fin = sd_helper->getFs().open(path.c_str(), O_RDONLY);
            if (!fin.isOpen()) return false;
            sd_helper->getFs().remove(tmp_path.c_str());
            FsFile fout = sd_helper->getFs().open(tmp_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            if (!fout.isOpen()) { fin.close(); return false; }
            
            size_t copied = 0;
            static constexpr size_t CHUNK = 512;
            uint8_t buf[CHUNK];
            while (copied < chunk_offset) {
                size_t want = chunk_offset - copied;
                if (want > CHUNK) want = CHUNK;
                int n = fin.read(buf, want);
                if (n <= 0) break;
                fout.write(buf, n);
                copied += n;
            }
            
            fout.write((const uint8_t*)new_content.c_str(), new_content.length());
            
            fin.seek(chunk_offset + chunk_old_len);
            while (true) {
                int n = fin.read(buf, CHUNK);
                if (n <= 0) break;
                fout.write(buf, n);
            }
            
            fin.close();
            fout.close();
            
            sd_helper->getFs().remove(path.c_str());
            sd_helper->getFs().rename(tmp_path.c_str(), path.c_str());
            return true;
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
        return (ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" || ext == "gif");
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
