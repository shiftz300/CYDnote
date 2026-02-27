#ifndef VFS_LITTLEFS_H
#define VFS_LITTLEFS_H

#include "vfs.h"
#include <LittleFS.h>

class LittleFSFileImpl : public VFSFileImpl {
    File file;
public:
    LittleFSFileImpl(File f) : file(f) {}
    ~LittleFSFileImpl() { if (file) file.close(); }

    size_t read(uint8_t* buf, size_t size) override { return file.read(buf, size); }
    int read() override { return file.read(); }
    size_t write(const uint8_t* buf, size_t size) override { return file.write(buf, size); }
    void close() override { file.close(); }
    size_t size() override { return file.size(); }
    bool isOpen() override { return file == true; }
    bool isDirectory() override { return file.isDirectory(); }
    bool available() override { return file.available(); }

    VFSFileImpl* openNextFile() override {
        File next = file.openNextFile();
        if (!next) return nullptr;
        return new LittleFSFileImpl(next);
    }
};

class LittleFSImpl : public VFSImpl {
public:
    VFSFileImpl* open(const char* path, const char* mode) override {
        File f = LittleFS.open(path, mode);
        if (!f) return nullptr;
        return new LittleFSFileImpl(f);
    }

    bool exists(const char* path) override { return LittleFS.exists(path); }
    bool remove(const char* path) override { return LittleFS.remove(path); }
    bool rename(const char* pathFrom, const char* pathTo) override { return LittleFS.rename(pathFrom, pathTo); }
    bool mkdir(const char* path) override { return LittleFS.mkdir(path); }
    bool rmdir(const char* path) override { return LittleFS.rmdir(path); }

    bool readDir(const char* path, std::vector<VFSFileInfo>& out_files) override {
        File dir = LittleFS.open(path, "r");
        if (!dir || !dir.isDirectory()) return false;

        while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;

            VFSFileInfo info;
            String name = entry.name();
            int lastSlash = name.lastIndexOf('/');
            info.name = (lastSlash >= 0) ? name.substring(lastSlash + 1) : name;
            info.is_dir = entry.isDirectory();
            info.size = entry.size();
            out_files.push_back(info);
            entry.close();
        }
        dir.close();
        return true;
    }

    uint64_t totalBytes() override { return LittleFS.totalBytes(); }
    uint64_t usedBytes() override { return LittleFS.usedBytes(); }
};

#endif
