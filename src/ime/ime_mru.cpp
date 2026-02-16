#include "ime_mru.h"

#include <LittleFS.h>
#include <string.h>
#include <stdlib.h>

ImeMru& ImeMru::getInstance() {
    static ImeMru inst;
    return inst;
}

ImeMru::ImeMru() : inited(false), counter(0) {
    memset(items, 0, sizeof(items));
    memset(panel_cache, 0, sizeof(panel_cache));
}

void ImeMru::init() {
    if (inited) return;
    inited = true;
    // Ensure save file exists on first boot to avoid repeated "not exist" errors.
    File ensure = LittleFS.open(SAVE_PATH, "a");
    if (ensure) ensure.close();
    load();
}

int ImeMru::findItem(uint32_t cp) const {
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].cp == cp) return i;
    }
    return -1;
}

int ImeMru::findReplaceSlot() const {
    int empty = -1;
    int worst = 0;
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].cp == 0) {
            empty = i;
            break;
        }
        if (items[i].score < items[worst].score) worst = i;
        else if (items[i].score == items[worst].score && items[i].order < items[worst].order) worst = i;
    }
    return (empty >= 0) ? empty : worst;
}

uint16_t ImeMru::scoreOf(uint32_t cp) const {
    int idx = findItem(cp);
    return idx >= 0 ? items[idx].score : 0;
}

void ImeMru::load() {
    File f = LittleFS.open(SAVE_PATH, "r");
    if (!f) return;

    memset(items, 0, sizeof(items));
    counter = 0;

    char line[96];
    int idx = 0;
    while (f.available()) {
        size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[n] = '\0';
        if (n == 0) continue;
        if (line[0] == '\r' || line[0] == '\n') continue;

        // Optional first line: #counter,<value>
        if (line[0] == '#') {
            if (strncmp(line, "#counter,", 9) == 0) {
                long c = strtol(line + 9, nullptr, 10);
                if (c >= 0 && c <= 0xFFFF) counter = (uint16_t)c;
            }
            continue;
        }

        char* p1 = strchr(line, ',');
        if (!p1) continue;
        *p1 = '\0';
        char* p2 = strchr(p1 + 1, ',');
        if (!p2) continue;
        *p2 = '\0';

        long cp = strtol(line, nullptr, 10);
        long score = strtol(p1 + 1, nullptr, 10);
        long order = strtol(p2 + 1, nullptr, 10);

        if (cp <= 0 || cp > 0x10FFFF) continue;
        if (score < 0 || score > 0xFFFF) continue;
        if (order < 0 || order > 0xFFFF) continue;
        if (idx >= MAX_ITEMS) break;

        items[idx].cp = (uint32_t)cp;
        items[idx].score = (uint16_t)score;
        items[idx].order = (uint16_t)order;
        idx++;
    }
    f.close();
}

void ImeMru::save() const {
    File f = LittleFS.open(SAVE_PATH, "w");
    if (!f) return;

    f.print("#counter,");
    f.println((unsigned int)counter);
    for (int i = 0; i < MAX_ITEMS; i++) {
        if (items[i].cp == 0) continue;
        f.print((unsigned long)items[i].cp);
        f.print(',');
        f.print((unsigned int)items[i].score);
        f.print(',');
        f.println((unsigned int)items[i].order);
    }
    f.close();
}

