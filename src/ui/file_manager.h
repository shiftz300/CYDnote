#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <functional>
#include <vector>
#include <lvgl.h>
#include "../utils/sd_helper.h"
#include "font_manager.h"
#include "custom_pinyin_dict_plus.h"

class FileManager {
private:
    static constexpr uint16_t SCREEN_TRANSITION_MS = 55;
    static constexpr uint16_t IME_SLIDE_IN_MS = 90;
    static constexpr uint16_t IME_SLIDE_OUT_MS = 80;
    static constexpr int32_t DIALOG_LIFT_Y = -84;
    static constexpr int32_t IME_CANDIDATE_H = 64;
    static constexpr int32_t IME_KEYBOARD_H = 132;
    static constexpr int32_t IME_TOTAL_H_FALLBACK = IME_CANDIDATE_H + IME_KEYBOARD_H;
    enum DialogMode {
        DIALOG_NONE,
        DIALOG_NEW_ENTRY,
        DIALOG_RENAME
    };

    struct EntryMeta {
        String name;
        bool is_dir;
        String vpath;
        bool marked;
        lv_obj_t* checkbox;
        lv_obj_t* label;
    };

    struct CrumbMeta {
        String path;
    };

    lv_obj_t* screen;
    lv_obj_t* sidebar;
    lv_obj_t* breadcrumb_wrap;
    lv_obj_t* file_list;
    lv_obj_t* empty_label;
    lv_obj_t* fs_bar;
    lv_obj_t* fs_label;
    lv_obj_t* fs_panel;
    lv_obj_t* up_btn_ref;
    lv_obj_t* drive_btn_l;
    lv_obj_t* drive_btn_d;
    lv_obj_t* menu_panel;
    lv_obj_t* menu_copy_btn;
    lv_obj_t* menu_paste_btn;
    lv_obj_t* dialog_box;
    lv_obj_t* dialog_input;
    lv_obj_t* dialog_ime_container;
    lv_obj_t* dialog_ime;
    lv_obj_t* dialog_keyboard;
    lv_obj_t* dialog_new_file_btn;
    lv_obj_t* dialog_new_dir_btn;
    bool dialog_ime_font_acquired;
    std::function<void(const char*)> on_open_cb;

    bool sd_ready;
    bool remove_mode;
    bool copy_pick_mode;
    char active_drive;
    String current_path;
    String selected_vpath;
    String copied_vpath;
    String pending_open_vpath;
    bool new_as_dir;
    DialogMode dialog_mode;
    uint32_t fs_usage_last_ms;
    int fs_usage_last_pct;
    bool fs_usage_last_valid;
    bool list_suspended_for_dialog;

public:
    FileManager()
        : screen(nullptr), sidebar(nullptr), breadcrumb_wrap(nullptr), file_list(nullptr),
          empty_label(nullptr), fs_bar(nullptr), fs_label(nullptr), fs_panel(nullptr), up_btn_ref(nullptr), drive_btn_l(nullptr), drive_btn_d(nullptr), menu_panel(nullptr), menu_copy_btn(nullptr), menu_paste_btn(nullptr), dialog_box(nullptr), dialog_input(nullptr),
          dialog_ime_container(nullptr), dialog_ime(nullptr), dialog_keyboard(nullptr),
          dialog_new_file_btn(nullptr), dialog_new_dir_btn(nullptr),
          dialog_ime_font_acquired(false),
          on_open_cb(nullptr), sd_ready(false), remove_mode(false), copy_pick_mode(false), active_drive('L'),
          current_path("/"), selected_vpath(""), copied_vpath(""), pending_open_vpath(""), new_as_dir(false), dialog_mode(DIALOG_NONE),
          fs_usage_last_ms(0), fs_usage_last_pct(0), fs_usage_last_valid(false), list_suspended_for_dialog(false) {}

