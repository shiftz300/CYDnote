#ifndef SD_HELPER_H
#define SD_HELPER_H

#include <Arduino.h> 
#include <SdFat.h>

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
        
        // Initialize SD with explicit SPI configuration to avoid conflicts
        // Use HSPI (same as TFT) but with separate CS pin (5)
        // Note: SdFat defaults to use VSPI unless configured, so CS pin helps differentiate
        
        // Try init with CS pin and clock frequency
        // SdFat will auto-select SPI bus based on pin availability
        if (!sd.begin(SD_CS, SD_SCK_MHZ(25))) {
            Serial.print("SD init failed. Error code: 0x");
            Serial.println(sd.sdErrorCode(), HEX);
            Serial.print("Error data: 0x");
            Serial.println(sd.sdErrorData(), HEX);
            
            if (sd.sdErrorCode() == SD_CARD_ERROR_CMD0) {
                Serial.println("  -> Check CS pin, wiring, or power");
            } else if (sd.sdErrorCode() == SD_CARD_ERROR_ACMD41) {
                Serial.println("  -> SD card not detected");
            }
            return false;
        }
        
        Serial.println("SD card initialized successfully");
        initialized = true;
        return true;
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
        
        FsFile file = sd.open(path, O_RDONLY);
        if (!file.isOpen()) {  // Check if file is actually open
            Serial.print("Failed to open file: ");
            Serial.println(path);
            return false;
        }
        
        while (file.available()) {
            int c = file.read();
            if (c < 0) break;
            content += (char)c;
        }
        file.close();
        return true;
    }
    
    bool writeFile(const char* path, const String& content) {
        if (!initialized) return false;
        
        FsFile file = sd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
        if (!file.isOpen()) {
            Serial.print("Failed to open file for write: ");
            Serial.println(path);
            return false;
        }
        
        file.write((uint8_t*)content.c_str(), content.length());
        file.close();
        return true;
    }
    
    bool deleteFile(const char* path) {
        if (!initialized) return false;
        return sd.remove(path);
    }
};

StorageHelper* StorageHelper::instance = nullptr;

#endif