uint32_t ImeMru::utf8ToCodepoint(const char* s, uint8_t& len_out) {
    len_out = 0;
    if (!s || !s[0]) return 0;
    const uint8_t b0 = (uint8_t)s[0];
    if ((b0 & 0x80) == 0) { len_out = 1; return b0; }
    if ((b0 & 0xE0) == 0xC0) {
        len_out = 2;
        return ((uint32_t)(b0 & 0x1F) << 6) | ((uint8_t)s[1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0) {
        len_out = 3;
        return ((uint32_t)(b0 & 0x0F) << 12) | (((uint8_t)s[1] & 0x3F) << 6) | ((uint8_t)s[2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0) {
        len_out = 4;
        return ((uint32_t)(b0 & 0x07) << 18) | (((uint8_t)s[1] & 0x3F) << 12) |
               (((uint8_t)s[2] & 0x3F) << 6) | ((uint8_t)s[3] & 0x3F);
    }
    return 0;
}

bool ImeMru::isArrowOrEmpty(const char* txt) {
    if (!txt || txt[0] == '\0') return true;
    if (strcmp(txt, "<") == 0 || strcmp(txt, ">") == 0) return true;
    if (strcmp(txt, LV_SYMBOL_LEFT) == 0 || strcmp(txt, LV_SYMBOL_RIGHT) == 0) return true;
    if (strcmp(txt, " ") == 0) return true;
    return false;
}

bool ImeMru::isCandidateChar(const char* txt, uint32_t& cp_out) {
    cp_out = 0;
    if (isArrowOrEmpty(txt)) return false;
    uint8_t len = 0;
    uint32_t cp = utf8ToCodepoint(txt, len);
    if (cp == 0 || len == 0) return false;
    if (txt[len] != '\0') return false; // only single-char candidate
    // Keep only CJK for now
    if (cp < 0x4E00 || cp > 0x9FAF) return false;
    cp_out = cp;
    return true;
}

void ImeMru::learnFromUtf8(const char* utf8_char) {
    init();
    uint32_t cp = 0;
    if (!isCandidateChar(utf8_char, cp)) return;

    int idx = findItem(cp);
    if (idx < 0) {
        idx = findReplaceSlot();
        items[idx].cp = cp;
        items[idx].score = 1;
    } else if (items[idx].score < 0xFFFF) {
        items[idx].score++;
    }
    counter++;
    items[idx].order = counter;
    save();
}

ImeMru::PanelCache* ImeMru::getPanelCache(lv_obj_t* panel) {
    for (int i = 0; i < 2; i++) {
        if (panel_cache[i].used && panel_cache[i].panel == panel) return &panel_cache[i];
    }
    for (int i = 0; i < 2; i++) {
        if (!panel_cache[i].used) {
            panel_cache[i].used = true;
            panel_cache[i].panel = panel;
            return &panel_cache[i];
        }
    }
    panel_cache[0].panel = panel;
    return &panel_cache[0];
}

void ImeMru::applyToCandidatePanel(lv_obj_t* cand_panel) {
    init();
    if (!cand_panel) return;
    const char* const* map = lv_buttonmatrix_get_map(cand_panel);
    if (!map) return;

    // Build list length
    int map_len = 0;
    while (map[map_len] && map[map_len][0] != '\0' && map_len < 15) map_len++;
    if (map_len <= 0) return;

    struct CandInfo {
        int idx;
        uint16_t score;
        char txt[8];
    } cands[12];
    int cand_cnt = 0;
    bool changed = false;

    for (int i = 0; i < map_len; i++) {
        uint32_t cp = 0;
        if (!isCandidateChar(map[i], cp)) continue;
        if (cand_cnt >= 12) break;
        cands[cand_cnt].idx = i;
        cands[cand_cnt].score = scoreOf(cp);
        strncpy(cands[cand_cnt].txt, map[i], sizeof(cands[cand_cnt].txt) - 1);
        cands[cand_cnt].txt[sizeof(cands[cand_cnt].txt) - 1] = '\0';
        cand_cnt++;
    }
    if (cand_cnt <= 1) return;

    // Sort by score desc (stable-ish bubble due to very small N)
    for (int i = 0; i < cand_cnt - 1; i++) {
        for (int j = 0; j < cand_cnt - i - 1; j++) {
            if (cands[j].score < cands[j + 1].score) {
                CandInfo t = cands[j];
                cands[j] = cands[j + 1];
                cands[j + 1] = t;
            }
        }
    }

    // Copy original map
    PanelCache* pc = getPanelCache(cand_panel);
    for (int i = 0; i < map_len; i++) {
        strncpy(pc->texts[i], map[i], sizeof(pc->texts[i]) - 1);
        pc->texts[i][sizeof(pc->texts[i]) - 1] = '\0';
        pc->map[i] = pc->texts[i];
    }
    pc->map[map_len] = "";

    // Write sorted candidate texts back to original candidate slots order
    // candidate slots are encountered in appearance order in original map
    int slots[12];
    int slot_cnt = 0;
    for (int i = 0; i < map_len; i++) {
        uint32_t cp = 0;
        if (isCandidateChar(map[i], cp)) slots[slot_cnt++] = i;
    }
    for (int i = 0; i < slot_cnt && i < cand_cnt; i++) {
        if (strcmp(pc->texts[slots[i]], cands[i].txt) != 0) changed = true;
        strncpy(pc->texts[slots[i]], cands[i].txt, sizeof(pc->texts[slots[i]]) - 1);
        pc->texts[slots[i]][sizeof(pc->texts[slots[i]]) - 1] = '\0';
    }

    if (changed) {
        lv_buttonmatrix_set_map(cand_panel, pc->map);
    }
}
