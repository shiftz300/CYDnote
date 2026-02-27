#ifndef FILES_H
#define FILES_H

#include <Arduino.h>
#include <LittleFS.h>
#include <functional>
#include <vector>
#include <cctype>
#include <string.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../utils/storage.h"
#include "fonts.h"
#include "../ime/pinyin.h"

class FileManager {
private:
    static constexpr uint16_t SCREEN_TRANSITION_MS = 55;
    static constexpr uint16_t IME_SLIDE_IN_MS = 90;
    static constexpr uint16_t IME_SLIDE_OUT_MS = 80;
    static constexpr int32_t DIALOG_LIFT_Y = -84;
    static constexpr int32_t IME_CANDIDATE_H = 28;
    static constexpr int32_t IME_KEYBOARD_H = 132;
    static constexpr int32_t INPUT_TEXT_SIZE_PX = 14;
    static constexpr int32_t IME_TOTAL_H_FALLBACK = IME_CANDIDATE_H + IME_KEYBOARD_H;
    static constexpr uint8_t IME_PROXY_CAND_MAX = 20;
    static constexpr size_t COPY_IO_CHUNK = 24576;
    static constexpr uint8_t COPY_CHUNKS_PER_TICK = 12;
    enum FsWorkerJobType {
        FS_WORK_NONE = 0,
        FS_WORK_COPY_DIR = 1,
        FS_WORK_DELETE_BATCH = 2,
        FS_WORK_COPY_FILE = 3,
        FS_WORK_CREATE_FILE = 4,
        FS_WORK_CREATE_DIR = 5,
        FS_WORK_RENAME = 6,
        FS_WORK_SCAN_DIR = 7,
    };
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
    struct FileListItem {
        String name;
        bool is_dir;
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
    lv_obj_t* share_btn_ref;
    lv_obj_t* drive_btn_l;
    lv_obj_t* drive_btn_d;
    lv_obj_t* menu_panel;
    lv_obj_t* menu_copy_btn;
    lv_obj_t* menu_move_btn;
    lv_obj_t* menu_paste_btn;
    lv_obj_t* menu_paste_label;
    lv_obj_t* menu_paste_progress_track;
    lv_obj_t* menu_paste_progress_bg;
    lv_obj_t* menu_copy_cancel_btn;
    lv_obj_t* menu_copy_cancel_label;
    lv_timer_t* copy_timer;
    lv_timer_t* delete_timer;
    lv_timer_t* fs_job_timer;
    char copy_src_drive;
    char copy_dst_drive;
    String copy_src_inner;
    String copy_dst_inner;
    size_t copy_total_bytes;
    volatile size_t copy_done_bytes;
    size_t copy_total_files;
    volatile size_t copy_done_files;
    bool copy_is_dir_job;
    File copy_src_lfs;
    File copy_dst_lfs;
    FsFile copy_src_sdfs;
    FsFile copy_dst_sdfs;
    uint8_t copy_buf[COPY_IO_CHUNK];
    lv_obj_t* dialog_box;
    lv_obj_t* dialog_input;
    lv_obj_t* dialog_ime_container;
    lv_obj_t* dialog_ime;
    lv_obj_t* dialog_keyboard;
    lv_obj_t* dialog_ime_cand_proxy;
    lv_obj_t* dialog_ime_cand_src;
    lv_obj_t* dialog_new_file_btn;
    lv_obj_t* dialog_new_dir_btn;
    lv_obj_t* share_info_label;
    lv_obj_t* share_action_btn;
    lv_obj_t* share_action_label;
    bool dialog_ime_font_acquired;
    char dialog_ime_cand_texts[IME_PROXY_CAND_MAX][8];
    const char* dialog_ime_cand_map[IME_PROXY_CAND_MAX + 1];
    uint8_t dialog_ime_cand_src_idx[IME_PROXY_CAND_MAX];
    bool dialog_ime_cand_syncing;
    std::function<void(const char*)> on_open_cb;
    std::function<bool()> on_share_ap_toggle_cb;
    std::function<bool()> on_share_ap_running_cb;
    std::function<String()> on_share_ap_status_cb;

    bool sd_ready;
    bool remove_mode;
    bool copy_pick_mode;
    bool move_pick_mode;
    char active_drive;
    String current_path;
    String selected_vpath;
    String copied_vpath;
    String moved_vpath;
    String pending_open_vpath;
    bool new_as_dir;
    volatile bool copy_cancel_requested;
    bool copy_in_progress;
    bool delete_in_progress;
    bool copy_dir_worker_mode;
    bool fs_job_in_progress;
    uint32_t copy_started_ms;
    TaskHandle_t fs_worker_task;
    volatile FsWorkerJobType fs_worker_job;
    volatile bool fs_worker_busy;
    volatile bool fs_worker_done;
    volatile bool fs_worker_ok;
    volatile size_t fs_worker_delete_done;
    volatile size_t fs_worker_delete_removed;
    size_t fs_worker_delete_total;
    bool fs_worker_delete_force;
    std::vector<String> fs_worker_delete_paths;
    String fs_worker_src_vpath;
    String fs_worker_dst_vpath;
    String fs_worker_arg1;
    String fs_worker_arg2;
    std::vector<FileListItem> fs_worker_scan_items;
    String fs_worker_scan_vpath;
    bool scan_in_progress;
    bool scan_result_ready;
    bool scan_result_ok;
    DialogMode dialog_mode;
    uint32_t fs_usage_last_ms;
    int fs_usage_last_pct;
    bool fs_usage_last_valid;
    bool list_suspended_for_dialog;
    bool reset_scroll_pending;
    uint32_t dialog_ime_cursor_anchor_pos;
    bool dialog_ime_cursor_anchor_valid;

public:
    FileManager()
        : screen(nullptr), sidebar(nullptr), breadcrumb_wrap(nullptr), file_list(nullptr),
          empty_label(nullptr), fs_bar(nullptr), fs_label(nullptr), fs_panel(nullptr), up_btn_ref(nullptr), share_btn_ref(nullptr), drive_btn_l(nullptr), drive_btn_d(nullptr), menu_panel(nullptr), menu_copy_btn(nullptr), menu_move_btn(nullptr), menu_paste_btn(nullptr), menu_paste_label(nullptr), menu_paste_progress_track(nullptr), menu_paste_progress_bg(nullptr), menu_copy_cancel_btn(nullptr), menu_copy_cancel_label(nullptr), copy_timer(nullptr), delete_timer(nullptr), fs_job_timer(nullptr), copy_src_drive('L'), copy_dst_drive('L'), copy_src_inner(""), copy_dst_inner(""), copy_total_bytes(0), copy_done_bytes(0), copy_total_files(0), copy_done_files(0), copy_is_dir_job(false), dialog_box(nullptr), dialog_input(nullptr),
          dialog_ime_container(nullptr), dialog_ime(nullptr), dialog_keyboard(nullptr), dialog_ime_cand_proxy(nullptr), dialog_ime_cand_src(nullptr),
          dialog_new_file_btn(nullptr), dialog_new_dir_btn(nullptr), share_info_label(nullptr), share_action_btn(nullptr), share_action_label(nullptr),
          dialog_ime_font_acquired(false), dialog_ime_cand_syncing(false),
          on_open_cb(nullptr), on_share_ap_toggle_cb(nullptr), on_share_ap_running_cb(nullptr), on_share_ap_status_cb(nullptr),
          sd_ready(false), remove_mode(false), copy_pick_mode(false), move_pick_mode(false), active_drive('L'),
          current_path("/"), selected_vpath(""), copied_vpath(""), moved_vpath(""), pending_open_vpath(""), new_as_dir(false), copy_cancel_requested(false), copy_in_progress(false), delete_in_progress(false), copy_dir_worker_mode(false), fs_job_in_progress(false), copy_started_ms(0), fs_worker_task(nullptr), fs_worker_job(FS_WORK_NONE), fs_worker_busy(false), fs_worker_done(false), fs_worker_ok(false), fs_worker_delete_done(0), fs_worker_delete_removed(0), fs_worker_delete_total(0), fs_worker_delete_force(false), fs_worker_src_vpath(""), fs_worker_dst_vpath(""), fs_worker_arg1(""), fs_worker_arg2(""), fs_worker_scan_items(), fs_worker_scan_vpath(""), scan_in_progress(false), scan_result_ready(false), scan_result_ok(false), dialog_mode(DIALOG_NONE),
          fs_usage_last_ms(0), fs_usage_last_pct(0), fs_usage_last_valid(false), list_suspended_for_dialog(false), reset_scroll_pending(true),
          dialog_ime_cursor_anchor_pos(0), dialog_ime_cursor_anchor_valid(false) {
        memset(dialog_ime_cand_texts, 0, sizeof(dialog_ime_cand_texts));
        memset(dialog_ime_cand_map, 0, sizeof(dialog_ime_cand_map));
        memset(dialog_ime_cand_src_idx, 0xFF, sizeof(dialog_ime_cand_src_idx));
    }

