#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#ifndef DISABLE_FS_H_WARNING
#define DISABLE_FS_H_WARNING
#endif
#include <SdFat.h>
#include <SdCard/SdCardInfo.h>
#include "../config.h"

// StorageHelper - SD card file operations wrapper for CYDnote
class StorageHelper {
private:
    SdFs sd;
    bool initialized;
    static StorageHelper* instance;
    
public:
    StorageHelper() : initialized(false) {}
    
    static StorageHelper* getInstance() {
        if (!instance) {
            instance = new StorageHelper();
        }
        return instance;
    }
    
    bool begin() {
        if (initialized) return true;
        pinMode(SD_CS, OUTPUT);
        digitalWrite(SD_CS, HIGH);

        uint32_t cfg_mhz = SD_SPI_SPEED / 1000000UL;
        if (cfg_mhz < 4) cfg_mhz = 4;
        if (cfg_mhz > 40) cfg_mhz = 40;
        if (sd.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(cfg_mhz), &SPI))) {
            initialized = true;
            Serial.print("[SD] mounted @ ");
            Serial.print(cfg_mhz);
            Serial.println("MHz");
            return true;
        }

#if SD_ALLOW_FALLBACK_SPEEDS
        const uint32_t fallback_speeds[] = {25, 12, 8, 4};
        for (uint8_t i = 0; i < (sizeof(fallback_speeds) / sizeof(fallback_speeds[0])); i++) {
            uint32_t mhz = fallback_speeds[i];
            if (mhz == cfg_mhz) continue;
            if (sd.begin(SdSpiConfig(SD_CS, DEDICATED_SPI, SD_SCK_MHZ(mhz), &SPI))) {
                initialized = true;
                Serial.print("[SD] mounted @ ");
                Serial.print(mhz);
                Serial.println("MHz (fallback)");
                return true;
            }
        }
#endif

        Serial.print("[SD] Init failed. Error code: 0x");
        Serial.println(sd.sdErrorCode(), HEX);
        Serial.print("[SD] Error data: 0x");
        Serial.println(sd.sdErrorData(), HEX);
        return false;
    }
    
    // Expose underlying SdFs for advanced operations
    SdFs& getFs() { return sd; }
    
    bool isInitialized() const { return initialized; }
    
    bool fileExists(const char* path) {
        if (!initialized) return false;
        return sd.exists(path);
    }
    
    bool readFile(const char* path, String& content) {
        if (!initialized) return false;

        content = "";
        FsFile file = sd.open(normalizePath(path).c_str(), O_RDONLY);
        if (!file.isOpen()) {  // Check if file is actually open
            Serial.print("Failed to open file: ");
            Serial.println(path);
            return false;
        }
        
        uint32_t fsz = (uint32_t)file.fileSize();
        if (fsz > 0) content.reserve(fsz + 1);

        static constexpr size_t CHUNK = 1024;
        char buf[CHUNK + 1];
        while (true) {
            int n = file.read(buf, CHUNK);
            if (n <= 0) break;
            buf[n] = '\0';
            if (!content.concat(buf, (unsigned int)n)) {
                file.close();
                return false;
            }
        }
        file.close();
        return true;
    }
    
    bool writeFile(const char* path, const String& content) {
        if (!initialized) return false;

        String norm = normalizePath(path);
        String parent = parentPath(norm);
        if (parent.length() > 0 && !sd.exists(parent.c_str())) {
            sd.mkdir(parent.c_str(), true);
        }

        FsFile file = sd.open(norm.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
        if (!file.isOpen()) {
            Serial.print("Failed to open file for write: ");
            Serial.println(path);
            return false;
        }

        size_t expected = content.length();
        size_t written = file.write((const uint8_t*)content.c_str(), expected);
        file.close();
        return written == expected;
    }
    
    bool deleteFile(const char* path) {
        if (!initialized) return false;
        return sd.remove(normalizePath(path).c_str());
    }

    uint64_t totalBytes() {
        if (!initialized || !sd.vol()) return 0;
        return (uint64_t)sd.vol()->bytesPerCluster() * (uint64_t)sd.vol()->clusterCount();
    }

    uint64_t usedBytes() {
        if (!initialized || !sd.vol()) return 0;
        uint64_t total = totalBytes();
        int32_t free_clusters = sd.freeClusterCount();
        if (free_clusters < 0) return 0;
        uint64_t free_bytes = (uint64_t)sd.vol()->bytesPerCluster() * (uint64_t)free_clusters;
        return (total > free_bytes) ? (total - free_bytes) : 0;
    }

    uint64_t freeBytes() {
        uint64_t total = totalBytes();
        uint64_t used = usedBytes();
        return (total > used) ? (total - used) : 0;
    }

    uint32_t sectorCount() {
        if (!initialized || !sd.card()) return 0;
        return (uint32_t)sd.card()->sectorCount();
    }

    String fsTypeString() {
        if (!initialized) return "N/A";
        uint8_t t = sd.fatType();
        if (t == FAT_TYPE_EXFAT) return "exFAT";
        if (t == FAT_TYPE_FAT32) return "FAT32";
        if (t == FAT_TYPE_FAT16) return "FAT16";
        if (t == FAT_TYPE_FAT12) return "FAT12";
        return "Unknown";
    }

    String cardTypeString() {
        if (!initialized || !sd.card()) return "N/A";
        uint8_t t = sd.card()->type();
        if (t == SD_CARD_TYPE_SDHC) return "SDHC/SDXC";
        if (t == SD_CARD_TYPE_SD2) return "SDSC v2";
        if (t == SD_CARD_TYPE_SD1) return "SDSC v1";
        return "Unknown";
    }

private:
    String normalizePath(const char* p) {
        if (!p || p[0] == '\0') return "/";
        String path = String(p);
        if (!path.startsWith("/")) path = "/" + path;
        return path;
    }

    String parentPath(const String& p) {
        if (p == "/") return "";
        int idx = p.lastIndexOf('/');
        if (idx <= 0) return "/";
        return p.substring(0, idx);
    }
};

StorageHelper* StorageHelper::instance = nullptr;

#endif
