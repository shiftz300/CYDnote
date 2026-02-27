#ifndef VFS_H
#define VFS_H

#include <Arduino.h>
#include <vector>

struct VFSFileInfo {
    String name;
    bool is_dir;
    size_t size;
};

class VFSFileImpl {
public:
    virtual ~VFSFileImpl() {}
    virtual size_t read(uint8_t* buf, size_t size) = 0;
    virtual int read() = 0;
    virtual size_t write(const uint8_t* buf, size_t size) = 0;
    virtual void close() = 0;
    virtual size_t size() = 0;
    virtual bool isOpen() = 0;
    virtual bool isDirectory() = 0;
    virtual bool available() = 0;
    virtual VFSFileImpl* openNextFile() = 0;
};

class VFSImpl {
public:
    virtual ~VFSImpl() {}
    virtual VFSFileImpl* open(const char* path, const char* mode) = 0;
    virtual bool exists(const char* path) = 0;
    virtual bool remove(const char* path) = 0;
    virtual bool rename(const char* pathFrom, const char* pathTo) = 0;
    virtual bool mkdir(const char* path) = 0;
    virtual bool rmdir(const char* path) = 0;
    virtual bool readDir(const char* path, std::vector<VFSFileInfo>& out_files) = 0;
    virtual uint64_t totalBytes() = 0;
    virtual uint64_t usedBytes() = 0;
};

class VFS {
public:
    static void registerFS(char drive_letter, VFSImpl* fs);
    static VFSImpl* getFS(char drive_letter);

    static VFSFileImpl* open(const String& vpath, const char* mode);
    static bool exists(const String& vpath);
    static bool remove(const String& vpath);
    static bool rename(const String& vpathFrom, const String& vpathTo);
    static bool mkdir(const String& vpath);
    static bool rmdir(const String& vpath);
    static bool readDir(const String& vpath, std::vector<VFSFileInfo>& out_files);
    static uint64_t totalBytes(char drive_letter);
    static uint64_t usedBytes(char drive_letter);

    static char getDrive(const String& vpath);
    static String getPath(const String& vpath);
};

#endif