    void create(bool has_sd, std::function<void(const char*)> on_open = nullptr,
                std::function<bool()> on_share_ap_toggle = nullptr,
                std::function<bool()> on_share_ap_running = nullptr,
                std::function<String()> on_share_ap_status = nullptr) {
        on_open_cb = on_open;
        on_share_ap_toggle_cb = on_share_ap_toggle;
        on_share_ap_running_cb = on_share_ap_running;
        on_share_ap_status_cb = on_share_ap_status;
        sd_ready = has_sd;
        active_drive = 'L';
        current_path = "/";
        selected_vpath = "";
        pending_open_vpath = "";
        remove_mode = false;
        copy_pick_mode = false;
        move_pick_mode = false;
        new_as_dir = false;
        dialog_mode = DIALOG_NONE;
        list_suspended_for_dialog = false;
        reset_scroll_pending = true;

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
        lv_obj_set_style_pad_row(sidebar, 2, 0);
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

        lv_obj_t* info_btn = lv_obj_create(sidebar);
        lv_obj_set_width(info_btn, lv_pct(100));
        lv_obj_set_height(info_btn, 26);
        lv_obj_set_style_pad_all(info_btn, 1, 0);
        lv_obj_set_style_pad_row(info_btn, 0, 0);
        lv_obj_set_style_radius(info_btn, 4, 0);
        lv_obj_set_style_clip_corner(info_btn, true, 0);
        lv_obj_set_style_bg_color(info_btn, lv_color_hex(0x090909), 0);
        lv_obj_set_style_border_color(info_btn, lv_color_hex(0x303030), 0);
        lv_obj_add_flag(info_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(info_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* info_lbl = lv_label_create(info_btn);
        lv_label_set_text(info_lbl, "i");
        lv_obj_set_style_text_font(info_lbl, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(info_lbl, lv_color_hex(0xBFDFFF), 0);
        lv_obj_center(info_lbl);
        lv_obj_add_event_cb(info_btn, sidebar_info_event_cb, LV_EVENT_CLICKED, this);

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

        lv_obj_t* share_btn = lv_button_create(control_row);
        lv_obj_set_size(share_btn, 26, 26);
        styleActionButton(share_btn);
        share_btn_ref = share_btn;
        lv_obj_t* share_lbl = lv_label_create(share_btn);
        lv_label_set_text(share_lbl, LV_SYMBOL_UPLOAD);
        lv_obj_set_style_text_font(share_lbl, FontManager::iconFont(), 0);
        lv_obj_center(share_lbl);
        lv_obj_add_event_cb(share_btn, share_btn_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_move_to_index(share_btn, lv_obj_get_index(menu_btn));

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
        menu_move_btn = addMenuAction(menu_panel, LV_SYMBOL_RIGHT " Move", menu_move_event_cb);
        menu_paste_btn = addMenuAction(menu_panel, LV_SYMBOL_PASTE " Paste", menu_paste_event_cb);
        menu_paste_label = lv_obj_get_child(menu_paste_btn, 0);
        menu_paste_progress_track = lv_obj_create(menu_paste_btn);
        lv_obj_remove_style_all(menu_paste_progress_track);
        lv_obj_set_size(menu_paste_progress_track, lv_pct(100), 18);
        lv_obj_align(menu_paste_progress_track, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(menu_paste_progress_track, lv_color_hex(0x2A2A2A), 0);
        lv_obj_set_style_bg_opa(menu_paste_progress_track, LV_OPA_80, 0);
        lv_obj_set_style_radius(menu_paste_progress_track, 3, 0);
        lv_obj_add_flag(menu_paste_progress_track, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_to_index(menu_paste_progress_track, 0);

        menu_paste_progress_bg = lv_obj_create(menu_paste_progress_track);
        lv_obj_remove_style_all(menu_paste_progress_bg);
        lv_obj_set_size(menu_paste_progress_bg, 0, 18);
        lv_obj_set_align(menu_paste_progress_bg, LV_ALIGN_LEFT_MID);
        lv_obj_set_style_bg_color(menu_paste_progress_bg, lv_color_hex(0x5CB8FF), 0);
        lv_obj_set_style_bg_opa(menu_paste_progress_bg, LV_OPA_90, 0);
        lv_obj_set_style_radius(menu_paste_progress_bg, 3, 0);
        if (menu_paste_label) lv_obj_move_foreground(menu_paste_label);
        menu_copy_cancel_btn = addMenuAction(menu_panel, LV_SYMBOL_CLOSE " Cancel", menu_copy_cancel_event_cb);
        menu_copy_cancel_label = lv_obj_get_child(menu_copy_cancel_btn, 0);
        lv_obj_add_flag(menu_copy_cancel_btn, LV_OBJ_FLAG_HIDDEN);
        addMenuAction(menu_panel, LV_SYMBOL_EDIT " Rename", menu_rename_event_cb);
        lv_obj_add_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
        updateMenuActionStates();

        refreshUi();
    }

    void destroy() {
        cancelCopyJob(true);
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
            share_btn_ref = nullptr;
            drive_btn_l = nullptr;
            drive_btn_d = nullptr;
            menu_panel = nullptr;
            menu_copy_btn = nullptr;
            menu_move_btn = nullptr;
            menu_paste_btn = nullptr;
            menu_paste_label = nullptr;
            menu_paste_progress_track = nullptr;
            menu_paste_progress_bg = nullptr;
            menu_copy_cancel_btn = nullptr;
            menu_copy_cancel_label = nullptr;
            copy_timer = nullptr;
            dialog_box = nullptr;
            dialog_input = nullptr;
            dialog_ime_container = nullptr;
            dialog_ime = nullptr;
            dialog_keyboard = nullptr;
            dialog_ime_cand_proxy = nullptr;
            dialog_ime_cand_src = nullptr;
            share_info_label = nullptr;
            share_action_btn = nullptr;
            share_action_label = nullptr;
            dialog_ime_font_acquired = false;
        }
    }

    void show(lv_screen_load_anim_t anim = LV_SCR_LOAD_ANIM_NONE) {
        if (screen) {
            if(anim == LV_SCR_LOAD_ANIM_NONE) lv_scr_load(screen);
            else lv_screen_load_anim(screen, anim, SCREEN_TRANSITION_MS, 0, false);
        }
    }

    bool isCopyInProgress() const { return copy_in_progress; }
    bool isFsBusy() const { return copy_in_progress || delete_in_progress || fs_job_in_progress || scan_in_progress; }

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
        fm->reset_scroll_pending = true;
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
        fm->reset_scroll_pending = true;
        fm->refreshUi();
    }

    static void crumb_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (!fm || !btn) return;
        CrumbMeta* meta = (CrumbMeta*)lv_obj_get_user_data(btn);
        if (!meta) return;
        fm->current_path = meta->path;
        fm->reset_scroll_pending = true;
        fm->refreshUi();
    }

    static void file_item_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
        if (!fm || !btn) return;
        if (fm->delete_in_progress || fm->fs_job_in_progress) return;
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
            fm->copied_vpath = meta->vpath;
            fm->selected_vpath = meta->vpath;
            fm->updateMenuActionStates();
            fm->copy_pick_mode = false;
            fm->closeMenuPanel();
            fm->refreshSelectionHighlight();
            return;
        }
        if (fm->move_pick_mode) {
            fm->moved_vpath = meta->vpath;
            fm->selected_vpath = meta->vpath;
            fm->updateMenuActionStates();
            fm->move_pick_mode = false;
            fm->closeMenuPanel();
            fm->refreshSelectionHighlight();
            return;
        }

        // Single tap: select only. Tap the same row again to open.
        if (!same_selected) return;

        if (meta->is_dir) {
            fm->current_path = fm->joinPath(fm->current_path, meta->name);
            fm->selected_vpath = "";
            fm->reset_scroll_pending = true;
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

    static void share_btn_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->openShareDialog();
    }

    static void menu_new_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
        fm->new_as_dir = false;
        fm->openInputDialog(DIALOG_NEW_ENTRY, "Create new", "note.txt");
    }

    static void menu_remove_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->delete_in_progress || fm->fs_job_in_progress) return;
        fm->exitCopyPickModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
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
        if (fm->delete_in_progress || fm->fs_job_in_progress) return;
        fm->exitRemoveModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
        fm->moved_vpath = "";
        if (fm->selected_vpath.length() > 0) {
            // Use current selection as copy source immediately.
            fm->copied_vpath = fm->selected_vpath;
            fm->copy_pick_mode = false;
            fm->updateMenuActionStates();
        } else {
            // Fallback: enter pick mode if nothing is selected yet.
            fm->copy_pick_mode = true;
        }
        fm->closeMenuPanel();
        fm->refreshSelectionHighlight();
    }

