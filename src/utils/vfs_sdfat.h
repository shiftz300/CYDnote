#ifndef VFS_SDFAT_H
#define VFS_SDFAT_H

#include "vfs.h"
#include "sd_helper.h"

class SdFatFileImpl : public VFSFileImpl {
    FsFile file;
public:
    SdFatFileImpl() {}
    SdFatFileImpl(FsFile&& f) : file(std::move(f)) {}
    ~SdFatFileImpl() { if (file.isOpen()) file.close(); }

    size_t read(uint8_t* buf, size_t size) override {
        int n = file.read(buf, size);
        return n > 0 ? n : 0;
    }
    int read() override { return file.read(); }
    size_t write(const uint8_t* buf, size_t size) override {
        int n = file.write(buf, size);
        return n > 0 ? n : 0;
    }
    void close() override { file.close(); }
    size_t size() override { return file.fileSize(); }
    bool isOpen() override { return file.isOpen(); }
    bool isDirectory() override { return file.isDir(); }
    bool available() override { return file.available(); }

    VFSFileImpl* openNextFile() override {
        FsFile next;
        if (!next.openNext(&file, O_RDONLY)) return nullptr;
        return new SdFatFileImpl(std::move(next));
    }
};

class SdFatImpl : public VFSImpl {
    StorageHelper* helper;
public:
    SdFatImpl(StorageHelper* h) : helper(h) {}

    VFSFileImpl* open(const char* path, const char* mode) override {
        if (!helper || !helper->isInitialized()) return nullptr;
        oflag_t flags = O_RDONLY;
        if (mode && mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
        else if (mode && mode[0] == 'a') flags = O_WRONLY | O_CREAT | O_APPEND;

        FsFile f = helper->getFs().open(path, flags);
        if (!f.isOpen()) return nullptr;
        return new SdFatFileImpl(std::move(f));
    }

    bool exists(const char* path) override {
        return helper && helper->isInitialized() && helper->getFs().exists(path);
    }
    bool remove(const char* path) override {
        return helper && helper->isInitialized() && helper->getFs().remove(path);
    }
    bool rename(const char* pathFrom, const char* pathTo) override {
        return helper && helper->isInitialized() && helper->getFs().rename(pathFrom, pathTo);
    }
    bool mkdir(const char* path) override {
        return helper && helper->isInitialized() && helper->getFs().mkdir(path, true);
    }
    bool rmdir(const char* path) override {
        return helper && helper->isInitialized() && helper->getFs().rmdir(path);
    }

    bool readDir(const char* path, std::vector<VFSFileInfo>& out_files) override {
        if (!helper || !helper->isInitialized()) return false;
        FsFile dir = helper->getFs().open(path, O_RDONLY);
        if (!dir.isOpen() || !dir.isDir()) return false;

        FsFile entry;
        while (entry.openNext(&dir, O_RDONLY)) {
            char name[256];
            entry.getName(name, sizeof(name));
            if (name[0] != '\0') {
                VFSFileInfo info;
                info.name = String(name);
                info.is_dir = entry.isDir();
                info.size = entry.fileSize();
                out_files.push_back(info);
            }
            entry.close();
        }
        dir.close();
        return true;
    }

    uint64_t totalBytes() override { return helper ? helper->totalBytes() : 0; }
    uint64_t usedBytes() override { return helper ? helper->usedBytes() : 0; }
};

#endif