    void create(bool has_sd, std::function<void(const char*)> on_open = nullptr) {
        on_open_cb = on_open;
        sd_ready = has_sd;
        active_drive = 'L';
        current_path = "/";
        selected_vpath = "";
        pending_open_vpath = "";
        remove_mode = false;
        copy_pick_mode = false;
        new_as_dir = false;
        dialog_mode = DIALOG_NONE;
        list_suspended_for_dialog = false;

        screen = lv_obj_create(NULL);
        lv_obj_set_size(screen, 240, 320);
        lv_obj_set_style_pad_all(screen, 0, 0);
        lv_obj_set_style_pad_column(screen, 0, 0);
        lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(screen, 0, 0);
        lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

        sidebar = lv_obj_create(screen);
        lv_obj_set_size(sidebar, 40, 320);
        lv_obj_set_style_pad_all(sidebar, 2, 0);
        lv_obj_set_style_pad_row(sidebar, 4, 0);
        lv_obj_set_flex_flow(sidebar, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(sidebar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(sidebar, lv_color_hex(0x101010), 0);
        lv_obj_set_style_border_color(sidebar, lv_color_hex(0x303030), 0);
        lv_obj_clear_flag(sidebar, LV_OBJ_FLAG_SCROLLABLE);

        createDriveButton("L:", 'L', true);
        createDriveButton("D:", 'D', sd_ready);
        updateDriveButtonStyles();

        lv_obj_t* sidebar_spacer = lv_obj_create(sidebar);
        lv_obj_set_width(sidebar_spacer, lv_pct(100));
        lv_obj_set_flex_grow(sidebar_spacer, 1);
        lv_obj_set_style_bg_opa(sidebar_spacer, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(sidebar_spacer, 0, 0);
        lv_obj_set_style_pad_all(sidebar_spacer, 0, 0);
        lv_obj_clear_flag(sidebar_spacer, LV_OBJ_FLAG_SCROLLABLE);

        fs_panel = lv_obj_create(sidebar);
        lv_obj_set_width(fs_panel, lv_pct(100));
        lv_obj_set_height(fs_panel, 72);
        lv_obj_set_style_pad_all(fs_panel, 1, 0);
        lv_obj_set_style_pad_row(fs_panel, 0, 0);
        lv_obj_set_style_radius(fs_panel, 4, 0);
        lv_obj_set_style_clip_corner(fs_panel, true, 0);
        lv_obj_set_style_bg_color(fs_panel, lv_color_hex(0x090909), 0);
        lv_obj_set_style_border_color(fs_panel, lv_color_hex(0x303030), 0);
        lv_obj_set_flex_flow(fs_panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(fs_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(fs_panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(fs_panel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_event_cb(fs_panel, fs_panel_event_cb, LV_EVENT_CLICKED, this);

        fs_bar = lv_bar_create(fs_panel);
        lv_obj_set_size(fs_bar, 10, 38);
        lv_bar_set_range(fs_bar, 0, 100);
        lv_bar_set_orientation(fs_bar, LV_BAR_ORIENTATION_VERTICAL);
        lv_obj_set_style_bg_color(fs_bar, lv_color_hex(0x1b1b1b), LV_PART_MAIN);
        lv_obj_set_style_bg_color(fs_bar, lv_color_hex(0x5CB8FF), LV_PART_INDICATOR);
        lv_obj_set_style_radius(fs_bar, 2, LV_PART_MAIN);
        lv_obj_set_style_radius(fs_bar, 2, LV_PART_INDICATOR);

        fs_label = lv_label_create(fs_panel);
        lv_label_set_text(fs_label, "0");
        lv_obj_set_style_text_font(fs_label, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(fs_label, lv_color_hex(0xBFDFFF), 0);
        lv_obj_set_style_text_align(fs_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(fs_label, lv_pct(100));
        lv_label_set_long_mode(fs_label, LV_LABEL_LONG_CLIP);

        lv_obj_t* main = lv_obj_create(screen);
        lv_obj_set_height(main, 320);
        lv_obj_set_flex_grow(main, 1);
        lv_obj_set_style_pad_left(main, 2, 0);
        lv_obj_set_style_pad_right(main, 2, 0);
        lv_obj_set_style_pad_top(main, 2, 0);
        lv_obj_set_style_pad_bottom(main, 2, 0);
        lv_obj_set_style_pad_row(main, 2, 0);
        lv_obj_set_flex_flow(main, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(main, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_bg_color(main, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(main, lv_color_hex(0x303030), 0);
        lv_obj_set_style_border_side(main, LV_BORDER_SIDE_FULL, 0);
        lv_obj_clear_flag(main, LV_OBJ_FLAG_SCROLLABLE);

        breadcrumb_wrap = lv_obj_create(main);
        lv_obj_set_width(breadcrumb_wrap, lv_pct(100));
        lv_obj_set_height(breadcrumb_wrap, 30);
        lv_obj_set_style_pad_left(breadcrumb_wrap, 2, 0);
        lv_obj_set_style_pad_right(breadcrumb_wrap, 2, 0);
        lv_obj_set_style_pad_top(breadcrumb_wrap, 2, 0);
        lv_obj_set_style_pad_bottom(breadcrumb_wrap, 2, 0);
        lv_obj_set_style_margin_bottom(breadcrumb_wrap, 1, 0);
        lv_obj_set_flex_flow(breadcrumb_wrap, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_color(breadcrumb_wrap, lv_color_hex(0x050505), 0);
        lv_obj_set_style_border_color(breadcrumb_wrap, lv_color_hex(0x303030), 0);
        lv_obj_set_style_radius(breadcrumb_wrap, 5, 0);
        lv_obj_set_style_clip_corner(breadcrumb_wrap, true, 0);
        lv_obj_clear_flag(breadcrumb_wrap, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* control_row = lv_obj_create(main);
        lv_obj_set_width(control_row, lv_pct(100));
        lv_obj_set_height(control_row, 36);
        lv_obj_set_style_pad_left(control_row, 3, 0);
        lv_obj_set_style_pad_right(control_row, 3, 0);
        lv_obj_set_style_pad_top(control_row, 4, 0);
        lv_obj_set_style_pad_bottom(control_row, 4, 0);
        lv_obj_set_style_pad_column(control_row, 2, 0);
        lv_obj_set_flex_flow(control_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_bg_color(control_row, lv_color_hex(0x0A0A0A), 0);
        lv_obj_set_style_border_color(control_row, lv_color_hex(0x303030), 0);
        lv_obj_set_style_radius(control_row, 4, 0);
        lv_obj_set_style_clip_corner(control_row, true, 0);
        lv_obj_clear_flag(control_row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* up_btn = lv_button_create(control_row);
        lv_obj_set_size(up_btn, 26, 26);
        styleActionButton(up_btn);
        up_btn_ref = up_btn;
        lv_obj_t* up_lbl = lv_label_create(up_btn);
        lv_label_set_text(up_lbl, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(up_lbl, FontManager::iconFont(), 0);
        lv_obj_center(up_lbl);
        lv_obj_add_event_cb(up_btn, up_btn_event_cb, LV_EVENT_CLICKED, this);

        lv_obj_t* control_spacer = lv_obj_create(control_row);
        lv_obj_set_width(control_spacer, 1);
        lv_obj_set_flex_grow(control_spacer, 1);
        lv_obj_set_style_bg_opa(control_spacer, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(control_spacer, 0, 0);
        lv_obj_set_style_pad_all(control_spacer, 0, 0);
        lv_obj_clear_flag(control_spacer, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* menu_btn = lv_button_create(control_row);
        lv_obj_set_size(menu_btn, 26, 26);
        styleActionButton(menu_btn);
        lv_obj_t* menu_lbl = lv_label_create(menu_btn);
        lv_label_set_text(menu_lbl, LV_SYMBOL_LIST);
        lv_obj_set_style_text_font(menu_lbl, FontManager::iconFont(), 0);
        lv_obj_center(menu_lbl);
        lv_obj_add_event_cb(menu_btn, menu_btn_event_cb, LV_EVENT_CLICKED, this);

        file_list = lv_obj_create(main);
        lv_obj_set_width(file_list, lv_pct(100));
        lv_obj_set_flex_grow(file_list, 1);
        lv_obj_set_style_pad_all(file_list, 1, 0);
        lv_obj_set_style_pad_row(file_list, 1, 0);
        lv_obj_set_flex_flow(file_list, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(file_list, LV_DIR_VER);
        lv_obj_set_style_bg_color(file_list, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(file_list, lv_color_hex(0x303030), 0);
        lv_obj_set_style_radius(file_list, 4, 0);

        empty_label = lv_label_create(file_list);
        lv_label_set_text(empty_label, "");
        lv_obj_set_style_text_color(empty_label, lv_color_hex(0xAAAAAA), 0);
        lv_obj_set_style_text_font(empty_label, FontManager::iconFont(), 0);
        lv_obj_center(empty_label);
        lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);

        menu_panel = lv_obj_create(main);
        lv_obj_set_width(menu_panel, lv_pct(100));
        lv_obj_set_height(menu_panel, LV_SIZE_CONTENT);
        lv_obj_align(menu_panel, LV_ALIGN_TOP_LEFT, 0, 72);
        lv_obj_set_style_bg_color(menu_panel, lv_color_hex(0x080808), 0);
        lv_obj_set_style_border_color(menu_panel, lv_color_hex(0x404040), 0);
        lv_obj_set_style_pad_all(menu_panel, 4, 0);
        lv_obj_set_style_pad_row(menu_panel, 2, 0);
        lv_obj_set_flex_flow(menu_panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_scroll_dir(menu_panel, LV_DIR_VER);
        addMenuAction(menu_panel, LV_SYMBOL_FILE " New", menu_new_event_cb);
        addMenuAction(menu_panel, LV_SYMBOL_TRASH " Delete", menu_remove_event_cb);
        menu_copy_btn = addMenuAction(menu_panel, LV_SYMBOL_COPY " Copy", menu_copy_event_cb);
        menu_paste_btn = addMenuAction(menu_panel, LV_SYMBOL_PASTE " Paste", menu_paste_event_cb);
        addMenuAction(menu_panel, LV_SYMBOL_EDIT " Rename", menu_rename_event_cb);
        lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
        updateMenuActionStates();

        refreshUi();
    }

    void destroy() {
        releaseDialogIMEFont();
        if (screen) {
            clearList();
            clearBreadcrumb();
            lv_obj_del(screen);
            screen = nullptr;
            sidebar = nullptr;
            breadcrumb_wrap = nullptr;
            file_list = nullptr;
            empty_label = nullptr;
            fs_panel = nullptr;
            up_btn_ref = nullptr;
            drive_btn_l = nullptr;
            drive_btn_d = nullptr;
            menu_panel = nullptr;
            menu_copy_btn = nullptr;
            menu_paste_btn = nullptr;
            dialog_box = nullptr;
            dialog_input = nullptr;
            dialog_ime_container = nullptr;
            dialog_ime = nullptr;
            dialog_keyboard = nullptr;
            dialog_ime_font_acquired = false;
        }
    }

    void show(lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE) {
        if (screen) {
            if(anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(screen);
            else lv_screen_load_anim(screen, anim, SCREEN_TRANSITION_MS, 0, false);
        }
    }

private:
    static void drive_btn_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        char drive = (char)(intptr_t)lv_obj_get_user_data((lv_obj_t*)lv_event_get_target(e));
        if (drive == 'D' && !fm->sd_ready) return;
        fm->exitRemoveModeIfNeeded(false);
        fm->active_drive = drive;
        fm->current_path = "/";
        fm->selected_vpath = "";
        fm->updateDriveButtonStyles();
        fm->refreshUi();
    }

    static void up_btn_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->remove_mode) {
            fm->exitRemoveModeIfNeeded(true);
            return;
        }
        if (fm->current_path == "/") return;
        fm->goUp();
        fm->refreshUi();
    }

    static void crumb_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (!fm || !btn) return;
        CrumbMeta* meta = (CrumbMeta*)lv_obj_get_user_data(btn);
        if (!meta) return;
        fm->current_path = meta->path;
        fm->refreshUi();
    }

    static void file_item_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (!fm || !btn) return;
        EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(btn);
        if (!meta) return;

        bool same_selected = (fm->selected_vpath == meta->vpath);
        fm->selected_vpath = meta->vpath;
        fm->refreshSelectionHighlight();

        if (fm->remove_mode) {
            meta->marked = !meta->marked;
            fm->refreshSelectionHighlight();
            return;
        }
        if (fm->copy_pick_mode) {
            if (meta->is_dir) {
                fm->current_path = fm->joinPath(fm->current_path, meta->name);
                fm->selected_vpath = "";
                fm->refreshUi();
                return;
            }
            fm->copied_vpath = meta->vpath;
            fm->updateMenuActionStates();
            fm->copy_pick_mode = false;
            fm->closeMenuPanel();
            fm->refreshSelectionHighlight();
            return;
        }

        // Single tap: select only. Tap the same row again to open.
        if (!same_selected) return;

        if (meta->is_dir) {
            fm->current_path = fm->joinPath(fm->current_path, meta->name);
            fm->selected_vpath = "";
            fm->refreshUi();
            return;
        }

        if (fm->on_open_cb) {
            static const size_t LARGE_FILE_WARN_BYTES = 128 * 1024;
            size_t fsz = fm->getFileSize(meta->vpath);
            if (fsz >= LARGE_FILE_WARN_BYTES) {
                fm->openLargeFileWarnDialog(meta->vpath, fsz);
            } else {
                fm->on_open_cb(meta->vpath.c_str());
            }
        }
    }

    static void checkbox_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        lv_obj_t* cb = (lv_obj_t*)lv_event_get_target(e);
        if (!fm || !cb) return;
        lv_obj_t* row_btn = lv_obj_get_parent(cb);
        if (!row_btn) return;
        EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(row_btn);
        if (!meta) return;
        meta->marked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    }

    static void menu_btn_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm || !fm->menu_panel) return;
        if (lv_obj_has_flag(fm->menu_panel, LV_OBJ_FLAG_HIDDEN)) {
            fm->updateMenuActionStates();
            lv_obj_remove_flag(fm->menu_panel, LV_OBJ_FLAG_HIDDEN);
        }
        else lv_obj_add_flag(fm->menu_panel, LV_OBJ_FLAG_HIDDEN);
    }

    static void menu_new_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->new_as_dir = false;
        fm->openInputDialog(DIALOG_NEW_ENTRY, "Create new", "new_note.txt");
    }

    static void menu_remove_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitCopyPickModeIfNeeded(false);
        if (!fm->remove_mode) {
            fm->remove_mode = true;
            fm->closeMenuPanel();
            lv_async_call(FileManager::async_refresh_cb, fm);
            return;
        }
        fm->openRemoveConfirmDialog();
    }

    static void async_refresh_cb(void* user_data) {
        FileManager* fm = (FileManager*)user_data;
        if (!fm) return;
        fm->refreshUi();
    }

    static void menu_copy_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(false);
        fm->copy_pick_mode = true;
        fm->selected_vpath = "";
        fm->closeMenuPanel();
        fm->refreshSelectionHighlight();
    }

    static void menu_paste_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->pasteCopied();
    }

    static void menu_rename_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        if (fm->selected_vpath.length() == 0) return;
        String cur = fm->baseName(fm->innerPath(fm->selected_vpath));
        fm->openInputDialog(DIALOG_RENAME, "Rename to", cur.c_str());
    }

    static void dialog_ok_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->confirmDialog();
    }

    static void dialog_cancel_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->closeDialog();
    }

    static void dialog_input_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        lv_event_code_t code = lv_event_get_code(e);
        if (code == LV_EVENT_FOCUSED || code == LV_EVENT_CLICKED) {
            fm->openDialogIME();
        }
    }

    static void remove_normal_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->closeDialog();
        fm->applyRemoveMarked(false);
    }

    static void remove_force_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->closeDialog();
        fm->applyRemoveMarked(true);
    }

    static void fs_panel_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->openFsInfoDialog();
    }

    static void ime_translate_y_anim_cb(void* obj, int32_t v) {
        lv_obj_set_style_translate_y((lv_obj_t*)obj, v, 0);
    }

    static void dialog_ime_hide_done_cb(lv_anim_t* a) {
        FileManager* fm = (FileManager*)lv_anim_get_user_data(a);
        if (!fm || !fm->dialog_ime_container) return;
        lv_obj_del(fm->dialog_ime_container);
        fm->dialog_ime_container = nullptr;
        fm->dialog_ime = nullptr;
        fm->dialog_keyboard = nullptr;
        fm->releaseDialogIMEFont();
    }

    static void large_open_confirm_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->pending_open_vpath.length() > 0 && fm->on_open_cb) {
            fm->on_open_cb(fm->pending_open_vpath.c_str());
        }
        fm->pending_open_vpath = "";
        fm->closeDialog();
    }

    static void new_type_file_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->new_as_dir = false;
        fm->updateNewTypeButtons();
    }

