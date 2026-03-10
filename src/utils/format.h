#ifndef UTILS_FORMAT_H
#define UTILS_FORMAT_H

#include <Arduino.h>

namespace FormatUtil {

inline String formatBytesHuman(uint64_t bytes) {
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

} // namespace FormatUtil

#endif