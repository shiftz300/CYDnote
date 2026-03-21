#ifndef PROGRESS_H
#define PROGRESS_H

#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

struct ReadProgressRecord {
    uint8_t mode;          // 0: full text, 1: chunk mode
    uint32_t chunk_index;  // valid when mode == 1
    uint32_t cursor_pos;   // valid when mode == 0
    uint64_t file_size;
};

class ReadProgressStore {
private:
    static constexpr uint32_t TABLE_MAGIC = 0x43594450; // CYDP
    static constexpr uint16_t TABLE_VERSION = 1;
    static constexpr size_t MAX_RECORDS = 96;

    struct StoredProgressRecord {
        uint64_t hash;
        uint8_t mode;
        uint8_t reserved[3];
        uint32_t chunk_index;
        uint32_t cursor_pos;
        uint64_t file_size;
        uint32_t updated_seq;
    };

    struct StoredTable {
        uint32_t magic;
        uint16_t version;
        uint16_t count;
        uint32_t seq;
        StoredProgressRecord records[MAX_RECORDS];
    };

    Preferences prefs;
    bool ready;
    StoredTable table;

    static uint64_t fnv1a64(const String& s) {
        uint64_t h = 1469598103934665603ULL;
        for (size_t i = 0; i < s.length(); i++) {
            h ^= (uint8_t)s.charAt(i);
            h *= 1099511628211ULL;
        }
        return h;
    }

    static String normalizeVPath(const String& vpath) {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            char d = vpath.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            return String(d) + ":" + p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        return String("L:") + p;
    }

    uint64_t hashOfPath(const String& vpath) const {
        return fnv1a64(normalizeVPath(vpath));
    }

    int findIndex(uint64_t hash) const {
        size_t count = (table.count <= MAX_RECORDS) ? table.count : MAX_RECORDS;
        for (size_t i = 0; i < count; i++) {
            if (table.records[i].hash == hash) return (int)i;
        }
        return -1;
    }

    int findEvictIndex() const {
        if (table.count < MAX_RECORDS) return (int)table.count;
        size_t oldest = 0;
        uint32_t oldest_seq = table.records[0].updated_seq;
        for (size_t i = 1; i < MAX_RECORDS; i++) {
            if (table.records[i].updated_seq < oldest_seq) {
                oldest_seq = table.records[i].updated_seq;
                oldest = i;
            }
        }
        return (int)oldest;
    }

    bool saveTable() {
        if (!ready) return false;
        size_t wrote = prefs.putBytes("progress", &table, sizeof(table));
        return wrote == sizeof(table);
    }

public:
    ReadProgressStore() : ready(false), table{} {}

    bool begin() {
        if (ready) return true;
        if (!prefs.begin("cydnote", false)) return false;
        ready = true;

        memset(&table, 0, sizeof(table));
        table.magic = TABLE_MAGIC;
        table.version = TABLE_VERSION;
        table.count = 0;
        table.seq = 0;

        size_t got = prefs.getBytes("progress", &table, sizeof(table));
        if (got != sizeof(table) || table.magic != TABLE_MAGIC || table.version != TABLE_VERSION || table.count > MAX_RECORDS) {
            memset(&table, 0, sizeof(table));
            table.magic = TABLE_MAGIC;
            table.version = TABLE_VERSION;
            table.count = 0;
            table.seq = 0;
            saveTable();
        }
        return true;
    }

    bool load(const String& vpath, ReadProgressRecord& out) {
        if (!ready) {
            if (!begin()) return false;
        }
        uint64_t hash = hashOfPath(vpath);
        int idx = findIndex(hash);
        if (idx < 0) return false;
        const StoredProgressRecord& rec = table.records[idx];
        out.mode = rec.mode;
        out.chunk_index = rec.chunk_index;
        out.cursor_pos = rec.cursor_pos;
        out.file_size = rec.file_size;
        return true;
    }

    bool save(const String& vpath, const ReadProgressRecord& in) {
        if (!ready) {
            if (!begin()) return false;
        }
        uint64_t hash = hashOfPath(vpath);
        int idx = findIndex(hash);
        if (idx < 0) {
            idx = findEvictIndex();
            if ((size_t)idx == table.count && table.count < MAX_RECORDS) table.count++;
        }
        if (idx < 0 || (size_t)idx >= MAX_RECORDS) return false;

        table.seq++;
        StoredProgressRecord& rec = table.records[idx];
        rec.hash = hash;
        rec.mode = in.mode;
        rec.chunk_index = in.chunk_index;
        rec.cursor_pos = in.cursor_pos;
        rec.file_size = in.file_size;
        rec.updated_seq = table.seq;
        return saveTable();
    }
};

#endif