    static void new_type_dir_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->new_as_dir = true;
        fm->updateNewTypeButtons();
    }

    void createDriveButton(const char* label, char drive, bool enabled) {
        lv_obj_t* btn = lv_button_create(sidebar);
        lv_obj_set_size(btn, 30, 30);
        lv_obj_set_user_data(btn, (void*)(intptr_t)drive);
        lv_obj_add_event_cb(btn, drive_btn_event_cb, LV_EVENT_CLICKED, this);
        styleActionButton(btn);
        if (!enabled) lv_obj_add_state(btn, LV_STATE_DISABLED);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);

        if (drive == 'L') drive_btn_l = btn;
        else if (drive == 'D') drive_btn_d = btn;
    }

    void updateDriveButtonStyles() {
        if (drive_btn_l) {
            lv_obj_set_style_bg_color(
                drive_btn_l,
                (active_drive == 'L') ? lv_color_hex(0x5CB8FF) : lv_color_hex(0x1E1E1E),
                LV_STATE_DEFAULT
            );
            lv_obj_set_style_border_color(
                drive_btn_l,
                (active_drive == 'L') ? lv_color_hex(0x8ED1FF) : lv_color_hex(0x404040),
                LV_STATE_DEFAULT
            );
        }
        if (drive_btn_d) {
            lv_obj_set_style_bg_color(
                drive_btn_d,
                (active_drive == 'D') ? lv_color_hex(0x5CB8FF) : lv_color_hex(0x1E1E1E),
                LV_STATE_DEFAULT
            );
            lv_obj_set_style_border_color(
                drive_btn_d,
                (active_drive == 'D') ? lv_color_hex(0x8ED1FF) : lv_color_hex(0x404040),
                LV_STATE_DEFAULT
            );
        }
    }

    lv_obj_t* addMenuAction(lv_obj_t* parent, const char* label, lv_event_cb_t cb) {
        lv_obj_t* btn = lv_button_create(parent);
        lv_obj_set_size(btn, lv_pct(100), 30);
        styleActionButton(btn);
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, this);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label);
        lv_obj_set_style_text_font(lbl, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lbl);
        return btn;
    }

    void refreshUi() {
        rebuildBreadcrumb();
        reloadEntries();
        refreshSelectionHighlight();
        updateUpButtonState();
        updateFsUsageUi();
    }

    void updateUpButtonState() {
        if (!up_btn_ref) return;
        bool at_root = (current_path == "/");
        bool disable = at_root && !remove_mode;
        if (disable) {
            lv_obj_add_state(up_btn_ref, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(up_btn_ref, lv_color_hex(0x101010), LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(up_btn_ref, lv_color_hex(0x2A2A2A), LV_STATE_DEFAULT);
        } else {
            lv_obj_clear_state(up_btn_ref, LV_STATE_DISABLED);
            lv_obj_set_style_bg_color(up_btn_ref, lv_color_hex(0x1E1E1E), LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(up_btn_ref, lv_color_hex(0x404040), LV_STATE_DEFAULT);
        }
    }

    void rebuildBreadcrumb() {
        clearBreadcrumb();
        addCrumbButton(String(active_drive) + ":/", "/");
        if (current_path == "/") return;

        String work = current_path;
        if (work.startsWith("/")) work.remove(0, 1);

        String segs[16];
        String paths[16];
        int seg_count = 0;
        String built = "";
        int from = 0;
        while (from < work.length()) {
            int slash = work.indexOf('/', from);
            String seg = (slash >= 0) ? work.substring(from, slash) : work.substring(from);
            if (seg.length() > 0) {
                built += "/" + seg;
                if (seg_count < 16) {
                    segs[seg_count] = seg;
                    paths[seg_count] = built;
                    seg_count++;
                }
            }
            if (slash < 0) break;
            from = slash + 1;
        }

        if (seg_count <= 2) {
            for (int i = 0; i < seg_count; i++) addCrumbButton(segs[i], paths[i]);
            return;
        }

        // Compact breadcrumb: root / ... / parent / current
        String ellipsis_path = parentPath(paths[seg_count - 2]);
        addCrumbButton("...", ellipsis_path);
        addCrumbButton(segs[seg_count - 2], paths[seg_count - 2]);
        addCrumbButton(segs[seg_count - 1], paths[seg_count - 1]);
    }

    void addCrumbButton(const String& label, const String& path) {
        lv_obj_t* btn = lv_button_create(breadcrumb_wrap);
        lv_obj_set_height(btn, 22);
        lv_obj_set_style_pad_hor(btn, 4, 0);
        styleActionButton(btn);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_t* lbl = lv_label_create(btn);
        lv_label_set_text(lbl, label.c_str());
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, FontManager::iconFont(), 0);
        lv_obj_center(lbl);

        CrumbMeta* meta = new CrumbMeta();
        meta->path = path;
        lv_obj_set_user_data(btn, meta);
        lv_obj_add_event_cb(btn, crumb_event_cb, LV_EVENT_CLICKED, this);
    }

    void clearBreadcrumb() {
        if (!breadcrumb_wrap) return;
        uint32_t count = lv_obj_get_child_count(breadcrumb_wrap);
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* child = lv_obj_get_child(breadcrumb_wrap, i);
            CrumbMeta* meta = (CrumbMeta*)lv_obj_get_user_data(child);
            if (meta) delete meta;
        }
        lv_obj_clean(breadcrumb_wrap);
    }

    void reloadEntries() {
        clearList();
        if (!file_list || !empty_label) return;

        bool has_items = false;
        if (active_drive == 'L') has_items = loadLittleFsEntries();
        else if (active_drive == 'D') has_items = loadSdEntries();

        if (!has_items) {
            lv_label_set_text(empty_label, remove_mode ? "(remove mode, no items)" : "(empty)");
            lv_obj_remove_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_center(empty_label);
        } else {
            lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    bool loadLittleFsEntries() {
        File dir = LittleFS.open(current_path.c_str(), "r");
        if (!dir || !dir.isDirectory()) {
            lv_label_set_text(empty_label, "(invalid path)");
            return false;
        }
        bool has_items = false;
        uint32_t iter = 0;
        while (true) {
            File entry = dir.openNextFile();
            if (!entry) break;
            String name = baseName(entry.name());
            if (name.length() > 0) {
                addEntryButton(name, entry.isDirectory());
                has_items = true;
            }
            entry.close();
            if ((++iter & 0x0F) == 0) delay(0);
        }
        dir.close();
        return has_items;
    }

    bool loadSdEntries() {
        if (!sd_ready || !StorageHelper::getInstance()->isInitialized()) {
            lv_label_set_text(empty_label, "(SD unavailable)");
            return false;
        }
        FsFile dir = StorageHelper::getInstance()->getFs().open(current_path.c_str(), O_RDONLY);
        if (!dir || !dir.isDir()) {
            lv_label_set_text(empty_label, "(invalid path)");
            return false;
        }
        bool has_items = false;
        uint32_t iter = 0;
        FsFile entry;
        while (entry.openNext(&dir, O_RDONLY)) {
            char name[256];
            entry.getName(name, sizeof(name));
            if (name[0] != '\0') {
                addEntryButton(String(name), entry.isDir());
                has_items = true;
            }
            entry.close();
            if ((++iter & 0x0F) == 0) delay(0);
        }
        dir.close();
        return has_items;
    }

    void addEntryButton(const String& name, bool is_dir) {
        lv_obj_t* row = lv_button_create(file_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 32);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x101010), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x173248), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1E4B6D), LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x2C2C2C), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_style_pad_ver(row, 1, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        EntryMeta* meta = new EntryMeta();
        meta->name = name;
        meta->is_dir = is_dir;
        meta->vpath = String(active_drive) + ":" + joinPath(current_path, name);
        meta->marked = false;
        meta->checkbox = nullptr;
        meta->label = nullptr;
        lv_obj_set_user_data(row, meta);
        lv_obj_add_event_cb(row, file_item_event_cb, LV_EVENT_CLICKED, this);

        lv_obj_t* lbl = lv_label_create(row);
        String text = formatEntryLabel(*meta);
        lv_label_set_text(lbl, text.c_str());
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_flex_grow(lbl, 1);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(lbl, FontManager::textFont(), 0);
        meta->label = lbl;
    }

    String formatEntryLabel(const EntryMeta& meta) const {
        String base = meta.is_dir ? (String(LV_SYMBOL_DIRECTORY) + " " + meta.name)
                                  : (String(LV_SYMBOL_FILE) + " " + meta.name);
        return base;
    }

    void refreshSelectionHighlight() {
        if (!file_list) return;
        uint32_t count = lv_obj_get_child_count(file_list);
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* child = lv_obj_get_child(file_list, i);
            if (child == empty_label) continue;
            EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(child);
            if (!meta) continue;
            bool selected = (meta->vpath == selected_vpath);
            bool multi_selected = remove_mode ? meta->marked : selected;
            lv_obj_set_style_bg_color(child, multi_selected ? lv_color_hex(0x1E4B6D) : lv_color_hex(0x101010), LV_STATE_DEFAULT);
            lv_obj_set_style_border_color(child, multi_selected ? lv_color_hex(0x8ED1FF) : lv_color_hex(0x2C2C2C), LV_STATE_DEFAULT);
        }
    }

    void styleActionButton(lv_obj_t* btn) {
        lv_obj_set_style_radius(btn, 4, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E1E), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x5CB8FF), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_PRESSED);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x8ED1FF), LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
    }

    void updateFsUsageUi() {
        if (!fs_bar || !fs_label) return;

        // SD usage probing can be expensive (freeClusterCount scan). Throttle it.
        if (active_drive == 'D') {
            uint32_t now = millis();
            if (fs_usage_last_valid && (now - fs_usage_last_ms) < 1500) {
                lv_bar_set_value(fs_bar, fs_usage_last_pct, LV_ANIM_OFF);
                char quick[16];
                lv_snprintf(quick, sizeof(quick), "%d\n%%", fs_usage_last_pct);
                lv_label_set_text(fs_label, quick);
                return;
            }
            fs_usage_last_ms = now;
        }

        uint64_t total = 0;
        uint64_t used = 0;

        if (active_drive == 'L') {
            total = (uint64_t)LittleFS.totalBytes();
            used = (uint64_t)LittleFS.usedBytes();
        } else if (active_drive == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            total = StorageHelper::getInstance()->totalBytes();
            used = StorageHelper::getInstance()->usedBytes();
        }

        int pct = 0;
        if (total > 0) pct = (int)((used * 100) / total);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        lv_bar_set_value(fs_bar, pct, LV_ANIM_OFF);
        fs_usage_last_pct = pct;
        fs_usage_last_valid = (total > 0);

        char buf[48];
        if (total == 0) lv_snprintf(buf, sizeof(buf), "--");
        else lv_snprintf(buf, sizeof(buf), "%d\n%%", pct);
        lv_label_set_text(fs_label, buf);
    }

    void clearList() {
        if (!file_list) return;
        uint32_t count = lv_obj_get_child_count(file_list);
        for (int32_t i = (int32_t)count - 1; i >= 0; i--) {
            lv_obj_t* child = lv_obj_get_child(file_list, (uint32_t)i);
            if (child == empty_label) continue;
            EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(child);
            if (meta) delete meta;
            lv_obj_del(child);
        }
    }

    void closeMenuPanel() {
        if (menu_panel) lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
    }

    void updateMenuActionStates() {
        if (menu_paste_btn) {
            if (copied_vpath.length() > 0) lv_obj_clear_state(menu_paste_btn, LV_STATE_DISABLED);
            else lv_obj_add_state(menu_paste_btn, LV_STATE_DISABLED);
        }
    }

    void suspendListForDialog() {
        if (list_suspended_for_dialog) return;
        clearList();
        list_suspended_for_dialog = true;
    }

    void exitRemoveModeIfNeeded(bool refresh_ui = false) {
        if (!remove_mode) return;
        remove_mode = false;
        if (menu_panel) lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
        if (refresh_ui) refreshUi();
    }

    void exitCopyPickModeIfNeeded(bool refresh_ui = false) {
        if (!copy_pick_mode) return;
        copy_pick_mode = false;
        if (menu_panel) lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
        if (refresh_ui) refreshSelectionHighlight();
    }

    void goUp() {
        if (current_path == "/") return;
        int idx = current_path.lastIndexOf('/');
        current_path = (idx <= 0) ? "/" : current_path.substring(0, idx);
    }

    void openInputDialog(DialogMode mode, const char* title, const char* initial) {
        closeMenuPanel();
        closeDialog();
        dialog_mode = mode;
        suspendListForDialog();

        dialog_box = lv_obj_create(screen);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_remove_style_all(dialog_box);
        lv_obj_set_width(dialog_box, 186);
        lv_obj_set_height(dialog_box, LV_SIZE_CONTENT);
        lv_obj_center(dialog_box);
        lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x000000), 0);
        lv_obj_set_style_bg_opa(dialog_box, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dialog_box, lv_color_hex(0x505050), 0);
        lv_obj_set_style_border_width(dialog_box, 1, 0);
        lv_obj_set_style_radius(dialog_box, 6, 0);
        lv_obj_set_style_clip_corner(dialog_box, true, 0);
        lv_obj_set_style_shadow_width(dialog_box, 0, 0);
        lv_obj_set_style_outline_width(dialog_box, 0, 0);
        lv_obj_set_style_pad_all(dialog_box, 6, 0);
        lv_obj_set_style_pad_row(dialog_box, 4, 0);
        lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title_lbl = lv_label_create(dialog_box);
        lv_label_set_text(title_lbl, title);
        lv_label_set_long_mode(title_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(title_lbl, lv_pct(100));
        lv_obj_set_style_text_color(title_lbl, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(title_lbl, FontManager::textFont(), 0);

        lv_obj_t* hint_lbl = lv_label_create(dialog_box);
        lv_label_set_text(hint_lbl, "Type a name. New supports file and folder.");
        lv_label_set_long_mode(hint_lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(hint_lbl, lv_pct(100));
        lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0xB0B0B0), 0);
        lv_obj_set_style_text_font(hint_lbl, FontManager::iconFont(), 0);

        if (mode == DIALOG_NEW_ENTRY) {
            lv_obj_t* type_row = lv_obj_create(dialog_box);
            lv_obj_remove_style_all(type_row);
            lv_obj_set_width(type_row, lv_pct(100));
            lv_obj_set_height(type_row, 30);
            lv_obj_set_style_bg_color(type_row, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(type_row, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_width(type_row, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_left(type_row, 2, LV_PART_MAIN);
            lv_obj_set_style_pad_right(type_row, 2, LV_PART_MAIN);
            lv_obj_set_style_pad_top(type_row, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_bottom(type_row, 0, LV_PART_MAIN);
            lv_obj_set_style_pad_column(type_row, 6, 0);
            lv_obj_set_flex_flow(type_row, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(type_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_clear_flag(type_row, LV_OBJ_FLAG_SCROLLABLE);

            dialog_new_file_btn = lv_button_create(type_row);
            lv_obj_set_size(dialog_new_file_btn, 76, 28);
            styleActionButton(dialog_new_file_btn);
            lv_obj_add_event_cb(dialog_new_file_btn, new_type_file_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* file_lbl = lv_label_create(dialog_new_file_btn);
            lv_label_set_text(file_lbl, LV_SYMBOL_FILE " File");
            lv_obj_set_style_text_font(file_lbl, FontManager::iconFont(), 0);
            lv_obj_center(file_lbl);

            dialog_new_dir_btn = lv_button_create(type_row);
            lv_obj_set_size(dialog_new_dir_btn, 86, 28);
            styleActionButton(dialog_new_dir_btn);
            lv_obj_add_event_cb(dialog_new_dir_btn, new_type_dir_event_cb, LV_EVENT_CLICKED, this);
            lv_obj_t* dir_lbl = lv_label_create(dialog_new_dir_btn);
            lv_label_set_text(dir_lbl, LV_SYMBOL_DIRECTORY " Folder");
            lv_obj_set_style_text_font(dir_lbl, FontManager::iconFont(), 0);
            lv_obj_center(dir_lbl);

            updateNewTypeButtons();
        }

        dialog_input = lv_textarea_create(dialog_box);
        lv_obj_set_width(dialog_input, lv_pct(100));
        lv_obj_set_height(dialog_input, 36);
        lv_textarea_set_one_line(dialog_input, true);
        lv_textarea_set_text(dialog_input, initial ? initial : "");
        lv_obj_set_style_bg_color(dialog_input, lv_color_hex(0x101010), 0);
        lv_obj_set_style_text_color(dialog_input, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_color(dialog_input, lv_color_hex(0x404040), 0);
        lv_obj_set_style_text_font(dialog_input, FontManager::textFont(), 0);
        lv_obj_set_style_bg_color(dialog_input, lv_color_hex(0xFFFFFF), LV_PART_CURSOR);
        lv_obj_set_style_bg_opa(dialog_input, LV_OPA_70, LV_PART_CURSOR);
        lv_obj_set_style_width(dialog_input, 1, LV_PART_CURSOR);
        lv_obj_add_event_cb(dialog_input, dialog_input_event_cb, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(dialog_input, dialog_input_event_cb, LV_EVENT_CLICKED, this);

        lv_obj_t* row = lv_obj_create(dialog_box);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 42);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_column(row, 6, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cancel_btn = lv_button_create(row);
        lv_obj_set_size(cancel_btn, 70, 30);
        styleActionButton(cancel_btn);
        lv_obj_add_event_cb(cancel_btn, dialog_cancel_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_lbl, "Cancel");
        lv_obj_set_style_text_font(cancel_lbl, FontManager::iconFont(), 0);
        lv_obj_center(cancel_lbl);

        lv_obj_t* ok_btn = lv_button_create(row);
        lv_obj_set_size(ok_btn, 54, 30);
        styleActionButton(ok_btn);
        lv_obj_add_event_cb(ok_btn, dialog_ok_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* ok_lbl = lv_label_create(ok_btn);
        lv_label_set_text(ok_lbl, "OK");
        lv_obj_set_style_text_font(ok_lbl, FontManager::iconFont(), 0);
        lv_obj_center(ok_lbl);

    }

    void closeDialog() {
        closeDialogIME();
        bool need_resume_list = list_suspended_for_dialog;
        if (dialog_box) {
            lv_obj_del(dialog_box);
            dialog_box = nullptr;
            dialog_input = nullptr;
            dialog_new_file_btn = nullptr;
            dialog_new_dir_btn = nullptr;
            pending_open_vpath = "";
            dialog_mode = DIALOG_NONE;
        }
        if (need_resume_list) {
            list_suspended_for_dialog = false;
            refreshUi();
        }
    }

    void openDialogIME() {
        if (!screen || !dialog_input) return;
        if (dialog_ime_container) return;
        if (!dialog_ime_font_acquired) dialog_ime_font_acquired = FontManager::acquireIMEFont();

        dialog_ime_container = lv_obj_create(screen);
        lv_obj_add_flag(dialog_ime_container, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_width(dialog_ime_container, lv_pct(100));
        lv_obj_set_height(dialog_ime_container, LV_SIZE_CONTENT);
        lv_obj_set_align(dialog_ime_container, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_flex_flow(dialog_ime_container, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_style_bg_color(dialog_ime_container, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(dialog_ime_container, lv_color_hex(0x303030), 0);
        lv_obj_set_style_border_width(dialog_ime_container, 1, 0);
        lv_obj_set_style_pad_all(dialog_ime_container, 0, 0);
        lv_obj_clear_flag(dialog_ime_container, LV_OBJ_FLAG_SCROLLABLE);

        dialog_ime = lv_ime_pinyin_create(dialog_ime_container);
        lv_obj_set_width(dialog_ime, lv_pct(100));
        lv_obj_set_height(dialog_ime, IME_CANDIDATE_H);
        lv_obj_set_style_bg_color(dialog_ime, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(dialog_ime, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(dialog_ime, lv_color_hex(0x111111), LV_PART_ITEMS);
        lv_obj_set_style_text_color(dialog_ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(dialog_ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(dialog_ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(dialog_ime, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_font(dialog_ime, FontManager::imeFont(), LV_PART_MAIN);
        lv_obj_set_style_text_font(dialog_ime, FontManager::imeFont(), LV_PART_ITEMS);

        dialog_keyboard = lv_keyboard_create(dialog_ime_container);
        lv_obj_set_width(dialog_keyboard, lv_pct(100));
        lv_obj_set_height(dialog_keyboard, IME_KEYBOARD_H);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x000000), 0);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x1A1A1A), LV_PART_ITEMS);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x303030), LV_PART_ITEMS);
        lv_obj_set_style_text_font(dialog_keyboard, FontManager::imeFont(), LV_PART_MAIN);
        lv_obj_set_style_text_font(dialog_keyboard, FontManager::imeFont(), LV_PART_ITEMS);
        lv_keyboard_set_textarea(dialog_keyboard, dialog_input);
        lv_ime_pinyin_set_keyboard(dialog_ime, dialog_keyboard);
        lv_ime_pinyin_set_dict(dialog_ime, g_pinyin_dict_plus);
        lv_ime_pinyin_set_mode(dialog_ime, LV_IME_PINYIN_MODE_K26);

        lv_obj_add_state(dialog_input, LV_STATE_FOCUSED);
        lv_obj_scroll_to_view(dialog_input, LV_ANIM_OFF);

        int32_t h = lv_obj_get_height(dialog_ime_container);
        if (h <= 0) h = IME_TOTAL_H_FALLBACK;
        if (dialog_box) {
            lv_anim_t up;
            lv_anim_init(&up);
            lv_anim_set_var(&up, dialog_box);
            lv_anim_set_exec_cb(&up, ime_translate_y_anim_cb);
            lv_anim_set_values(&up, 0, DIALOG_LIFT_Y);
            lv_anim_set_time(&up, IME_SLIDE_IN_MS);
            lv_anim_set_path_cb(&up, lv_anim_path_ease_out);
            lv_anim_start(&up);
        }
        lv_obj_set_style_translate_y(dialog_ime_container, h, 0);

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, dialog_ime_container);
        lv_anim_set_exec_cb(&a, ime_translate_y_anim_cb);
        lv_anim_set_values(&a, h, 0);
        lv_anim_set_time(&a, IME_SLIDE_IN_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    void closeDialogIME() {
        if (dialog_ime_container) {
            lv_anim_del(dialog_ime_container, ime_translate_y_anim_cb);
            int32_t h = lv_obj_get_height(dialog_ime_container);
            if (h <= 0) h = IME_TOTAL_H_FALLBACK;
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, dialog_ime_container);
            lv_anim_set_exec_cb(&a, ime_translate_y_anim_cb);
            lv_anim_set_values(&a, 0, h);
            lv_anim_set_time(&a, IME_SLIDE_OUT_MS);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
            lv_anim_set_user_data(&a, this);
            lv_anim_set_completed_cb(&a, dialog_ime_hide_done_cb);
            lv_anim_start(&a);
        } else {
            releaseDialogIMEFont();
        }
        if (dialog_box) {
            lv_anim_del(dialog_box, ime_translate_y_anim_cb);
            int32_t cur_y = lv_obj_get_style_translate_y(dialog_box, LV_PART_MAIN);
            lv_anim_t down;
            lv_anim_init(&down);
            lv_anim_set_var(&down, dialog_box);
            lv_anim_set_exec_cb(&down, ime_translate_y_anim_cb);
            lv_anim_set_values(&down, cur_y, 0);
            lv_anim_set_time(&down, IME_SLIDE_OUT_MS);
            lv_anim_set_path_cb(&down, lv_anim_path_ease_in);
            lv_anim_start(&down);
        }
    }

    void releaseDialogIMEFont() {
        if (!dialog_ime_font_acquired) return;
        FontManager::releaseIMEFont();
        dialog_ime_font_acquired = false;
    }

    void openFsInfoDialog() {
        closeMenuPanel();
        closeDialog();

        dialog_box = lv_obj_create(screen);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(dialog_box, 198);
        lv_obj_set_height(dialog_box, LV_SIZE_CONTENT);
        lv_obj_center(dialog_box);
        lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(dialog_box, lv_color_hex(0x505050), 0);
        lv_obj_set_style_pad_all(dialog_box, 6, 0);
        lv_obj_set_style_pad_row(dialog_box, 4, 0);
        lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(dialog_box);
        lv_label_set_text(title, "Filesystem Info");
        lv_obj_set_style_text_font(title, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

        char info[220];
        if (active_drive == 'L') {
            uint64_t total = (uint64_t)LittleFS.totalBytes();
            uint64_t used = (uint64_t)LittleFS.usedBytes();
            uint64_t free_b = (total > used) ? (total - used) : 0;
            lv_snprintf(
                info, sizeof(info),
                "Drive: L:\nFS: LittleFS\nUsed: %s / %s (%d%%)\nFree: %s",
                formatBytesHuman(used).c_str(),
                formatBytesHuman(total).c_str(),
                (total > 0) ? (int)((used * 100) / total) : 0,
                formatBytesHuman(free_b).c_str()
            );
        } else if (active_drive == 'D') {
            if (!sd_ready || !StorageHelper::getInstance()->isInitialized()) {
                lv_snprintf(info, sizeof(info), "Drive: D:\nSD unavailable");
            } else {
                uint64_t total = StorageHelper::getInstance()->totalBytes();
                uint64_t used = StorageHelper::getInstance()->usedBytes();
                uint64_t free_b = (total > used) ? (total - used) : 0;
                lv_snprintf(
                    info, sizeof(info),
                    "Drive: D:\nFS: %s\nCard: %s\nUsed: %s / %s (%d%%)\nFree: %s\nSectors: %u",
                    StorageHelper::getInstance()->fsTypeString().c_str(),
                    StorageHelper::getInstance()->cardTypeString().c_str(),
                    formatBytesHuman(used).c_str(),
                    formatBytesHuman(total).c_str(),
                    (total > 0) ? (int)((used * 100) / total) : 0,
                    formatBytesHuman(free_b).c_str(),
                    (unsigned)StorageHelper::getInstance()->sectorCount()
                );
            }
        } else {
            lv_snprintf(info, sizeof(info), "Unknown drive");
        }

        lv_obj_t* txt = lv_label_create(dialog_box);
        lv_label_set_text(txt, info);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
        lv_obj_set_style_text_font(txt, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(txt, lv_color_hex(0xD0E6FF), 0);

        lv_obj_t* row = lv_obj_create(dialog_box);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row, 1, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* ok = lv_button_create(row);
        lv_obj_set_size(ok, 56, 28);
        styleActionButton(ok);
        lv_obj_add_event_cb(ok, dialog_cancel_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* ok_lbl = lv_label_create(ok);
        lv_label_set_text(ok_lbl, "OK");
        lv_obj_center(ok_lbl);
    }

    void openLargeFileWarnDialog(const String& vpath, size_t file_size) {
        closeMenuPanel();
        closeDialog();
        pending_open_vpath = vpath;

        dialog_box = lv_obj_create(screen);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(dialog_box, 198);
        lv_obj_set_height(dialog_box, LV_SIZE_CONTENT);
        lv_obj_center(dialog_box);
        lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(dialog_box, lv_color_hex(0x505050), 0);
        lv_obj_set_style_pad_all(dialog_box, 6, 0);
        lv_obj_set_style_pad_row(dialog_box, 4, 0);
        lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(dialog_box);
        lv_label_set_text(title, "Large file warning");
        lv_obj_set_style_text_font(title, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

        char msg[128];
        lv_snprintf(msg, sizeof(msg), "File size: %u KB\nOpen anyway?", (unsigned)(file_size / 1024));
        lv_obj_t* txt = lv_label_create(dialog_box);
        lv_label_set_text(txt, msg);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
        lv_obj_set_style_text_font(txt, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(txt, lv_color_hex(0xD0E6FF), 0);

        lv_obj_t* row = lv_obj_create(dialog_box);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_column(row, 6, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cancel_btn = lv_button_create(row);
        lv_obj_set_size(cancel_btn, 62, 28);
        styleActionButton(cancel_btn);
        lv_obj_add_event_cb(cancel_btn, dialog_cancel_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_lbl, "Cancel");
        lv_obj_center(cancel_lbl);

        lv_obj_t* open_btn = lv_button_create(row);
        lv_obj_set_size(open_btn, 56, 28);
        styleActionButton(open_btn);
        lv_obj_add_event_cb(open_btn, large_open_confirm_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* open_lbl = lv_label_create(open_btn);
        lv_label_set_text(open_lbl, "Open");
        lv_obj_center(open_lbl);
    }

    void updateNewTypeButtons() {
        if (!dialog_new_file_btn || !dialog_new_dir_btn) return;
        lv_obj_set_style_bg_color(dialog_new_file_btn, new_as_dir ? lv_color_hex(0x1E1E1E) : lv_color_hex(0x5CB8FF), LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(dialog_new_file_btn, new_as_dir ? lv_color_hex(0x404040) : lv_color_hex(0x8ED1FF), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dialog_new_dir_btn, new_as_dir ? lv_color_hex(0x5CB8FF) : lv_color_hex(0x1E1E1E), LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(dialog_new_dir_btn, new_as_dir ? lv_color_hex(0x8ED1FF) : lv_color_hex(0x404040), LV_STATE_DEFAULT);
    }

    void confirmDialog() {
        if (!dialog_input) {
            closeDialog();
            return;
        }
        String name = String(lv_textarea_get_text(dialog_input));
        name.trim();
        if (name.length() == 0) {
            closeDialog();
            return;
        }

        if (dialog_mode == DIALOG_NEW_ENTRY) {
            String base_vpath = String(active_drive) + ":" + joinPath(current_path, name);
            String final_vpath = nextAvailableVPath(base_vpath);
            if (new_as_dir) makeDir(final_vpath);
            else writeTextFile(final_vpath, "");
        } else if (dialog_mode == DIALOG_RENAME) {
            if (selected_vpath.length() > 0) {
                String src = innerPath(selected_vpath);
                String parent = parentPath(src);
                String dst = normalizeInner(parent + "/" + name);
                String dst_v = String(driveOf(selected_vpath)) + ":" + dst;
                if (dst_v != selected_vpath) dst_v = nextAvailableVPath(dst_v);
                renamePath(selected_vpath, dst_v);
                selected_vpath = dst_v;
            }
        }
        closeDialog();
    }

    void applyRemoveMarked(bool force_delete) {
        if (!file_list) return;
        uint32_t count = lv_obj_get_child_count(file_list);
        uint32_t removed = 0;
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* row = lv_obj_get_child(file_list, i);
            EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(row);
            if (meta && meta->marked) {
                if (deletePath(meta->vpath, force_delete)) removed++;
                delay(0);
            }
        }
        selected_vpath = "";
        remove_mode = false;
        closeMenuPanel();
        if (removed) delay(0);
        refreshUi();
    }

    void pasteCopied() {
        if (copied_vpath.length() == 0) return;
        String src_name = baseName(innerPath(copied_vpath));
        String dest = joinPath(current_path, withCopySuffix(src_name));
        String dest_v = String(active_drive) + ":" + dest;
        int idx = 1;
        while (pathExists(dest_v)) {
            dest = joinPath(current_path, withCopySuffix(src_name, idx));
            dest_v = String(active_drive) + ":" + dest;
            idx++;
        }
        copyFile(copied_vpath, dest_v);
        closeMenuPanel();
        refreshUi();
    }

    char driveOf(const String& vpath) const {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            char d = vpath.charAt(0);
            if (d >= 'a' && d <= 'z') d = d - 'a' + 'A';
            return d;
        }
        return active_drive;
    }

    String innerPath(const String& vpath) const {
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            return p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        return p;
    }

    String normalizeInner(const String& p) const {
        if (p.length() == 0) return "/";
        if (p.startsWith("/")) return p;
        return "/" + p;
    }

    String parentPath(const String& p) const {
        if (p == "/") return "/";
        int idx = p.lastIndexOf('/');
        if (idx <= 0) return "/";
        return p.substring(0, idx);
    }

    String withCopySuffix(const String& name, int n = 0) const {
        int dot = name.lastIndexOf('.');
        String stem = (dot > 0) ? name.substring(0, dot) : name;
        String ext = (dot > 0) ? name.substring(dot) : "";
        if (n == 0) return stem + "_copy" + ext;
        return stem + "_copy" + String(n) + ext;
    }

    bool pathExists(const String& vpath) {
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') return LittleFS.exists(p.c_str());
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return StorageHelper::getInstance()->getFs().exists(p.c_str());
        }
        return false;
    }

    bool writeTextFile(const String& vpath, const String& data) {
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') {
            File f = LittleFS.open(p.c_str(), "w");
            if (!f) return false;
            f.write((const uint8_t*)data.c_str(), data.length());
            f.close();
            return true;
        }
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return StorageHelper::getInstance()->writeFile(p.c_str(), data);
        }
        return false;
    }

    bool makeDir(const String& vpath) {
        char d = driveOf(vpath);
        String p = normalizeInner(innerPath(vpath));
        if (p == "/") return false;
        if (d == 'L') return LittleFS.mkdir(p.c_str());
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return StorageHelper::getInstance()->getFs().mkdir(p.c_str(), true);
        }
        return false;
    }

    bool deletePath(const String& vpath, bool force_delete) {
        char d = driveOf(vpath);
        String p = normalizeInner(innerPath(vpath));
        if (p == "/") return false;
        if (!force_delete) {
            if (d == 'L') return deleteLittleFsPathSimple(p);
            if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
                return deleteSdPathSimple(p);
            }
            return false;
        }
        if (d == 'L') return deleteLittleFsPathRecursive(p);
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return deleteSdPathRecursive(p);
        }
        return false;
    }

    bool deleteLittleFsPathSimple(const String& path) {
        File node = LittleFS.open(path.c_str(), "r");
        if (!node) return false;
        bool is_dir = node.isDirectory();
        node.close();
        if (is_dir) return LittleFS.rmdir(path.c_str());
        return LittleFS.remove(path.c_str());
    }

    bool deleteLittleFsPathRecursive(const String& path) {
        File node = LittleFS.open(path.c_str(), "r");
        if (!node) return false;

        if (!node.isDirectory()) {
            node.close();
            return LittleFS.remove(path.c_str());
        }

        bool ok = true;
        std::vector<String> children;
        while (true) {
            File entry = node.openNextFile();
            if (!entry) break;

            String child = normalizeInner(String(entry.name()));
            entry.close();

            if (child.length() == 0 || child == "/") {
                ok = false;
                continue;
            }
            children.push_back(child);
        }
        node.close();

        for (size_t i = 0; i < children.size(); i++) {
            if (!deleteLittleFsPathRecursive(children[i])) ok = false;
            if ((i & 0x03) == 0) delay(0);
        }

        if (!LittleFS.rmdir(path.c_str())) ok = false;
        return ok;
    }

    bool deleteSdPathRecursive(const String& path) {
        SdFs& fs = StorageHelper::getInstance()->getFs();
        struct Node {
            String p;
            bool expanded;
        };

        // Iterative post-order delete to avoid deep recursion stack overflow.
        std::vector<Node> stack;
        stack.push_back({path, false});

        bool ok = true;
        uint32_t ops = 0;
        const uint32_t MAX_OPS = 60000;  // safety guard for malformed trees

        while (!stack.empty()) {
            Node cur = stack.back();
            stack.pop_back();

            FsFile node = fs.open(cur.p.c_str(), O_RDONLY);
            if (!node) {
                ok = false;
                continue;
            }

            if (!node.isDir()) {
                node.close();
                if (!fs.remove(cur.p.c_str())) {
                    ok = false;
                }
                if ((++ops & 0x0F) == 0) delay(0);
                if (ops > MAX_OPS) {
                    return false;
                }
                continue;
            }

            if (cur.expanded) {
                node.close();
                if (!fs.rmdir(cur.p.c_str())) {
                    ok = false;
                }
                if ((++ops & 0x0F) == 0) delay(0);
                if (ops > MAX_OPS) {
                    return false;
                }
                continue;
            }

            // First visit: re-push this dir as expanded, then children.
            stack.push_back({cur.p, true});

            FsFile entry;
            while (entry.openNext(&node, O_RDONLY)) {
                char name[256];
                entry.getName(name, sizeof(name));
                entry.close();

                if (name[0] == '\0') continue;
                if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

                String child = joinPath(cur.p, String(name));
                if (child == cur.p) continue;
                stack.push_back({child, false});
            }
            node.close();

            if ((++ops & 0x0F) == 0) delay(0);
            if (ops > MAX_OPS) {
                return false;
            }
        }
        return ok;
    }

    bool deleteSdPathSimple(const String& path) {
        SdFs& fs = StorageHelper::getInstance()->getFs();
        FsFile node = fs.open(path.c_str(), O_RDONLY);
        if (!node) return false;
        bool is_dir = node.isDir();
        node.close();
        if (is_dir) return fs.rmdir(path.c_str());
        return fs.remove(path.c_str());
    }

    void countMarkedEntries(uint32_t& files, uint32_t& dirs) {
        files = 0;
        dirs = 0;
        if (!file_list) return;
        uint32_t count = lv_obj_get_child_count(file_list);
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* row = lv_obj_get_child(file_list, i);
            if (row == empty_label) continue;
            EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(row);
            if (!meta || !meta->marked) continue;
            if (meta->is_dir) dirs++;
            else files++;
        }
    }

    void openRemoveConfirmDialog() {
        closeMenuPanel();
        closeDialog();
        uint32_t file_cnt = 0;
        uint32_t dir_cnt = 0;
        countMarkedEntries(file_cnt, dir_cnt);

        dialog_box = lv_obj_create(screen);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_FLOATING);
        lv_obj_add_flag(dialog_box, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_set_width(dialog_box, 206);
        lv_obj_set_height(dialog_box, LV_SIZE_CONTENT);
        lv_obj_center(dialog_box);
        lv_obj_set_style_bg_color(dialog_box, lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_color(dialog_box, lv_color_hex(0x505050), 0);
        lv_obj_set_style_pad_all(dialog_box, 6, 0);
        lv_obj_set_style_pad_row(dialog_box, 4, 0);
        lv_obj_set_flex_flow(dialog_box, LV_FLEX_FLOW_COLUMN);
        lv_obj_clear_flag(dialog_box, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* title = lv_label_create(dialog_box);
        lv_label_set_text(title, "Delete selected");
        lv_obj_set_style_text_font(title, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

        lv_obj_t* txt = lv_label_create(dialog_box);
        char msg[192];
        lv_snprintf(
            msg, sizeof(msg),
            "Selected: %u files, %u dirs\nNormal: non-empty folders fail.\nForce: recursive delete.",
            (unsigned)file_cnt, (unsigned)dir_cnt
        );
        lv_label_set_text(txt, msg);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
        lv_obj_set_style_text_font(txt, FontManager::iconFont(), 0);
        lv_obj_set_style_text_color(txt, lv_color_hex(0xD0E6FF), 0);

        lv_obj_t* row = lv_obj_create(dialog_box);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_left(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_right(row, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_top(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(row, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_column(row, 4, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* cancel_btn = lv_button_create(row);
        lv_obj_set_size(cancel_btn, 60, 28);
        styleActionButton(cancel_btn);
        lv_obj_add_event_cb(cancel_btn, dialog_cancel_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* cancel_lbl = lv_label_create(cancel_btn);
        lv_label_set_text(cancel_lbl, "Cancel");
        lv_obj_center(cancel_lbl);

        lv_obj_t* normal_btn = lv_button_create(row);
        lv_obj_set_size(normal_btn, 60, 28);
        styleActionButton(normal_btn);
        lv_obj_add_event_cb(normal_btn, remove_normal_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* normal_lbl = lv_label_create(normal_btn);
        lv_label_set_text(normal_lbl, "Normal");
        lv_obj_center(normal_lbl);

        lv_obj_t* force_btn = lv_button_create(row);
        lv_obj_set_size(force_btn, 52, 28);
        styleActionButton(force_btn);
        lv_obj_add_event_cb(force_btn, remove_force_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* force_lbl = lv_label_create(force_btn);
        lv_label_set_text(force_lbl, "Force");
        lv_obj_center(force_lbl);
    }

    bool renamePath(const String& from_vpath, const String& to_vpath) {
        char d = driveOf(from_vpath);
        String from = innerPath(from_vpath);
        String to = innerPath(to_vpath);
        if (d == 'L') return LittleFS.rename(from.c_str(), to.c_str());
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return StorageHelper::getInstance()->getFs().rename(from.c_str(), to.c_str());
        }
        return false;
    }

    bool copyFile(const String& src_vpath, const String& dst_vpath) {
        char sd = driveOf(src_vpath);
        char dd = driveOf(dst_vpath);
        String sp = innerPath(src_vpath);
        String dp = innerPath(dst_vpath);
        static constexpr size_t CHUNK = 1024;
        uint8_t buf[CHUNK];

        if (sd == 'L' && dd == 'L') {
            File in = LittleFS.open(sp.c_str(), "r");
            if (!in || in.isDirectory()) return false;
            File out = LittleFS.open(dp.c_str(), "w");
            if (!out) { in.close(); return false; }
            while (true) {
                int n = in.read(buf, CHUNK);
                if (n <= 0) break;
                if ((int)out.write(buf, (size_t)n) != n) { in.close(); out.close(); return false; }
                delay(0);
            }
            in.close();
            out.close();
            return true;
        }
        if (sd == 'D' && dd == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            SdFs& fs = StorageHelper::getInstance()->getFs();
            FsFile in = fs.open(sp.c_str(), O_RDONLY);
            if (!in || in.isDir()) return false;
            FsFile out = fs.open(dp.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            if (!out) { in.close(); return false; }
            while (true) {
                int n = in.read(buf, CHUNK);
                if (n <= 0) break;
                if ((int)out.write(buf, (size_t)n) != n) { in.close(); out.close(); return false; }
                delay(0);
            }
            in.close();
            out.close();
            return true;
        }
        if (sd == 'L' && dd == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            File in = LittleFS.open(sp.c_str(), "r");
            if (!in || in.isDirectory()) return false;
            SdFs& fs = StorageHelper::getInstance()->getFs();
            FsFile out = fs.open(dp.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            if (!out) { in.close(); return false; }
            while (true) {
                int n = in.read(buf, CHUNK);
                if (n <= 0) break;
                if ((int)out.write(buf, (size_t)n) != n) { in.close(); out.close(); return false; }
                delay(0);
            }
            in.close();
            out.close();
            return true;
        }
        if (sd == 'D' && dd == 'L' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            SdFs& fs = StorageHelper::getInstance()->getFs();
            FsFile in = fs.open(sp.c_str(), O_RDONLY);
            if (!in || in.isDir()) return false;
            File out = LittleFS.open(dp.c_str(), "w");
            if (!out) { in.close(); return false; }
            while (true) {
                int n = in.read(buf, CHUNK);
                if (n <= 0) break;
                if ((int)out.write(buf, (size_t)n) != n) { in.close(); out.close(); return false; }
                delay(0);
            }
            in.close();
            out.close();
            return true;
        }
        return false;
    }

    bool readTextFile(const String& vpath, String& out) {
        out = "";
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') {
            File f = LittleFS.open(p.c_str(), "r");
            if (!f) return false;
            while (f.available()) {
                int c = f.read();
                if (c < 0) break;
                out += (char)c;
            }
            f.close();
            return true;
        }
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            return StorageHelper::getInstance()->readFile(p.c_str(), out);
        }
        return false;
    }

    size_t getFileSize(const String& vpath) {
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') {
            File f = LittleFS.open(p.c_str(), "r");
            if (!f) return 0;
            size_t sz = (size_t)f.size();
            f.close();
            return sz;
        }
        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            FsFile f = StorageHelper::getInstance()->getFs().open(p.c_str(), O_RDONLY);
            if (!f) return 0;
            size_t sz = (size_t)f.fileSize();
            f.close();
            return sz;
        }
        return 0;
    }

    String appendIndexToName(const String& name, int idx) const {
        int dot = name.lastIndexOf('.');
        if (dot > 0) {
            String stem = name.substring(0, dot);
            String ext = name.substring(dot);
            return stem + "(" + String(idx) + ")" + ext;
        }
        return name + "(" + String(idx) + ")";
    }

    String nextAvailableVPath(const String& base_vpath) {
        if (!pathExists(base_vpath)) return base_vpath;

        char d = driveOf(base_vpath);
        String inner = innerPath(base_vpath);
        String parent = parentPath(inner);
        String name = baseName(inner);

        int idx = 1;
        while (idx < 1000) {
            String candidate_name = appendIndexToName(name, idx);
            String candidate_inner = joinPath(parent, candidate_name);
            String candidate_v = String(d) + ":" + candidate_inner;
            if (!pathExists(candidate_v)) return candidate_v;
            idx++;
        }
        return base_vpath;
    }

    String baseName(const String& full) const {
        int idx = full.lastIndexOf('/');
        if (idx < 0) return full;
        if (idx == (int)full.length() - 1) return "";
        return full.substring(idx + 1);
    }

    String joinPath(const String& base, const String& name) const {
        if (base == "/") return "/" + name;
        return base + "/" + name;
    }

    String formatBytesHuman(uint64_t bytes) const {
        static const uint64_t KB = 1024ULL;
        static const uint64_t MB = 1024ULL * 1024ULL;
        static const uint64_t GB = 1024ULL * 1024ULL * 1024ULL;
        char buf[32];
        if (bytes >= GB) {
            uint64_t t = (bytes * 10ULL) / GB;
            lv_snprintf(buf, sizeof(buf), "%u.%u GB", (unsigned)(t / 10ULL), (unsigned)(t % 10ULL));
        } else if (bytes >= MB) {
            uint64_t t = (bytes * 10ULL) / MB;
            lv_snprintf(buf, sizeof(buf), "%u.%u MB", (unsigned)(t / 10ULL), (unsigned)(t % 10ULL));
        } else if (bytes >= KB) {
            uint64_t t = (bytes * 10ULL) / KB;
            lv_snprintf(buf, sizeof(buf), "%u.%u KB", (unsigned)(t / 10ULL), (unsigned)(t % 10ULL));
        } else {
            lv_snprintf(buf, sizeof(buf), "%u B", (unsigned)bytes);
        }
        return String(buf);
    }
};

#endif