    static void menu_move_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->delete_in_progress || fm->fs_job_in_progress || fm->copy_in_progress) return;
        fm->exitRemoveModeIfNeeded(false);
        fm->exitCopyPickModeIfNeeded(false);
        fm->copied_vpath = "";
        if (fm->selected_vpath.length() > 0) {
            fm->moved_vpath = fm->selected_vpath;
            fm->move_pick_mode = false;
        } else {
            fm->move_pick_mode = true;
        }
        fm->updateMenuActionStates();
        fm->closeMenuPanel();
        fm->refreshSelectionHighlight();
    }

    static void menu_paste_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->copy_in_progress || fm->delete_in_progress || fm->fs_job_in_progress) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
        if (fm->moved_vpath.length() > 0) fm->moveSelected();
        else fm->pasteCopied();
    }

    static void menu_rename_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
        if (fm->selected_vpath.length() == 0) return;
        String cur = fm->baseName(fm->innerPath(fm->selected_vpath));
        fm->openInputDialog(DIALOG_RENAME, "Rename to", cur.c_str());
    }

    static void menu_copy_cancel_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->copy_cancel_requested = true;
    }

    static void sidebar_info_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->exitRemoveModeIfNeeded(true);
        fm->exitCopyPickModeIfNeeded(false);
        fm->exitMovePickModeIfNeeded(false);
        if (fm->selected_vpath.length() == 0) return;
        fm->openEntryInfoDialog(fm->selected_vpath);
    }

    static void copy_timer_cb(lv_timer_t* t) {
        FileManager* fm = (FileManager*)lv_timer_get_user_data(t);
        if (!fm) return;
        fm->stepCopyJob();
    }

    static void delete_timer_cb(lv_timer_t* t) {
        FileManager* fm = (FileManager*)lv_timer_get_user_data(t);
        if (!fm) return;
        fm->stepDeleteJob();
    }

    static void fs_job_timer_cb(lv_timer_t* t) {
        FileManager* fm = (FileManager*)lv_timer_get_user_data(t);
        if (!fm) return;
        fm->stepFsJob();
    }

    static void fs_worker_task_entry(void* arg) {
        FileManager* fm = (FileManager*)arg;
        if (!fm) {
            vTaskDelete(nullptr);
            return;
        }
        while (true) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            bool ok = false;
            fm->fs_worker_busy = true;
            fm->fs_worker_done = false;
            if (fm->fs_worker_job == FS_WORK_COPY_DIR) {
                ok = fm->copyDirectoryRecursive(fm->fs_worker_src_vpath, fm->fs_worker_dst_vpath);
            } else if (fm->fs_worker_job == FS_WORK_DELETE_BATCH) {
                ok = true;
                fm->fs_worker_delete_removed = 0;
                for (size_t i = 0; i < fm->fs_worker_delete_paths.size(); i++) {
                    if (fm->deletePath(fm->fs_worker_delete_paths[i], fm->fs_worker_delete_force)) {
                        fm->fs_worker_delete_removed++;
                    }
                    fm->fs_worker_delete_done = i + 1;
                    delay(0);
                }
            } else if (fm->fs_worker_job == FS_WORK_COPY_FILE) {
                ok = fm->copyFile(fm->fs_worker_src_vpath, fm->fs_worker_dst_vpath, fm->copy_total_bytes, false);
            } else if (fm->fs_worker_job == FS_WORK_CREATE_FILE) {
                ok = fm->writeTextFile(fm->fs_worker_arg1, "");
            } else if (fm->fs_worker_job == FS_WORK_CREATE_DIR) {
                ok = fm->makeDir(fm->fs_worker_arg1);
            } else if (fm->fs_worker_job == FS_WORK_RENAME) {
                ok = fm->renamePath(fm->fs_worker_arg1, fm->fs_worker_arg2);
            } else if (fm->fs_worker_job == FS_WORK_SCAN_DIR) {
                ok = fm->scanDirectoryIntoWorker(fm->fs_worker_scan_vpath);
            }
            fm->fs_worker_ok = ok;
            fm->fs_worker_busy = false;
            fm->fs_worker_done = true;
        }
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

    static void share_action_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        if (fm->on_share_ap_toggle_cb) {
            fm->on_share_ap_toggle_cb();
            fm->refreshShareDialog();
        }
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
        fm->dialog_ime_cand_proxy = nullptr;
        fm->dialog_ime_cand_src = nullptr;
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
        if (fm->dialog_mode == DIALOG_NEW_ENTRY && fm->dialog_input) {
            lv_textarea_set_text(fm->dialog_input, "note.txt");
            lv_textarea_set_cursor_pos(fm->dialog_input, LV_TEXTAREA_CURSOR_LAST);
        }
        fm->updateNewTypeButtons();
    }

    static void new_type_dir_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm) return;
        fm->new_as_dir = true;
        if (fm->dialog_mode == DIALOG_NEW_ENTRY && fm->dialog_input) {
            lv_textarea_set_text(fm->dialog_input, "folder");
            lv_textarea_set_cursor_pos(fm->dialog_input, LV_TEXTAREA_CURSOR_LAST);
        }
        fm->updateNewTypeButtons();
    }

    static void dialog_input_cursor_anchor_event_cb(lv_event_t* e) {
        FileManager* fm = (FileManager*)lv_event_get_user_data(e);
        if (!fm || !fm->dialog_input) return;
        fm->dialog_ime_cursor_anchor_pos = lv_textarea_get_cursor_pos(fm->dialog_input);
        fm->dialog_ime_cursor_anchor_valid = true;
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

    void showEmptyState(const char* text) {
        lv_label_set_text(empty_label, text);
        lv_obj_remove_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_center(empty_label);
    }

    void applyScanItemsToList(bool ok) {
        clearList();
        bool has_items = false;
        if (ok) {
            sortEntryItems(fs_worker_scan_items);
            for (size_t i = 0; i < fs_worker_scan_items.size(); i++) {
                addEntryButton(fs_worker_scan_items[i].name, fs_worker_scan_items[i].is_dir);
            }
            has_items = !fs_worker_scan_items.empty();
        }
        if (!has_items) showEmptyState(remove_mode ? "(remove mode, no items)" : "(empty)");
        else lv_obj_add_flag(empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    void applyResetScrollIfNeeded() {
        if (!reset_scroll_pending) return;
        lv_obj_scroll_to_y(file_list, 0, LV_ANIM_OFF);
        reset_scroll_pending = false;
    }

    void reloadEntries() {
        if (!file_list || !empty_label) return;

        if (active_drive == 'D') {
            fs_worker_scan_items.clear();
            bool ok = scanDirectoryIntoWorker(String('D') + ":" + current_path);
            applyScanItemsToList(ok);
            fs_worker_scan_items.clear();
            applyResetScrollIfNeeded();
            return;
        }

        if (scan_result_ready) {
            applyScanItemsToList(scan_result_ok);
            scan_result_ready = false;
            fs_worker_scan_items.clear();
            applyResetScrollIfNeeded();
            return;
        }

        if (!scan_in_progress) {
            String v = String(active_drive) + ":" + current_path;
            if (!startScanJob(v)) {
                if (lv_obj_get_child_count(file_list) <= 1) showEmptyState(remove_mode ? "(remove mode, no items)" : "(empty)");
                return;
            }
            // Keep old list visible while background scan is running.
            if (lv_obj_get_child_count(file_list) <= 1) {
                showEmptyState("");
            }
        }
    }

    static uint8_t sortRankChar(uint8_t c) {
        if (c >= '0' && c <= '9') return 0;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return 1;
        if (c < 0x80) return 2;   // ASCII symbols
        return 3;                 // non-ASCII bytes (UTF-8)
    }

    bool scanDirectoryIntoWorker(const String& vpath) {
        fs_worker_scan_items.clear();
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') {
            File dir = LittleFS.open(p.c_str(), "r");
            if (!dir || !dir.isDirectory()) return false;
            uint32_t iter = 0;
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                String name = baseName(entry.name());
                if (name.length() > 0) {
                    FileListItem it;
                    it.name = name;
                    it.is_dir = entry.isDirectory();
                    fs_worker_scan_items.push_back(it);
                }
                entry.close();
                if ((++iter & 0x0F) == 0) delay(0);
            }
            dir.close();
            return true;
        }
        if (d == 'D') {
            if (!sd_ready || !StorageHelper::getInstance()->isInitialized()) return false;
            FsFile dir = StorageHelper::getInstance()->getFs().open(p.c_str(), O_RDONLY);
            if (!dir || !dir.isDir()) return false;
            uint32_t iter = 0;
            FsFile entry;
            while (entry.openNext(&dir, O_RDONLY)) {
                char name[256];
                entry.getName(name, sizeof(name));
                if (name[0] != '\0') {
                    FileListItem it;
                    it.name = String(name);
                    it.is_dir = entry.isDir();
                    fs_worker_scan_items.push_back(it);
                }
                entry.close();
                if ((++iter & 0x0F) == 0) delay(0);
            }
            dir.close();
            return true;
        }
        return false;
    }

    bool usesSdPath(const String& vpath) const {
        return driveOf(vpath) == 'D';
    }

    bool hasAnySdPath(const std::vector<String>& vpaths) const {
        for (size_t i = 0; i < vpaths.size(); i++) {
            if (usesSdPath(vpaths[i])) return true;
        }
        return false;
    }

    static bool lessName09az(const String& a, const String& b) {
        const size_t la = a.length();
        const size_t lb = b.length();
        const size_t n = (la < lb) ? la : lb;
        for (size_t i = 0; i < n; i++) {
            uint8_t ca = (uint8_t)a.charAt((unsigned int)i);
            uint8_t cb = (uint8_t)b.charAt((unsigned int)i);
            uint8_t ra = sortRankChar(ca);
            uint8_t rb = sortRankChar(cb);
            if (ra != rb) return ra < rb;

            if (ra == 1) { // letters: case-insensitive compare
                ca = (uint8_t)tolower((int)ca);
                cb = (uint8_t)tolower((int)cb);
            }
            if (ca != cb) return ca < cb;
        }
        return la < lb;
    }

    static void sortEntryItems(std::vector<FileListItem>& items) {
        std::sort(items.begin(), items.end(), [](const FileListItem& a, const FileListItem& b) {
            if (a.is_dir != b.is_dir) return a.is_dir && !b.is_dir; // folders first, then files
            return lessName09az(a.name, b.name);
        });
    }

    void addEntryButton(const String& name, bool is_dir) {
        lv_obj_t* row = lv_button_create(file_list);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, LV_SIZE_CONTENT);
        lv_obj_set_style_radius(row, 4, 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x101010), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x173248), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(row, lv_color_hex(0x1E4B6D), LV_STATE_FOCUSED);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(0x2C2C2C), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_hor(row, 4, 0);
        lv_obj_set_style_pad_ver(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

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
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_flex_grow(lbl, 1);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_pad_top(lbl, 0, 0);
        lv_obj_set_style_pad_bottom(lbl, 0, 0);
        lv_obj_set_style_text_line_space(lbl, -2, 0);
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
        bool fs_busy = copy_in_progress || delete_in_progress || fs_job_in_progress || scan_in_progress;

        // SD usage probing can be expensive (freeClusterCount scan). Throttle it.
        if (active_drive == 'D') {
            if (fs_busy) {
                if (fs_usage_last_valid) {
                    lv_bar_set_value(fs_bar, fs_usage_last_pct, LV_ANIM_OFF);
                    char quick[16];
                    lv_snprintf(quick, sizeof(quick), "%d\n%%", fs_usage_last_pct);
                    lv_label_set_text(fs_label, quick);
                }
                return;
            }
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
        bool busy = copy_in_progress || delete_in_progress || fs_job_in_progress;
        if (menu_copy_btn) {
            if (busy) lv_obj_add_state(menu_copy_btn, LV_STATE_DISABLED);
            else lv_obj_clear_state(menu_copy_btn, LV_STATE_DISABLED);
        }
        if (menu_move_btn) {
            if (busy) lv_obj_add_state(menu_move_btn, LV_STATE_DISABLED);
            else lv_obj_clear_state(menu_move_btn, LV_STATE_DISABLED);
        }
        if (menu_paste_btn) {
            if (busy) lv_obj_add_state(menu_paste_btn, LV_STATE_DISABLED);
            else if (copied_vpath.length() > 0 || moved_vpath.length() > 0) lv_obj_clear_state(menu_paste_btn, LV_STATE_DISABLED);
            else lv_obj_add_state(menu_paste_btn, LV_STATE_DISABLED);
        }
        if (menu_paste_label) {
            if (moved_vpath.length() > 0) lv_label_set_text(menu_paste_label, LV_SYMBOL_PASTE " Move here");
            else lv_label_set_text(menu_paste_label, LV_SYMBOL_PASTE " Paste");
        }
        if (menu_copy_cancel_btn) {
            if (copy_in_progress) lv_obj_clear_state(menu_copy_cancel_btn, LV_STATE_DISABLED);
            else lv_obj_add_state(menu_copy_cancel_btn, LV_STATE_DISABLED);
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

    void exitMovePickModeIfNeeded(bool refresh_ui = false) {
        if (!move_pick_mode) return;
        move_pick_mode = false;
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
        suspendListForDialog();
        dialog_mode = mode;

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

        const char* hint_text = "Enter a name.";
        if (mode == DIALOG_RENAME) hint_text = "Enter a new name.";

        lv_obj_t* hint_lbl = lv_label_create(dialog_box);
        lv_label_set_text(hint_lbl, hint_text);
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
        int32_t dialog_cursor_h = INPUT_TEXT_SIZE_PX;
        if (dialog_cursor_h < 4) dialog_cursor_h = 4;
        lv_obj_set_style_height(dialog_input, dialog_cursor_h, LV_PART_CURSOR);
        lv_obj_add_event_cb(dialog_input, dialog_input_event_cb, LV_EVENT_FOCUSED, this);
        lv_obj_add_event_cb(dialog_input, dialog_input_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(dialog_input, dialog_input_cursor_anchor_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(dialog_input, dialog_input_cursor_anchor_event_cb, LV_EVENT_VALUE_CHANGED, this);
        dialog_ime_cursor_anchor_pos = lv_textarea_get_cursor_pos(dialog_input);
        dialog_ime_cursor_anchor_valid = true;

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
            share_info_label = nullptr;
            share_action_btn = nullptr;
            share_action_label = nullptr;
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
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x1A1A1A), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x222222), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x2A2A2A), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x202020), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(dialog_keyboard, lv_color_hex(0x111111), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_text_color(dialog_keyboard, lv_color_hex(0xB8B8B8), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x303030), LV_PART_ITEMS | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x3A3A3A), LV_PART_ITEMS | LV_STATE_FOCUSED);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x4A4A4A), LV_PART_ITEMS | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x3A3A3A), LV_PART_ITEMS | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(dialog_keyboard, lv_color_hex(0x242424), LV_PART_ITEMS | LV_STATE_DISABLED);
        lv_obj_set_style_text_font(dialog_keyboard, FontManager::imeFont(), LV_PART_MAIN);
        lv_obj_set_style_text_font(dialog_keyboard, FontManager::imeFont(), LV_PART_ITEMS);
        lv_keyboard_set_textarea(dialog_keyboard, dialog_input);
        lv_ime_pinyin_set_keyboard(dialog_ime, dialog_keyboard);
        lv_ime_pinyin_set_dict(dialog_ime, g_pinyin_dict_plus);
        lv_ime_pinyin_set_mode(dialog_ime, LV_IME_PINYIN_MODE_K26);
        lv_obj_t* cand_panel = lv_ime_pinyin_get_cand_panel(dialog_ime);
        if (cand_panel) {
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x111111), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x1A1A1A), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x202020), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_bg_color(cand_panel, lv_color_hex(0x151515), LV_PART_ITEMS | LV_STATE_CHECKED);
            lv_obj_set_style_bg_opa(cand_panel, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(cand_panel, LV_OPA_TRANSP, LV_PART_ITEMS);
            lv_obj_set_style_border_width(cand_panel, 0, LV_PART_MAIN);
            lv_obj_set_style_border_width(cand_panel, 0, LV_PART_ITEMS);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_DEFAULT);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_FOCUSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_PRESSED);
            lv_obj_set_style_text_color(cand_panel, lv_color_hex(0xFFFFFF), LV_PART_ITEMS | LV_STATE_CHECKED);
            dialog_ime_cand_src = cand_panel;
        }

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

    void refreshShareDialog() {
        if (!dialog_box || !share_info_label) return;
        String status = on_share_ap_status_cb ? on_share_ap_status_cb() : String("AP service unavailable");
        String body = String("Hotspot Transfer\n") + status;
        lv_label_set_text(share_info_label, body.c_str());
        if (share_action_btn) lv_obj_clear_flag(share_action_btn, LV_OBJ_FLAG_HIDDEN);
        if (share_action_label) {
            bool running = on_share_ap_running_cb ? on_share_ap_running_cb() : false;
            lv_label_set_text(share_action_label, running ? "Stop AP" : "Start AP");
        }
    }

    void openShareDialog() {
        closeMenuPanel();
        closeDialog();

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
        lv_label_set_text(title, "Share");
        lv_obj_set_style_text_font(title, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

        share_info_label = lv_label_create(dialog_box);
        lv_obj_set_width(share_info_label, lv_pct(100));
        lv_label_set_long_mode(share_info_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_font(share_info_label, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(share_info_label, lv_color_hex(0xD0E6FF), 0);

        lv_obj_t* row = lv_obj_create(dialog_box);
        lv_obj_remove_style_all(row);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_height(row, 36);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, 6, LV_PART_MAIN);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* close_btn = lv_button_create(row);
        lv_obj_set_size(close_btn, 56, 28);
        styleActionButton(close_btn);
        lv_obj_add_event_cb(close_btn, dialog_cancel_event_cb, LV_EVENT_CLICKED, this);
        lv_obj_t* close_lbl = lv_label_create(close_btn);
        lv_label_set_text(close_lbl, "Close");
        lv_obj_center(close_lbl);

        share_action_btn = lv_button_create(row);
        lv_obj_set_size(share_action_btn, 72, 28);
        styleActionButton(share_action_btn);
        lv_obj_add_event_cb(share_action_btn, share_action_event_cb, LV_EVENT_CLICKED, this);
        share_action_label = lv_label_create(share_action_btn);
        lv_label_set_text(share_action_label, "Start AP");
        lv_obj_center(share_action_label);

        refreshShareDialog();
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
        lv_label_set_text(title, "File Info");
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

    void openEntryInfoDialog(const String& vpath) {
        closeMenuPanel();
        closeDialog();

        bool is_dir = false;
        uint32_t file_count = 0;
        uint64_t file_size = 0;
        if (!getEntryInfo(vpath, is_dir, file_count, file_size)) return;

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
        lv_label_set_text(title, "Info");
        lv_obj_set_style_text_font(title, FontManager::textFont(), 0);
        lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

        char info[256];
        if (is_dir) {
            lv_snprintf(
                info, sizeof(info),
                "Name: %s\nFiles: %u",
                baseName(innerPath(vpath)).c_str(),
                (unsigned)file_count
            );
        } else {
            lv_snprintf(
                info, sizeof(info),
                "Name: %s\nSize: %s",
                baseName(innerPath(vpath)).c_str(),
                formatBytesHuman(file_size).c_str()
            );
        }

        lv_obj_t* txt = lv_label_create(dialog_box);
        lv_label_set_text(txt, info);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
        lv_obj_set_style_text_font(txt, FontManager::textFont(), 0);
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
            if (new_as_dir) {
                startFsJob(FS_WORK_CREATE_DIR, final_vpath, "", true);
            } else {
                startFsJob(FS_WORK_CREATE_FILE, final_vpath, "", true);
            }
        } else if (dialog_mode == DIALOG_RENAME) {
            if (selected_vpath.length() > 0) {
                String old_v = selected_vpath;
                String src = innerPath(selected_vpath);
                String parent = parentPath(src);
                String dst = normalizeInner(parent + "/" + name);
                String dst_v = String(driveOf(selected_vpath)) + ":" + dst;
                if (dst_v != selected_vpath) dst_v = nextAvailableVPath(dst_v);
                selected_vpath = dst_v;
                startFsJob(FS_WORK_RENAME, old_v, dst_v, true);
            }
        }
        closeDialog();
    }

    void applyRemoveMarked(bool force_delete) {
        if (!file_list || delete_in_progress || copy_in_progress) return;
        uint32_t count = lv_obj_get_child_count(file_list);
        fs_worker_delete_paths.clear();
        for (uint32_t i = 0; i < count; i++) {
            lv_obj_t* row = lv_obj_get_child(file_list, i);
            EntryMeta* meta = (EntryMeta*)lv_obj_get_user_data(row);
            if (meta && meta->marked) {
                fs_worker_delete_paths.push_back(meta->vpath);
            }
        }
        if (fs_worker_delete_paths.empty()) {
            selected_vpath = "";
            remove_mode = false;
            closeMenuPanel();
            refreshUi();
            return;
        }

        if (hasAnySdPath(fs_worker_delete_paths) || !ensureFsWorkerTask()) {
            // Fallback to sync delete if worker creation fails.
            uint32_t removed = 0;
            for (size_t i = 0; i < fs_worker_delete_paths.size(); i++) {
                if (deletePath(fs_worker_delete_paths[i], force_delete)) removed++;
                delay(0);
            }
            selected_vpath = "";
            remove_mode = false;
            closeMenuPanel();
            if (removed) delay(0);
            refreshUi();
            fs_worker_delete_paths.clear();
            return;
        }

        fs_worker_delete_force = force_delete;
        fs_worker_delete_total = fs_worker_delete_paths.size();
        fs_worker_delete_done = 0;
        fs_worker_delete_removed = 0;
        fs_worker_ok = false;
        fs_worker_done = false;
        delete_in_progress = true;
        selected_vpath = "";
        remove_mode = false;
        closeMenuPanel();
        updateMenuActionStates();
        if (!delete_timer) delete_timer = lv_timer_create(delete_timer_cb, 25, this);
        fs_worker_job = FS_WORK_DELETE_BATCH;
        xTaskNotifyGive(fs_worker_task);
        refreshUi();
    }

    void pasteCopied() {
        if (copied_vpath.length() == 0 || copy_in_progress) return;
        suspendListForDialog();
        String src_name = baseName(innerPath(copied_vpath));
        size_t src_size = getFileSize(copied_vpath);
        String base_dest_v = String(active_drive) + ":" + joinPath(current_path, src_name);
        String dest_v = nextAvailableVPath(base_dest_v);
        bool involve_sd = usesSdPath(copied_vpath) || usesSdPath(dest_v);

        if (isDirectoryPath(copied_vpath)) {
            copy_total_bytes = calcDirectoryTotalBytes(copied_vpath);
            copy_done_bytes = 0;
            copy_total_files = calcDirectoryFileCount(copied_vpath);
            copy_done_files = 0;
            copy_is_dir_job = true;
            copy_cancel_requested = false;
            copy_in_progress = true;
            copy_dir_worker_mode = true;
            copy_started_ms = millis();
            fs_worker_src_vpath = copied_vpath;
            fs_worker_dst_vpath = dest_v;
            fs_worker_ok = false;
            fs_worker_done = false;
            updateMenuActionStates();
            if (menu_panel) lv_obj_remove_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
            showCopyProgressOnPaste();
            if (involve_sd || !ensureFsWorkerTask()) {
                bool ok = copyDirectoryRecursive(copied_vpath, dest_v);
                if (!ok) {
                    deletePath(dest_v, true);
                } else if (copy_total_bytes > 0) {
                    copy_done_files = copy_total_files;
                    updateCopyProgressOnPaste(copy_total_bytes, copy_total_bytes);
                }
                copy_in_progress = false;
                copy_cancel_requested = false;
                updateMenuActionStates();
                hideCopyProgressOnPaste();
                closeMenuPanel();
                list_suspended_for_dialog = false;
                refreshUi();
                return;
            }
            fs_worker_job = FS_WORK_COPY_DIR;
            xTaskNotifyGive(fs_worker_task);
            if (!copy_timer) copy_timer = lv_timer_create(copy_timer_cb, 8, this);
            if (!copy_timer) {
                copy_cancel_requested = true;
                copy_in_progress = false;
                copy_dir_worker_mode = false;
                updateMenuActionStates();
                hideCopyProgressOnPaste();
                closeMenuPanel();
                list_suspended_for_dialog = false;
                refreshUi();
            }
            return;
        }

        // Keep single-file copy on UI task (timer-driven) to avoid SD SPI task ownership asserts.
        showCopyProgressOnPaste();
        if (!beginCopyJob(copied_vpath, dest_v, src_size)) {
            hideCopyProgressOnPaste();
            closeMenuPanel();
            list_suspended_for_dialog = false;
            refreshUi();
            return;
        }
    }

    void moveSelected() {
        if (moved_vpath.length() == 0) return;
        char src_drive = driveOf(moved_vpath);
        char dst_drive = active_drive;
        if (src_drive != dst_drive) {
            Serial.println("[MOVE] cross-drive move is not supported");
            return;
        }

        String src_inner = innerPath(moved_vpath);
        String src_name = baseName(src_inner);
        String dst_base_v = String(dst_drive) + ":" + joinPath(current_path, src_name);

        String src_parent_v = String(src_drive) + ":" + parentPath(src_inner);
        if (src_parent_v == String(dst_drive) + ":" + current_path) {
            // same folder: no move
            moved_vpath = "";
            updateMenuActionStates();
            closeMenuPanel();
            return;
        }

        String dst_v = nextAvailableVPath(dst_base_v);
        if (isDirectoryPath(moved_vpath)) {
            String dst_inner = innerPath(dst_v);
            if (dst_inner.startsWith(src_inner + "/")) {
                Serial.println("[MOVE] cannot move directory into its subdirectory");
                return;
            }
        }

        bool ok = renamePath(moved_vpath, dst_v);
        if (ok) {
            if (selected_vpath == moved_vpath) selected_vpath = dst_v;
            moved_vpath = "";
            closeMenuPanel();
            refreshUi();
        } else {
            Serial.println("[MOVE] rename/move failed");
        }
        updateMenuActionStates();
    }

    bool isDirectoryPath(const String& vpath) {
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') {
            File node = LittleFS.open(p.c_str(), "r");
            if (!node) return false;
            bool is_dir = node.isDirectory();
            node.close();
            return is_dir;
        }
        if (d == 'D' && isSdFsReady()) {
            FsFile node = sdFs().open(p.c_str(), O_RDONLY);
            if (!node) return false;
            bool is_dir = node.isDir();
            node.close();
            return is_dir;
        }
        return false;
    }

    size_t calcDirectoryTotalBytes(const String& vpath) {
        if (!isDirectoryPath(vpath)) return getFileSize(vpath);

        size_t total = 0;
        char d = driveOf(vpath);
        String p = innerPath(vpath);

        if (d == 'L') {
            File dir = LittleFS.open(p.c_str(), "r");
            if (!dir || !dir.isDirectory()) return 0;
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                String name = baseName(String(entry.name()));
                bool is_dir = entry.isDirectory();
                size_t sz = (size_t)entry.size();
                entry.close();
                if (name.length() == 0) continue;
                String child_v = String('L') + ":" + joinPath(p, name);
                total += is_dir ? calcDirectoryTotalBytes(child_v) : sz;
                delay(0);
            }
            dir.close();
            return total;
        }

        if (d == 'D' && isSdFsReady()) {
            SdFs& fs = sdFs();
            FsFile dir = fs.open(p.c_str(), O_RDONLY);
            if (!dir || !dir.isDir()) return 0;
            FsFile entry;
            while (entry.openNext(&dir, O_RDONLY)) {
                char name_buf[256];
                name_buf[0] = '\0';
                entry.getName(name_buf, sizeof(name_buf));
                bool is_dir = entry.isDir();
                size_t sz = (size_t)entry.fileSize();
                entry.close();
                String name = baseName(String(name_buf));
                if (name.length() == 0) continue;
                String child_v = String('D') + ":" + joinPath(p, name);
                total += is_dir ? calcDirectoryTotalBytes(child_v) : sz;
                delay(0);
            }
            dir.close();
            return total;
        }

        return 0;
    }

    size_t calcDirectoryFileCount(const String& vpath) {
        if (!isDirectoryPath(vpath)) return 1;

        size_t total = 0;
        char d = driveOf(vpath);
        String p = innerPath(vpath);

        if (d == 'L') {
            File dir = LittleFS.open(p.c_str(), "r");
            if (!dir || !dir.isDirectory()) return 0;
            while (true) {
                File entry = dir.openNextFile();
                if (!entry) break;
                String name = baseName(String(entry.name()));
                bool is_dir = entry.isDirectory();
                entry.close();
                if (name.length() == 0) continue;
                String child_v = String('L') + ":" + joinPath(p, name);
                total += is_dir ? calcDirectoryFileCount(child_v) : 1;
                delay(0);
            }
            dir.close();
            return total;
        }

        if (d == 'D' && isSdFsReady()) {
            SdFs& fs = sdFs();
            FsFile dir = fs.open(p.c_str(), O_RDONLY);
            if (!dir || !dir.isDir()) return 0;
            FsFile entry;
            while (entry.openNext(&dir, O_RDONLY)) {
                char name_buf[256];
                name_buf[0] = '\0';
                entry.getName(name_buf, sizeof(name_buf));
                bool is_dir = entry.isDir();
                entry.close();
                String name = baseName(String(name_buf));
                if (name.length() == 0) continue;
                String child_v = String('D') + ":" + joinPath(p, name);
                total += is_dir ? calcDirectoryFileCount(child_v) : 1;
                delay(0);
            }
            dir.close();
            return total;
        }

        return 0;
    }

    bool copyDirectoryRecursive(const String& src_vpath, const String& dst_vpath) {
        if (copy_cancel_requested) return false;
        if (!makeDir(dst_vpath)) return false;

        char src_drive = driveOf(src_vpath);
        char dst_drive = driveOf(dst_vpath);
        String src_inner = innerPath(src_vpath);
        String dst_inner = innerPath(dst_vpath);

        if (src_drive == 'L') {
            File dir = LittleFS.open(src_inner.c_str(), "r");
            if (!dir || !dir.isDirectory()) return false;
            while (true) {
                delay(0);
                if (copy_cancel_requested) {
                    dir.close();
                    return false;
                }
                File entry = dir.openNextFile();
                if (!entry) break;
                String name = baseName(String(entry.name()));
                bool is_dir = entry.isDirectory();
                entry.close();
                if (name.length() == 0) continue;

                String child_src_v = String(src_drive) + ":" + joinPath(src_inner, name);
                String child_dst_v = String(dst_drive) + ":" + joinPath(dst_inner, name);
                bool ok = is_dir ? copyDirectoryRecursive(child_src_v, child_dst_v)
                                 : copyFile(child_src_v, child_dst_v, copy_total_bytes, !copy_dir_worker_mode);
                if (!ok) {
                    dir.close();
                    return false;
                }
            }
            dir.close();
            return true;
        }

        if (src_drive == 'D' && isSdFsReady()) {
            SdFs& fs = sdFs();
            FsFile dir = fs.open(src_inner.c_str(), O_RDONLY);
            if (!dir || !dir.isDir()) return false;
            FsFile entry;
            while (entry.openNext(&dir, O_RDONLY)) {
                delay(0);
                if (copy_cancel_requested) {
                    entry.close();
                    dir.close();
                    return false;
                }
                char name_buf[256];
                name_buf[0] = '\0';
                entry.getName(name_buf, sizeof(name_buf));
                bool is_dir = entry.isDir();
                entry.close();
                String name = baseName(String(name_buf));
                if (name.length() == 0) continue;

                String child_src_v = String(src_drive) + ":" + joinPath(src_inner, name);
                String child_dst_v = String(dst_drive) + ":" + joinPath(dst_inner, name);
                bool ok = is_dir ? copyDirectoryRecursive(child_src_v, child_dst_v)
                                 : copyFile(child_src_v, child_dst_v, copy_total_bytes, !copy_dir_worker_mode);
                if (!ok) {
                    dir.close();
                    return false;
                }
            }
            dir.close();
            return true;
        }

        return false;
    }

    bool beginCopyJob(const String& src_vpath, const String& dst_vpath, size_t total_bytes) {
        if (copy_in_progress) return false;
        copy_src_drive = driveOf(src_vpath);
        copy_dst_drive = driveOf(dst_vpath);
        copy_src_inner = innerPath(src_vpath);
        copy_dst_inner = innerPath(dst_vpath);
        copy_total_bytes = total_bytes;
        copy_done_bytes = 0;
        copy_total_files = 0;
        copy_done_files = 0;
        copy_is_dir_job = false;
        copy_dir_worker_mode = false;
        copy_cancel_requested = false;
        copy_started_ms = millis();

        bool src_ok = false;
        if (copy_src_drive == 'L') {
            copy_src_lfs = LittleFS.open(copy_src_inner.c_str(), "r");
            src_ok = (copy_src_lfs && !copy_src_lfs.isDirectory());
        } else if (copy_src_drive == 'D' && isSdFsReady()) {
            copy_src_sdfs = sdFs().open(copy_src_inner.c_str(), O_RDONLY);
            src_ok = (copy_src_sdfs && !copy_src_sdfs.isDir());
        }
        if (!src_ok) {
            cancelCopyJob(false);
            return false;
        }

        bool dst_ok = false;
        if (copy_dst_drive == 'L') {
            copy_dst_lfs = LittleFS.open(copy_dst_inner.c_str(), "w");
            dst_ok = (bool)copy_dst_lfs;
        } else if (copy_dst_drive == 'D' && isSdFsReady()) {
            copy_dst_sdfs = sdFs().open(copy_dst_inner.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            dst_ok = (bool)copy_dst_sdfs;
        }
        if (!dst_ok) {
            cancelCopyJob(false);
            return false;
        }

        copy_in_progress = true;
        updateMenuActionStates();
        if (menu_panel) lv_obj_remove_flag(menu_panel, LV_OBJ_FLAG_HIDDEN);
        copy_timer = lv_timer_create(copy_timer_cb, 8, this);
        if (!copy_timer) {
            cancelCopyJob(true);
            return false;
        }
        return true;
    }

    void cancelCopyJob(bool remove_partial) {
        if (copy_timer) {
            lv_timer_del(copy_timer);
            copy_timer = nullptr;
        }
        if (copy_src_lfs) copy_src_lfs.close();
        if (copy_dst_lfs) copy_dst_lfs.close();
        if (copy_src_sdfs) copy_src_sdfs.close();
        if (copy_dst_sdfs) copy_dst_sdfs.close();
        if (remove_partial && copy_dst_inner.length() > 0) {
            if (copy_dst_drive == 'L') LittleFS.remove(copy_dst_inner.c_str());
            else if (copy_dst_drive == 'D' && isSdFsReady()) {
                sdFs().remove(copy_dst_inner.c_str());
            }
        }
        copy_in_progress = false;
        copy_cancel_requested = false;
        copy_src_inner = "";
        copy_dst_inner = "";
        copy_total_bytes = 0;
        copy_done_bytes = 0;
        copy_total_files = 0;
        copy_done_files = 0;
        copy_is_dir_job = false;
        copy_dir_worker_mode = false;
        updateMenuActionStates();
    }

    void finishCopyJob(bool success) {
        if (!success && fs_worker_dst_vpath.length() > 0 &&
            (fs_worker_job == FS_WORK_COPY_DIR || fs_worker_job == FS_WORK_COPY_FILE)) {
            deletePath(fs_worker_dst_vpath, true);
        }
        cancelCopyJob(!success);
        fs_worker_src_vpath = "";
        fs_worker_dst_vpath = "";
        fs_worker_job = FS_WORK_NONE;
        fs_worker_done = false;
        fs_worker_ok = false;
        hideCopyProgressOnPaste();
        closeMenuPanel();
        list_suspended_for_dialog = false;
        refreshUi();
    }

    void finishDeleteJob() {
        delete_in_progress = false;
        if (delete_timer) {
            lv_timer_del(delete_timer);
            delete_timer = nullptr;
        }
        fs_worker_delete_paths.clear();
        fs_worker_delete_total = 0;
        fs_worker_delete_done = 0;
        fs_worker_delete_removed = 0;
        fs_worker_done = false;
        fs_worker_ok = false;
        fs_worker_job = FS_WORK_NONE;
        updateMenuActionStates();
        refreshUi();
    }

    void stepCopyJob() {
        if (!copy_in_progress) return;
        if (copy_cancel_requested) {
            if (fs_worker_job == FS_WORK_COPY_DIR || fs_worker_job == FS_WORK_COPY_FILE) {
                // Worker observes cancel flag and exits soon; poll until done.
                if (fs_worker_done) finishCopyJob(false);
                return;
            }
            finishCopyJob(false);
            return;
        }

        if (fs_worker_job == FS_WORK_COPY_DIR || fs_worker_job == FS_WORK_COPY_FILE) {
            if (copy_total_bytes > 0) updateCopyProgressOnPaste(copy_done_bytes, copy_total_bytes);
            if (fs_worker_done) {
                if (fs_worker_ok && copy_total_bytes > 0) {
                    copy_done_files = copy_total_files;
                    updateCopyProgressOnPaste(copy_total_bytes, copy_total_bytes);
                }
                finishCopyJob(fs_worker_ok);
            }
            return;
        }

        for (uint8_t i = 0; i < COPY_CHUNKS_PER_TICK; i++) {
            int n = -1;
            if (copy_src_drive == 'L') n = copy_src_lfs.read(copy_buf, COPY_IO_CHUNK);
            else if (copy_src_drive == 'D') n = copy_src_sdfs.read(copy_buf, COPY_IO_CHUNK);
            if (n < 0) {
                finishCopyJob(false);
                return;
            }
            if (n == 0) {
                updateCopyProgressOnPaste(copy_total_bytes, copy_total_bytes);
                finishCopyJob(true);
                return;
            }

            int w = -1;
            if (copy_dst_drive == 'L') w = (int)copy_dst_lfs.write(copy_buf, (size_t)n);
            else if (copy_dst_drive == 'D') w = (int)copy_dst_sdfs.write(copy_buf, (size_t)n);
            if (w != n) {
                finishCopyJob(false);
                return;
            }
            copy_done_bytes += (size_t)n;
        }

        updateCopyProgressOnPaste(copy_done_bytes, copy_total_bytes);
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
        char d = driveOf(vpath);
        if (vpath.length() >= 2 && vpath.charAt(1) == ':') {
            String p = vpath.substring(2);
            if (!p.startsWith("/")) p = "/" + p;
            if (d == 'L') {
                if (p == "/littlefs") return "/";
                if (p.startsWith("/littlefs/")) p = p.substring(9);
            } else if (d == 'D') {
                if (p == "/sd") return "/";
                if (p.startsWith("/sd/")) p = p.substring(3);
            }
            return p;
        }
        String p = vpath;
        if (!p.startsWith("/")) p = "/" + p;
        if (p == "/littlefs") return "/";
        if (p.startsWith("/littlefs/")) p = p.substring(9);
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

    bool isSdFsReady() const {
        return sd_ready && StorageHelper::getInstance()->isInitialized();
    }

    SdFs& sdFs() {
        return StorageHelper::getInstance()->getFs();
    }

    bool pathExists(const String& vpath) {
        char d = driveOf(vpath);
        String p = innerPath(vpath);
        if (d == 'L') return LittleFS.exists(p.c_str());
        if (d == 'D' && isSdFsReady()) return sdFs().exists(p.c_str());
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
        if (d == 'D' && isSdFsReady()) return StorageHelper::getInstance()->writeFile(p.c_str(), data);
        return false;
    }

    bool makeDir(const String& vpath) {
        char d = driveOf(vpath);
        String p = normalizeInner(innerPath(vpath));
        if (p == "/") return false;
        if (d == 'L') return LittleFS.mkdir(p.c_str());
        if (d == 'D' && isSdFsReady()) return sdFs().mkdir(p.c_str(), true);
        return false;
    }

    bool deletePath(const String& vpath, bool force_delete) {
        char d = driveOf(vpath);
        String p = normalizeInner(innerPath(vpath));
        if (p == "/") return false;
        if (!force_delete) {
            if (d == 'L') return deleteLittleFsPathSimple(p);
            if (d == 'D' && isSdFsReady()) return deleteSdPathSimple(p);
            return false;
        }
        if (d == 'L') return deleteLittleFsPathRecursive(p);
        if (d == 'D' && isSdFsReady()) return deleteSdPathRecursive(p);
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
            String name = baseName(String(entry.name()));
            entry.close();

            if (name.length() == 0 || name == "." || name == "..") {
                ok = false;
                continue;
            }
            String child = joinPath(path, name);
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
        if (d != driveOf(to_vpath)) return false;
        String from = innerPath(from_vpath);
        String to = innerPath(to_vpath);
        if (d == 'L') return LittleFS.rename(from.c_str(), to.c_str());
        if (d == 'D' && isSdFsReady()) return sdFs().rename(from.c_str(), to.c_str());
        return false;
    }

    String formatEta(uint32_t seconds) const {
        if (seconds >= 3600) {
            uint32_t h = seconds / 3600;
            uint32_t m = (seconds % 3600) / 60;
            return String(h) + "h" + String(m) + "m";
        }
        if (seconds >= 60) {
            uint32_t m = seconds / 60;
            uint32_t s = seconds % 60;
            return String(m) + "m" + String(s) + "s";
        }
        return String(seconds) + "s";
    }

    void showCopyProgressOnPaste() {
        if (menu_paste_progress_track) {
            lv_obj_clear_flag(menu_paste_progress_track, LV_OBJ_FLAG_HIDDEN);
        }
        if (menu_paste_progress_bg) {
            lv_obj_set_size(menu_paste_progress_bg, 0, 18);
        }
        if (menu_copy_cancel_btn) lv_obj_clear_flag(menu_copy_cancel_btn, LV_OBJ_FLAG_HIDDEN);
        if (menu_paste_label) lv_label_set_text(menu_paste_label, LV_SYMBOL_PASTE " Paste 0%");
        if (menu_copy_cancel_label) lv_label_set_text(menu_copy_cancel_label, LV_SYMBOL_CLOSE " Cancel");
        lv_refr_now(NULL);
    }

    void hideCopyProgressOnPaste() {
        if (menu_paste_progress_track) {
            lv_obj_add_flag(menu_paste_progress_track, LV_OBJ_FLAG_HIDDEN);
        }
        if (menu_paste_progress_bg) {
            lv_obj_set_size(menu_paste_progress_bg, 0, 18);
        }
        if (menu_paste_label) lv_label_set_text(menu_paste_label, LV_SYMBOL_PASTE " Paste");
        if (menu_copy_cancel_btn) lv_obj_add_flag(menu_copy_cancel_btn, LV_OBJ_FLAG_HIDDEN);
        if (menu_copy_cancel_label) lv_label_set_text(menu_copy_cancel_label, LV_SYMBOL_CLOSE " Cancel");
        lv_refr_now(NULL);
    }

    void updateCopyProgressOnPaste(size_t done, size_t total) {
        if (!menu_paste_progress_track || !menu_paste_progress_bg || !menu_paste_label || total == 0) return;

        int pct = (int)((done * 100) / total);
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;

        int32_t full_w = lv_obj_get_width(menu_paste_progress_track);
        if (full_w < 0) full_w = 0;
        int32_t fill_w = (int32_t)((full_w * pct) / 100);
        if (fill_w < 0) fill_w = 0;
        if (fill_w > full_w) fill_w = full_w;
        lv_obj_set_size(menu_paste_progress_bg, fill_w, 18);

        uint32_t elapsed_ms = millis() - copy_started_ms;
        if (done > 0 && elapsed_ms >= 250) {
            uint32_t eta_s = (uint32_t)(((uint64_t)(total - done) * elapsed_ms) / done / 1000ULL);
            String txt;
            if (copy_is_dir_job && copy_total_files > 0) {
                txt = String(LV_SYMBOL_PASTE) + " " +
                      String((unsigned long)copy_done_files) + "/" + String((unsigned long)copy_total_files) +
                      " " + String(pct) + "% " + formatEta(eta_s);
            } else {
                txt = String(LV_SYMBOL_PASTE) + " " + String(pct) + "% " + formatEta(eta_s);
            }
            lv_label_set_text(menu_paste_label, txt.c_str());
        } else {
            if (copy_is_dir_job && copy_total_files > 0) {
                String txt = String(LV_SYMBOL_PASTE) + " " +
                             String((unsigned long)copy_done_files) + "/" + String((unsigned long)copy_total_files) +
                             " " + String(pct) + "%";
                lv_label_set_text(menu_paste_label, txt.c_str());
            } else {
                char paste_txt[40];
                lv_snprintf(paste_txt, sizeof(paste_txt), LV_SYMBOL_PASTE " %d%%", pct);
                lv_label_set_text(menu_paste_label, paste_txt);
            }
        }

        if (menu_copy_cancel_label) {
            lv_label_set_text(menu_copy_cancel_label, LV_SYMBOL_CLOSE " Cancel");
        }
    }

    bool copyFile(const String& src_vpath, const String& dst_vpath, size_t total_bytes = 0, bool show_progress = false) {
        char sd = driveOf(src_vpath);
        char dd = driveOf(dst_vpath);
        String sp = innerPath(src_vpath);
        String dp = innerPath(dst_vpath);
        static constexpr size_t CHUNK = 1024;
        uint8_t buf[CHUNK];
        size_t copied = 0;
        uint32_t chunks = 0;

        auto transferLoop = [&](const std::function<int(uint8_t*, size_t)>& read_fn,
                                const std::function<int(const uint8_t*, size_t)>& write_fn,
                                const std::function<void()>& close_fn,
                                const std::function<void()>& remove_partial_fn) -> bool {
            while (true) {
                delay(0);
                if (copy_cancel_requested) {
                    close_fn();
                    remove_partial_fn();
                    return false;
                }
                int n = read_fn(buf, CHUNK);
                if (n <= 0) break;
                if (write_fn(buf, (size_t)n) != n) {
                    close_fn();
                    return false;
                }
                copied += (size_t)n;
                copy_done_bytes += (size_t)n;
                if (show_progress && ((++chunks & 0x03) == 0)) {
                    updateCopyProgressOnPaste(copy_done_bytes, total_bytes);
                    lv_refr_now(NULL);
                }
                delay(0);
            }
            close_fn();
            if (copy_is_dir_job) copy_done_files++;
            if (show_progress) updateCopyProgressOnPaste(copy_done_bytes, total_bytes);
            return true;
        };

        if (sd == 'L' && dd == 'L') {
            File in = LittleFS.open(sp.c_str(), "r");
            if (!in || in.isDirectory()) return false;
            File out = LittleFS.open(dp.c_str(), "w");
            if (!out) { in.close(); return false; }
            return transferLoop(
                [&](uint8_t* b, size_t s) -> int { return in.read(b, s); },
                [&](const uint8_t* b, size_t s) -> int { return (int)out.write(b, s); },
                [&]() { in.close(); out.close(); },
                [&]() { LittleFS.remove(dp.c_str()); }
            );
        }
        if (sd == 'D' && dd == 'D' && isSdFsReady()) {
            SdFs& fs = sdFs();
            FsFile in = fs.open(sp.c_str(), O_RDONLY);
            if (!in || in.isDir()) return false;
            FsFile out = fs.open(dp.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            if (!out) { in.close(); return false; }
            return transferLoop(
                [&](uint8_t* b, size_t s) -> int { return in.read(b, s); },
                [&](const uint8_t* b, size_t s) -> int { return (int)out.write(b, s); },
                [&]() { in.close(); out.close(); },
                [&]() { fs.remove(dp.c_str()); }
            );
        }
        if (sd == 'L' && dd == 'D' && isSdFsReady()) {
            File in = LittleFS.open(sp.c_str(), "r");
            if (!in || in.isDirectory()) return false;
            SdFs& fs = sdFs();
            FsFile out = fs.open(dp.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
            if (!out) { in.close(); return false; }
            return transferLoop(
                [&](uint8_t* b, size_t s) -> int { return in.read(b, s); },
                [&](const uint8_t* b, size_t s) -> int { return (int)out.write(b, s); },
                [&]() { in.close(); out.close(); },
                [&]() { fs.remove(dp.c_str()); }
            );
        }
        if (sd == 'D' && dd == 'L' && isSdFsReady()) {
            SdFs& fs = sdFs();
            FsFile in = fs.open(sp.c_str(), O_RDONLY);
            if (!in || in.isDir()) return false;
            File out = LittleFS.open(dp.c_str(), "w");
            if (!out) { in.close(); return false; }
            return transferLoop(
                [&](uint8_t* b, size_t s) -> int { return in.read(b, s); },
                [&](const uint8_t* b, size_t s) -> int { return (int)out.write(b, s); },
                [&]() { in.close(); out.close(); },
                [&]() { LittleFS.remove(dp.c_str()); }
            );
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
        if (d == 'D' && isSdFsReady()) return StorageHelper::getInstance()->readFile(p.c_str(), out);
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
        if (d == 'D' && isSdFsReady()) {
            FsFile f = sdFs().open(p.c_str(), O_RDONLY);
            if (!f) return 0;
            size_t sz = (size_t)f.fileSize();
            f.close();
            return sz;
        }
        return 0;
    }

    void stepDeleteJob() {
        if (!delete_in_progress) return;
        if (!fs_worker_done) return;
        finishDeleteJob();
    }

    void finishFsJob() {
        fs_job_in_progress = false;
        if (fs_job_timer) {
            lv_timer_del(fs_job_timer);
            fs_job_timer = nullptr;
        }
        fs_worker_job = FS_WORK_NONE;
        fs_worker_arg1 = "";
        fs_worker_arg2 = "";
        fs_worker_done = false;
        fs_worker_ok = false;
        list_suspended_for_dialog = false;
        updateMenuActionStates();
        refreshUi();
    }

    void stepFsJob() {
        if (!fs_job_in_progress) return;
        if (!fs_worker_done) return;
        if (fs_worker_job == FS_WORK_SCAN_DIR) {
            scan_in_progress = false;
            scan_result_ok = fs_worker_ok;
            scan_result_ready = true;
        }
        finishFsJob();
    }

    bool ensureFsWorkerTask() {
        if (fs_worker_task) return true;
        BaseType_t rc = xTaskCreatePinnedToCore(
            fs_worker_task_entry,
            "fm_fs_worker",
            10240,
            this,
            1,
            &fs_worker_task,
            1
        );
        return rc == pdPASS;
    }

    bool startFsJob(FsWorkerJobType job, const String& a1, const String& a2, bool refresh_after) {
        if (fs_job_in_progress || copy_in_progress || delete_in_progress) return false;
        bool touch_sd = false;
        if (job == FS_WORK_CREATE_FILE || job == FS_WORK_CREATE_DIR) touch_sd = usesSdPath(a1);
        else if (job == FS_WORK_RENAME) touch_sd = usesSdPath(a1) || usesSdPath(a2);
        if (touch_sd) {
            bool ok = false;
            if (job == FS_WORK_CREATE_FILE) ok = writeTextFile(a1, "");
            else if (job == FS_WORK_CREATE_DIR) ok = makeDir(a1);
            else if (job == FS_WORK_RENAME) ok = renamePath(a1, a2);
            if (refresh_after) refreshUi();
            return ok;
        }
        if (!ensureFsWorkerTask()) return false;
        LV_UNUSED(refresh_after);
        fs_worker_arg1 = a1;
        fs_worker_arg2 = a2;
        fs_worker_ok = false;
        fs_worker_done = false;
        fs_worker_job = job;
        fs_job_in_progress = true;
        updateMenuActionStates();
        if (!fs_job_timer) fs_job_timer = lv_timer_create(fs_job_timer_cb, 20, this);
        xTaskNotifyGive(fs_worker_task);
        return true;
    }

    bool startScanJob(const String& vpath) {
        if (scan_in_progress || fs_job_in_progress || copy_in_progress || delete_in_progress) return false;
        if (!ensureFsWorkerTask()) return false;
        fs_worker_scan_items.clear();
        fs_worker_scan_vpath = vpath;
        fs_worker_ok = false;
        fs_worker_done = false;
        fs_worker_job = FS_WORK_SCAN_DIR;
        scan_in_progress = true;
        fs_job_in_progress = true;
        if (!fs_job_timer) fs_job_timer = lv_timer_create(fs_job_timer_cb, 20, this);
        xTaskNotifyGive(fs_worker_task);
        return true;
    }

    bool getEntryInfo(const String& vpath, bool& is_dir, uint32_t& file_count, uint64_t& file_size) {
        is_dir = false;
        file_count = 0;
        file_size = 0;
        char d = driveOf(vpath);
        String p = innerPath(vpath);

        if (d == 'L') {
            File node = LittleFS.open(p.c_str(), "r");
            if (!node) return false;
            is_dir = node.isDirectory();
            if (!is_dir) {
                file_size = (uint64_t)node.size();
                node.close();
                return true;
            }
            uint32_t iter = 0;
            while (true) {
                File e = node.openNextFile();
                if (!e) break;
                if (!e.isDirectory()) file_count++;
                e.close();
                if ((++iter & 0x0F) == 0) delay(0);
            }
            node.close();
            return true;
        }

        if (d == 'D' && sd_ready && StorageHelper::getInstance()->isInitialized()) {
            FsFile node = StorageHelper::getInstance()->getFs().open(p.c_str(), O_RDONLY);
            if (!node) return false;
            is_dir = node.isDir();
            if (!is_dir) {
                file_size = (uint64_t)node.fileSize();
                node.close();
                return true;
            }
            uint32_t iter = 0;
            FsFile e;
            while (e.openNext(&node, O_RDONLY)) {
                if (!e.isDir()) file_count++;
                e.close();
                if ((++iter & 0x0F) == 0) delay(0);
            }
            node.close();
            return true;
        }
        return false;
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
