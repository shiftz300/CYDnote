#ifndef IME_MRU_H
#define IME_MRU_H

#include <Arduino.h>
#include <lvgl.h>

class ImeMru {
public:
    static ImeMru& getInstance();

    void init();
    void learnFromUtf8(const char* utf8_char);
    void applyToCandidatePanel(lv_obj_t* cand_panel);

private:
    static constexpr const char* SAVE_PATH = "/ime_mru.csv";
    static constexpr uint16_t MAX_ITEMS = 128;

    struct Item {
        uint32_t cp;
        uint16_t score;
        uint16_t order;
    };

    struct PanelCache {
        lv_obj_t* panel;
        bool used;
        char texts[16][8];
        const char* map[16];
    };

    bool inited;
    uint16_t counter;
    Item items[MAX_ITEMS];
    PanelCache panel_cache[2];

    ImeMru();
    int findItem(uint32_t cp) const;
    int findReplaceSlot() const;
    uint16_t scoreOf(uint32_t cp) const;
    void load();
    void save() const;

    static bool isCandidateChar(const char* txt, uint32_t& cp_out);
    static uint32_t utf8ToCodepoint(const char* s, uint8_t& len_out);
    static bool isArrowOrEmpty(const char* txt);
    PanelCache* getPanelCache(lv_obj_t* panel);
};

#endif
