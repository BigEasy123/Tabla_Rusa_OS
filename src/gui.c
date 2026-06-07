#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "mouse.h"
#include "keyboard.h"
#include "jobs.h"
#include "lang.h"
#include "mathlib.h"
#include "memory.h"
#include "net.h"
#include "policy.h"
#include "privacy.h"
#include "process.h"
#include "security.h"
#include "service.h"
#include "shell.h"
#include "timer.h"
#include "window.h"
#include "gui.h"

static int running = 0;
static int autostarted = 0;
static int desktop_mode = 0;
static int terminal_requested = 0;
static int launcher_open = 0;
static char active_app[16] = "desktop";
static char saver_hint[16] = "lava";
static char screensaver_hint[16] = "lava";
static char launch_notice[48] = "";
static char launch_override[128] = "";
static uint32_t open_apps = 0;
static int saver_backdrop = 1;
static int saver_live = 0;
static int screensaver_calm = 1;
static uint32_t saver_last_tick = 0;
static uint32_t gui_busy_until = 0;
static int editor_mode = 0; /* 0=paper, 1=code, 2=math notes */
static int rusa_tab = 0;
static int editor_focused = 0;
static int editor_dirty = 0;
static uint32_t editor_line = 0;
static uint32_t editor_col = 0;
static uint32_t editor_top = 0;
static uint32_t editor_sel_start = 0;
static uint32_t editor_sel_end = 0;
static int editor_selection = 0;
static int editor_dialog_mode = 0; /* 0=none, 1=open, 2=save-as */
static char editor_current_path[96] = "/home/notes.txt";
static char editor_dialog_dir[96] = "/home";
static char editor_dialog_selected[96] = "/home/readme.txt";
static char file_dir[96] = "/home";
static char file_selected[96] = "/home/readme.txt";
static uint32_t file_last_click_index = 9999;
static uint32_t file_last_click_tick = 0;
static char terminal_view[160] = "Terminal ready. Open an app command or press Enter.";
static char rusa_view[160] = "Rusa Workbench ready. Choose Check or Run.";
static char rusa_lines[5][96] = {
    "Rusa Workbench ready.",
    "Open a .rusa file, then Check or Run.",
    "Diagnostics stay readable and editor-linked.",
    "",
    ""
};
static int math_tab = 0;
static int settings_tab = 0;
static int security_tab = 0;
static uint32_t settings_mouse_speed = 2; /* 1=slow, 2=normal, 3=fast */
static char taskman_selected[16] = "compute";
static char math_lines[5][96] = {
    "Vector lab: dot([1,2,3],[4,5,6]) = 32",
    "Use tabs for matrix, group, physics, LaTeX, and jobs.",
    "Open Terminal exposes the full math command set.",
    "",
    ""
};
static char editor_clipboard[512] = "";
static char editor_find_text[40] = "";
static int editor_find_line = -1;
static char terminal_input[96] = "";
static uint32_t terminal_cursor = 0;
static char terminal_lines[24][96];
static uint32_t terminal_count = 0;
static uint32_t terminal_top = 0;
static int terminal_focused = 0;
static char terminal_history[8][96];
static uint32_t terminal_history_count = 0;
static int terminal_history_view = -1;
static char terminal_clipboard[256] = "";
static uint32_t terminal_sel_start = 0;
static uint32_t terminal_sel_end = 0;
static int terminal_selection = 0;
static uint32_t gui_win_x = 174;
static uint32_t gui_win_y = 72;
static uint32_t gui_win_w = 820;
static uint32_t gui_win_h = 594;
static uint32_t gui_restore_x = 174;
static uint32_t gui_restore_y = 72;
static uint32_t gui_restore_w = 820;
static uint32_t gui_restore_h = 594;
static int gui_window_maximized = 0;
static int gui_drag_mode = 0; /* 1=move, 2=resize */
static uint32_t gui_drag_dx = 0;
static uint32_t gui_drag_dy = 0;
static uint32_t minimized_apps = 0;
static uint32_t window_z_order[16];
static uint32_t window_z_count = 0;
static uint32_t gui_full_repaints = 0;
static uint32_t gui_window_repaints = 0;
static uint32_t gui_inactive_live_renders = 0;

#define APP_FILES    (1U << 0)
#define APP_TERM     (1U << 1)
#define APP_MATH     (1U << 2)
#define APP_RUSA     (1U << 3)
#define APP_SETTINGS (1U << 4)
#define APP_TASKS    (1U << 5)
#define APP_NET      (1U << 6)
#define APP_SAVER    (1U << 7)
#define APP_EDITOR   (1U << 8)
#define APP_PROJECTS (1U << 9)
#define APP_PACKAGES (1U << 10)
#define APP_LOGS     (1U << 11)
#define APP_SECURITY (1U << 12)
#define APP_EVENTS   (1U << 13)
#define APP_STORAGE  (1U << 14)
#define APP_INSPECTOR (1U << 15)

struct gui_app_window {
    const char* app;
    uint32_t mask;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t restore_x;
    uint32_t restore_y;
    uint32_t restore_w;
    uint32_t restore_h;
    int maximized;
};

static struct gui_app_window app_windows[] = {
    {"files", APP_FILES, 162, 76, 660, 456, 162, 76, 660, 456, 0},
    {"terminal", APP_TERM, 202, 118, 680, 430, 202, 118, 680, 430, 0},
    {"math", APP_MATH, 220, 94, 700, 470, 220, 94, 700, 470, 0},
    {"rusa", APP_RUSA, 238, 112, 700, 470, 238, 112, 700, 470, 0},
    {"privacy", APP_SETTINGS, 184, 92, 720, 490, 184, 92, 720, 490, 0},
    {"taskman", APP_TASKS, 260, 132, 650, 430, 260, 132, 650, 430, 0},
    {"network", APP_NET, 284, 108, 650, 430, 284, 108, 650, 430, 0},
    {"saver", APP_SAVER, 202, 100, 700, 470, 202, 100, 700, 470, 0},
    {"editor", APP_EDITOR, 174, 72, 820, 594, 174, 72, 820, 594, 0},
    {"projects", APP_PROJECTS, 232, 128, 650, 430, 232, 128, 650, 430, 0},
    {"packages", APP_PACKAGES, 248, 144, 650, 430, 248, 144, 650, 430, 0},
    {"logs", APP_LOGS, 264, 160, 650, 430, 264, 160, 650, 430, 0},
    {"security", APP_SECURITY, 280, 176, 650, 430, 280, 176, 650, 430, 0},
    {"events", APP_EVENTS, 296, 192, 650, 430, 296, 192, 650, 430, 0},
    {"storage", APP_STORAGE, 312, 208, 650, 430, 312, 208, 650, 430, 0},
    {"inspector", APP_INSPECTOR, 328, 224, 650, 430, 328, 224, 650, 430, 0}
};

struct gui_launcher_entry {
    const char* app;
    const char* label;
    const char* hint;
};

static const struct gui_launcher_entry launcher_entries[] = {
    {"files", "Files", "browse /home"},
    {"terminal", "Terminal", "type shell commands"},
    {"editor", "Editor", "draft or code"},
    {"rusa", "Rusa", "language workbench"},
    {"math", "Math Lab", "science tools"},
    {"privacy", "Settings", "hardware privacy display"},
    {"taskman", "Tasks", "processes and jobs"},
    {"network", "Network", "ports and link state"},
    {"saver", "Screensavers", "wallpaper and calm modes"},
    {"security", "Security", "audit and permissions"}
};

#define GUI_EDITOR_VISIBLE_LINES 9
#define GUI_EDITOR_MAX_LINES 64
#define GUI_EDITOR_COLS 72
#define GUI_FONT_ADVANCE 6
#define GUI_WALLPAPER_TICKS 90
#define GUI_INPUT_QUIET_TICKS 30
#define GUI_DESIGN_X 174
#define GUI_DESIGN_Y 72

static char editor_buf[GUI_EDITOR_MAX_LINES][GUI_EDITOR_COLS];

void gui_enter_desktop(void);
static const char* editor_path(void);
static void editor_clamp_cursor(void);
static int app_is_minimized(const char* app);
static uint32_t text_len32(const char* s);
static void gui_focus_app(const char* app);
static void request_terminal_command(const char* command, const char* notice);
static void draw_active_app_detail(void);
static void gui_open_app(const char* app);
static void gui_redraw_active_window(void);
static void gui_window_list(void);
static void gui_focus_next_open_app(void);
static void file_parent_path(const char* path, char* out, uint32_t max);

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

static int str_eq(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int text_has(const char* text, const char* pattern){
    if(!pattern || !pattern[0]) return 1;
    for(uint32_t i=0; text && text[i]; i++){
        uint32_t j = 0;
        while(text[i + j] && pattern[j] && lower_char(text[i + j]) == lower_char(pattern[j]))
            j++;
        if(!pattern[j])
            return 1;
    }
    return 0;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char* first_arg(char* arg, char** rest){
    while(is_space(*arg)) arg++;
    char* start = arg;
    while(*arg && !is_space(*arg)) arg++;
    if(*arg){
        *arg = 0;
        arg++;
    }
    while(is_space(*arg)) arg++;
    *rest = arg;
    return start;
}

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static int active_is(const char* name){
    return str_eq(active_app, name);
}

static uint32_t app_mask(const char* app){
    if(str_eq(app, "desktop")) return 0;
    if(str_eq(app, "files")) return APP_FILES;
    if(str_eq(app, "terminal")) return APP_TERM;
    if(str_eq(app, "math")) return APP_MATH;
    if(str_eq(app, "rusa")) return APP_RUSA;
    if(str_eq(app, "privacy") || str_eq(app, "settings")) return APP_SETTINGS;
    if(str_eq(app, "taskman") || str_eq(app, "tasks")) return APP_TASKS;
    if(str_eq(app, "network") || str_eq(app, "net")) return APP_NET;
    if(str_eq(app, "saver")) return APP_SAVER;
    if(str_eq(app, "editor")) return APP_EDITOR;
    if(str_eq(app, "projects") || str_eq(app, "project")) return APP_PROJECTS;
    if(str_eq(app, "packages") || str_eq(app, "package") || str_eq(app, "pkg")) return APP_PACKAGES;
    if(str_eq(app, "logs") || str_eq(app, "log")) return APP_LOGS;
    if(str_eq(app, "security") || str_eq(app, "sec")) return APP_SECURITY;
    if(str_eq(app, "events") || str_eq(app, "event")) return APP_EVENTS;
    if(str_eq(app, "storage") || str_eq(app, "disk") || str_eq(app, "block")) return APP_STORAGE;
    if(str_eq(app, "inspector") || str_eq(app, "inspect")) return APP_INSPECTOR;
    return 0;
}

static const char* canonical_app(const char* app){
    if(str_eq(app, "tasks")) return "taskman";
    if(str_eq(app, "net")) return "network";
    if(str_eq(app, "settings")) return "privacy";
    if(str_eq(app, "term")) return "terminal";
    if(str_eq(app, "project")) return "projects";
    if(str_eq(app, "pkg") || str_eq(app, "package")) return "packages";
    if(str_eq(app, "log")) return "logs";
    if(str_eq(app, "sec")) return "security";
    if(str_eq(app, "event")) return "events";
    if(str_eq(app, "disk") || str_eq(app, "block")) return "storage";
    if(str_eq(app, "inspect")) return "inspector";
    return app;
}

static const char* app_title_for(const char* app){
    if(str_eq(app, "desktop")) return "Desktop";
    if(str_eq(app, "files")) return "Files";
    if(str_eq(app, "taskman")) return "Task Manager";
    if(str_eq(app, "math")) return "Math Lab";
    if(str_eq(app, "rusa")) return "Rusa Workbench";
    if(str_eq(app, "privacy")) return "Settings";
    if(str_eq(app, "network")) return "Network";
    if(str_eq(app, "saver")) return "Screensavers";
    if(str_eq(app, "inspector")) return "Inspector";
    if(str_eq(app, "editor")) return "Tabla Editor";
    if(str_eq(app, "projects")) return "Projects";
    if(str_eq(app, "packages")) return "Packages";
    if(str_eq(app, "logs")) return "Logs";
    if(str_eq(app, "security")) return "Security";
    if(str_eq(app, "events")) return "Events";
    if(str_eq(app, "storage")) return "Storage";
    return "Terminal";
}

static struct gui_app_window* app_window_for(const char* app){
    app = canonical_app(app);
    for(uint32_t i=0; i<sizeof(app_windows)/sizeof(app_windows[0]); i++)
        if(str_eq(app_windows[i].app, app))
            return &app_windows[i];
    return 0;
}

static uint32_t app_window_count(void){
    return (uint32_t)(sizeof(app_windows) / sizeof(app_windows[0]));
}

static int app_window_index(const char* app){
    app = canonical_app(app);
    for(uint32_t i=0; i<app_window_count(); i++)
        if(str_eq(app_windows[i].app, app))
            return (int)i;
    return -1;
}

static void z_remove_index(uint32_t index){
    for(uint32_t i=0; i<window_z_count; i++){
        if(window_z_order[i] == index){
            for(uint32_t j=i + 1; j<window_z_count; j++)
                window_z_order[j - 1] = window_z_order[j];
            window_z_count--;
            return;
        }
    }
}

static void z_bring_to_front(const char* app){
    int index = app_window_index(app);
    if(index < 0)
        return;
    z_remove_index((uint32_t)index);
    if(window_z_count < app_window_count())
        window_z_order[window_z_count++] = (uint32_t)index;
}

static void z_remove_app(const char* app){
    int index = app_window_index(app);
    if(index >= 0)
        z_remove_index((uint32_t)index);
}

static const char* z_top_visible_app(void){
    for(int i=(int)window_z_count - 1; i>=0; i--){
        struct gui_app_window* win = &app_windows[window_z_order[i]];
        if((open_apps & win->mask) && !app_is_minimized(win->app))
            return win->app;
    }
    for(uint32_t i=0; i<app_window_count(); i++)
        if((open_apps & app_windows[i].mask) && !app_is_minimized(app_windows[i].app))
            return app_windows[i].app;
    return "desktop";
}

static void load_active_window_geometry(void){
    struct gui_app_window* win = app_window_for(active_app);
    if(!win) return;
    gui_win_x = win->x;
    gui_win_y = win->y;
    gui_win_w = win->w;
    gui_win_h = win->h;
    gui_restore_x = win->restore_x;
    gui_restore_y = win->restore_y;
    gui_restore_w = win->restore_w;
    gui_restore_h = win->restore_h;
    gui_window_maximized = win->maximized;
}

static void save_active_window_geometry(void){
    struct gui_app_window* win = app_window_for(active_app);
    if(!win) return;
    win->x = gui_win_x;
    win->y = gui_win_y;
    win->w = gui_win_w;
    win->h = gui_win_h;
    win->restore_x = gui_restore_x;
    win->restore_y = gui_restore_y;
    win->restore_w = gui_restore_w;
    win->restore_h = gui_restore_h;
    win->maximized = gui_window_maximized;
}

static int app_is_open(const char* app){
    uint32_t mask = app_mask(app);
    return mask != 0 && (open_apps & mask) != 0;
}

static int app_is_minimized(const char* app){
    uint32_t mask = app_mask(app);
    return mask != 0 && (minimized_apps & mask) != 0;
}

static int app_is_visible(const char* app){
    return app_is_open(app) && !app_is_minimized(app);
}

static void app_open(const char* app){
    uint32_t mask = app_mask(app);
    if(mask == 0)
        return;
    open_apps |= mask;
    minimized_apps &= ~mask;
    z_bring_to_front(app);
}

static void app_focus_next_or_desktop(void){
    copy_text(active_app, z_top_visible_app(), sizeof(active_app));
    load_active_window_geometry();
}

static void app_close(const char* app){
    uint32_t mask = app_mask(app);
    if(mask == 0){
        copy_text(active_app, "desktop", sizeof(active_app));
        editor_focused = 0;
        terminal_focused = 0;
        return;
    }
    z_remove_app(app);
    open_apps &= ~mask;
    minimized_apps &= ~mask;
    if(mask == APP_EDITOR)
        editor_focused = 0;
    if(mask == APP_TERM)
        terminal_focused = 0;
    if(app_mask(active_app) == mask)
        app_focus_next_or_desktop();
}

static void app_minimize(const char* app){
    uint32_t mask = app_mask(app);
    if(mask == 0 || !(open_apps & mask))
        return;
    minimized_apps |= mask;
    if(mask == APP_EDITOR)
        editor_focused = 0;
    if(mask == APP_TERM)
        terminal_focused = 0;
    if(app_mask(active_app) == mask)
        app_focus_next_or_desktop();
}

static void app_restore(const char* app){
    uint32_t mask = app_mask(app);
    if(mask == 0)
        return;
    open_apps |= mask;
    minimized_apps &= ~mask;
    gui_focus_app(app);
}

static void gui_focus_next_open_app(void){
    int active_index = app_window_index(active_app);
    uint32_t count = app_window_count();
    if(open_apps == 0){
        copy_text(active_app, "desktop", sizeof(active_app));
        load_active_window_geometry();
        return;
    }
    for(uint32_t step=1; step<=count; step++){
        uint32_t index = active_index >= 0 ? ((uint32_t)active_index + step) % count : step - 1;
        struct gui_app_window* win = &app_windows[index];
        if((open_apps & win->mask) && !app_is_minimized(win->app)){
            gui_focus_app(win->app);
            copy_text(launch_notice, "window cycled", sizeof(launch_notice));
            return;
        }
    }
    app_focus_next_or_desktop();
}

static void window_toggle_maximize(void){
    if(!gui_window_maximized){
        gui_restore_x = gui_win_x;
        gui_restore_y = gui_win_y;
        gui_restore_w = gui_win_w;
        gui_restore_h = gui_win_h;
        gui_win_x = 126;
        gui_win_y = 46;
        gui_win_w = 772;
        gui_win_h = 662;
        gui_window_maximized = 1;
        copy_text(launch_notice, "window maximized", sizeof(launch_notice));
    } else {
        gui_win_x = gui_restore_x;
        gui_win_y = gui_restore_y;
        gui_win_w = gui_restore_w;
        gui_win_h = gui_restore_h;
        gui_window_maximized = 0;
        copy_text(launch_notice, "window restored", sizeof(launch_notice));
    }
    save_active_window_geometry();
}

static void clamp_window_geometry(void){
    if(gui_win_w < 520) gui_win_w = 520;
    if(gui_win_h < 380) gui_win_h = 380;
    if(gui_win_w > 900) gui_win_w = 900;
    if(gui_win_h > 662) gui_win_h = 662;
    if(gui_win_x < 112) gui_win_x = 112;
    if(gui_win_y < 38) gui_win_y = 38;
    if(gui_win_x + gui_win_w > 1008) gui_win_x = 1008 - gui_win_w;
    if(gui_win_y + gui_win_h > 714) gui_win_y = 714 - gui_win_h;
    save_active_window_geometry();
}

static void u32_text(uint32_t value, char* out, uint32_t max){
    char tmp[12];
    uint32_t i = 0;
    uint32_t n = value;
    if(max == 0) return;
    if(n == 0){
        if(max > 1){
            out[0] = '0';
            out[1] = 0;
        } else out[0] = 0;
        return;
    }
    while(n && i < sizeof(tmp)){
        tmp[i++] = (char)('0' + (n % 10));
        n /= 10;
    }
    uint32_t j = 0;
    while(i && j + 1 < max)
        out[j++] = tmp[--i];
    out[j] = 0;
}

static void append_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    uint32_t j = 0;
    if(max == 0) return;
    while(dst[i] && i + 1 < max) i++;
    while(src && src[j] && i + 1 < max)
        dst[i++] = src[j++];
    dst[i] = 0;
}

static uint32_t app_x(uint32_t design_x){
    return gui_win_x + (design_x > GUI_DESIGN_X ? design_x - GUI_DESIGN_X : 0);
}

static uint32_t app_y(uint32_t design_y){
    return gui_win_y + (design_y > GUI_DESIGN_Y ? design_y - GUI_DESIGN_Y : 0);
}

static uint32_t design_x_from_screen(uint32_t x){
    return x >= gui_win_x ? GUI_DESIGN_X + (x - gui_win_x) : 0;
}

static uint32_t design_y_from_screen(uint32_t y){
    return y >= gui_win_y ? GUI_DESIGN_Y + (y - gui_win_y) : 0;
}

static void app_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color){
    fb_fill_rect(app_x(x), app_y(y), w, h, color);
}

static void app_draw_text(uint32_t x, uint32_t y, const char* text, uint32_t color){
    fb_draw_text(app_x(x), app_y(y), text, color);
}

static void gui_note_activity(void){
    gui_busy_until = timer_ticks() + GUI_INPUT_QUIET_TICKS;
}

static void terminal_add_line(const char* text){
    if(terminal_count < 24){
        copy_text(terminal_lines[terminal_count++], text, sizeof(terminal_lines[0]));
    } else {
        for(uint32_t i=1; i<24; i++)
            copy_text(terminal_lines[i - 1], terminal_lines[i], sizeof(terminal_lines[i - 1]));
        copy_text(terminal_lines[23], text, sizeof(terminal_lines[23]));
    }
    terminal_top = terminal_count > 8 ? terminal_count - 8 : 0;
}

static uint32_t terminal_add_capture(const char* text){
    char line[96];
    uint32_t count = 0;
    uint32_t pos = 0;
    for(uint32_t i=0; text && text[i]; i++){
        char c = text[i];
        if(c == '\r')
            continue;
        if(c == '\n'){
            line[pos] = 0;
            terminal_add_line(pos ? line : "");
            if(count == 0 && pos)
                copy_text(terminal_view, line, sizeof(terminal_view));
            pos = 0;
            count++;
        } else if(pos + 1 < sizeof(line)){
            line[pos++] = c;
        }
    }
    if(pos){
        line[pos] = 0;
        terminal_add_line(line);
        if(count == 0)
            copy_text(terminal_view, line, sizeof(terminal_view));
        count++;
    }
    return count;
}

static uint32_t terminal_sel_lo(void){
    return terminal_sel_start < terminal_sel_end ? terminal_sel_start : terminal_sel_end;
}

static uint32_t terminal_sel_hi(void){
    return terminal_sel_start < terminal_sel_end ? terminal_sel_end : terminal_sel_start;
}

static int terminal_line_selected(uint32_t line){
    return terminal_selection && line >= terminal_sel_lo() && line <= terminal_sel_hi();
}

static void terminal_insert_char(char c);

static void terminal_select_range(uint32_t start, uint32_t end){
    if(terminal_count == 0){
        terminal_selection = 0;
        return;
    }
    if(start >= terminal_count) start = terminal_count - 1;
    if(end >= terminal_count) end = terminal_count - 1;
    terminal_sel_start = start;
    terminal_sel_end = end;
    terminal_selection = 1;
    if(terminal_sel_lo() < terminal_top)
        terminal_top = terminal_sel_lo();
    if(terminal_sel_hi() >= terminal_top + 8)
        terminal_top = terminal_sel_hi() - 7;
}

static void terminal_copy_selection(void){
    uint32_t pos = 0;
    terminal_clipboard[0] = 0;
    if(!terminal_selection || terminal_count == 0)
        return;
    for(uint32_t row=terminal_sel_lo(); row<=terminal_sel_hi() && row<terminal_count; row++){
        for(uint32_t col=0; terminal_lines[row][col] && pos + 1 < sizeof(terminal_clipboard); col++)
            terminal_clipboard[pos++] = terminal_lines[row][col];
        if(pos + 1 < sizeof(terminal_clipboard))
            terminal_clipboard[pos++] = '\n';
    }
    terminal_clipboard[pos] = 0;
}

static void terminal_paste_clipboard(void){
    uint32_t len = text_len32(terminal_input);
    for(uint32_t i=0; terminal_clipboard[i] && len + 1 < sizeof(terminal_input); i++){
        char c = terminal_clipboard[i];
        if(c == '\n' || c == '\r')
            c = ' ';
        terminal_insert_char(c);
        len = text_len32(terminal_input);
    }
}

static void terminal_seed(void){
    if(terminal_count)
        return;
    terminal_add_line("Tabla Rusa GUI Terminal");
    terminal_add_line("Type commands here, Enter runs them.");
}

static void terminal_set_input(const char* text){
    copy_text(terminal_input, text, sizeof(terminal_input));
    terminal_cursor = text_len32(terminal_input);
}

static void terminal_history_add_local(const char* text){
    if(!text || !text[0])
        return;
    if(terminal_history_count < 8){
        copy_text(terminal_history[terminal_history_count++], text, sizeof(terminal_history[0]));
    } else {
        for(uint32_t i=1; i<8; i++)
            copy_text(terminal_history[i - 1], terminal_history[i], sizeof(terminal_history[i - 1]));
        copy_text(terminal_history[7], text, sizeof(terminal_history[7]));
    }
    terminal_history_view = -1;
}

static void terminal_history_prev_local(void){
    if(terminal_history_count == 0)
        return;
    if(terminal_history_view < 0)
        terminal_history_view = (int)terminal_history_count - 1;
    else if(terminal_history_view > 0)
        terminal_history_view--;
    terminal_set_input(terminal_history[terminal_history_view]);
}

static void terminal_history_next_local(void){
    if(terminal_history_view < 0)
        return;
    if(terminal_history_view + 1 < (int)terminal_history_count){
        terminal_history_view++;
        terminal_set_input(terminal_history[terminal_history_view]);
    } else {
        terminal_history_view = -1;
        terminal_set_input("");
    }
}

static void terminal_insert_char(char c){
    uint32_t len = text_len32(terminal_input);
    if(len + 1 >= sizeof(terminal_input))
        return;
    if(terminal_cursor > len)
        terminal_cursor = len;
    for(uint32_t i=len + 1; i>terminal_cursor; i--)
        terminal_input[i] = terminal_input[i - 1];
    terminal_input[terminal_cursor++] = c;
}

static void terminal_backspace(void){
    uint32_t len = text_len32(terminal_input);
    if(len == 0 || terminal_cursor == 0)
        return;
    if(terminal_cursor > len)
        terminal_cursor = len;
    for(uint32_t i=terminal_cursor - 1; i<len; i++)
        terminal_input[i] = terminal_input[i + 1];
    terminal_cursor--;
}

static void terminal_delete_char(void){
    uint32_t len = text_len32(terminal_input);
    if(terminal_cursor >= len)
        return;
    for(uint32_t i=terminal_cursor; i<len; i++)
        terminal_input[i] = terminal_input[i + 1];
}

static void terminal_run_input(void){
    char cmd[96];
    char line[120];
    char captured[768];
    uint32_t captured_lines;
    if(!terminal_input[0])
        return;
    copy_text(cmd, terminal_input, sizeof(cmd));
    copy_text(line, "$ ", sizeof(line));
    append_text(line, cmd, sizeof(line));
    terminal_add_line(line);
    copy_text(terminal_view, cmd, sizeof(terminal_view));
    terminal_history_add_local(cmd);
    shell_history_add(cmd);
    console_capture_begin(captured, sizeof(captured));
    shell_eval(cmd);
    console_capture_end();
    captured_lines = terminal_add_capture(captured);
    if(captured_lines == 0){
        copy_text(line, "ok: ", sizeof(line));
        append_text(line, cmd, sizeof(line));
        terminal_add_line(line);
        copy_text(terminal_view, line, sizeof(terminal_view));
    }
    terminal_set_input("");
}

static void rusa_set_line(uint32_t row, const char* text){
    if(row < 5)
        copy_text(rusa_lines[row], text, sizeof(rusa_lines[row]));
}

static void rusa_refresh_status(const char* action, int result, const char* path){
    char where[72];
    char title[72];
    char detail[96];
    unsigned int line = 0;
    unsigned int col = 0;
    rusa_set_line(0, action);
    if(result == 0){
        copy_text(rusa_view, "Rusa source is healthy.", sizeof(rusa_view));
        rusa_set_line(1, "Status: ok");
        rusa_set_line(2, path);
        rusa_set_line(3, "Plain English: no syntax, scanner, or runtime issues found.");
        rusa_set_line(4, "Open Terminal is optional for raw output.");
    } else if(result == -2){
        copy_text(rusa_view, "Rusa source file was not found.", sizeof(rusa_view));
        rusa_set_line(1, "Status: source not found");
        rusa_set_line(2, path);
        rusa_set_line(3, "Plain English: the workbench could not read this file.");
        rusa_set_line(4, "Try opening a .rusa file from Files or Editor.");
    } else if(lang_last_diag(where, sizeof(where), &line, &col, title, sizeof(title), detail, sizeof(detail))){
        char pos[96];
        char num[12];
        copy_text(rusa_view, "Rusa found a friendly diagnostic.", sizeof(rusa_view));
        copy_text(pos, where, sizeof(pos));
        append_text(pos, ":", sizeof(pos));
        u32_text(line, num, sizeof(num));
        append_text(pos, num, sizeof(pos));
        append_text(pos, ":", sizeof(pos));
        u32_text(col, num, sizeof(num));
        append_text(pos, num, sizeof(pos));
        rusa_set_line(1, title);
        rusa_set_line(2, pos);
        rusa_set_line(3, detail);
        rusa_set_line(4, "Click Diagnostics or use lang open-error to jump into the editor.");
    } else {
        copy_text(rusa_view, "Rusa run/check stopped.", sizeof(rusa_view));
        rusa_set_line(1, "Status: stopped");
        rusa_set_line(2, path);
        rusa_set_line(3, "Plain English: no saved diagnostic was available.");
        rusa_set_line(4, "Open Terminal for raw command output.");
    }
}

static void rusa_workbench_action(int run_source){
    const char* path = editor_path();
    int result = lang_run_file(path, "");
    rusa_refresh_status(run_source ? "Run current Rusa file" : "Check current Rusa file", result, path);
    if(run_source)
        fs_append_line("/var/log/system.log", "gui: Rusa Workbench run");
    else
        fs_append_line("/var/log/system.log", "gui: Rusa Workbench check");
}

static void math_set_line(uint32_t row, const char* text){
    if(row < 5)
        copy_text(math_lines[row], text, sizeof(math_lines[row]));
}

static const char* math_tab_name(void){
    if(math_tab == 1) return "matrix";
    if(math_tab == 2) return "group";
    if(math_tab == 3) return "physics";
    if(math_tab == 4) return "latex";
    if(math_tab == 5) return "jobs";
    return "vector";
}

static const char* taskman_app_for_process(const char* name){
    if(str_eq(name, "shell")) return "terminal";
    if(str_eq(name, "editor")) return "editor";
    if(str_eq(name, "network")) return "network";
    if(str_eq(name, "compute")) return "math";
    if(str_eq(name, "logger")) return "logs";
    return "taskman";
}

static void taskman_select_process(const char* name){
    if(process_find(name))
        copy_text(taskman_selected, name, sizeof(taskman_selected));
}

static void taskman_boost_compute(void){
    process_set_running("compute", 1);
    process_set_compute("compute", "gui-boosted-science", 99, 90);
    jobs_account("math-worker", 8);
    taskman_select_process("compute");
}

static uint32_t taskman_connection_count(uint32_t owner_pid){
    struct net_connection_info conns[4];
    uint32_t total = net_connection_list(conns, 4);
    uint32_t count = 0;
    for(uint32_t i=0; i<total && i<4; i++)
        if(conns[i].owner_pid == owner_pid)
            count++;
    return count;
}

static void copy_first_capture_line(char* out, const char* capture, uint32_t max){
    uint32_t i = 0;
    if(max == 0)
        return;
    while(capture[i] && capture[i] != '\n' && i + 1 < max){
        out[i] = capture[i];
        i++;
    }
    out[i] = 0;
    if(i == 0)
        copy_text(out, "ok", max);
}

static void math_run_gui_result(const char* title, const char* cmd, const char* note, const char* workload){
    char local[128];
    char captured[384];
    char result[96];
    char line[96];
    copy_text(local, cmd, sizeof(local));
    console_capture_begin(captured, sizeof(captured));
    math_cmd(local);
    console_capture_end();
    copy_first_capture_line(result, captured, sizeof(result));
    math_set_line(0, title);
    copy_text(line, "math ", sizeof(line));
    append_text(line, cmd, sizeof(line));
    math_set_line(1, line);
    copy_text(line, "result: ", sizeof(line));
    append_text(line, result, sizeof(line));
    math_set_line(2, line);
    math_set_line(3, note);
    math_set_line(4, workload);
}

static void math_workbench_select(int tab){
    char cmd[96];
    math_tab = tab;
    if(math_tab == 0){
        copy_text(cmd, "vec dot 1 2 3 | 4 5 6", sizeof(cmd));
        math_run_gui_result("Vector lab", cmd, "plot: dot-product bars drawn below", "compute: vector workload priority=98");
    } else if(math_tab == 1){
        copy_text(cmd, "object matrix A 1 2 3 4", sizeof(cmd));
        math_run_gui_result("Matrix lab", cmd, "grid: 2x2 object A is available", "then run: math object det A");
    } else if(math_tab == 2){
        copy_text(cmd, "group units 12", sizeof(cmd));
        math_run_gui_result("Group theory lab", cmd, "structure: units modulo n", "algebra helpers stay terminal-compatible");
    } else if(math_tab == 3){
        copy_text(cmd, "phys fields 1 2 3 | 4 5 6", sizeof(cmd));
        math_run_gui_result("First-principles physics", cmd, "fields: electric and magnetic vectors", "jobs: physics-worker workload accounted");
    } else if(math_tab == 4){
        copy_text(cmd, "latex mat2 1 2 3 4", sizeof(cmd));
        math_run_gui_result("LaTeX converter", cmd, "export: paper/editor friendly math text", "supports vectors, matrices, fractions, polynomials");
    } else {
        copy_text(cmd, "logic modus true true", sizeof(cmd));
        math_run_gui_result("Proof and job accounting", cmd, "logic: modus ponens proof helper", "taskman shows proof-worker and compute ticks");
    }
    fs_append_line("/var/log/system.log", "gui: Math Lab tab selected");
}

static void gui_save_settings(void){
    char text[256];
    copy_text(text, "wallpaper=", sizeof(text));
    append_text(text, saver_hint, sizeof(text));
    append_text(text, "\nwallpaper_live=", sizeof(text));
    append_text(text, saver_live ? "yes" : "no", sizeof(text));
    append_text(text, "\nscreensaver=", sizeof(text));
    append_text(text, screensaver_hint, sizeof(text));
    append_text(text, "\nscreensaver_calm=", sizeof(text));
    append_text(text, screensaver_calm ? "yes" : "no", sizeof(text));
    append_text(text, "\ncursor=", sizeof(text));
    append_text(text, fb_cursor_style(), sizeof(text));
    append_text(text, "\neditor_mode=", sizeof(text));
    append_text(text, editor_mode == 1 ? "code" : (editor_mode == 2 ? "math" : "paper"), sizeof(text));
    append_text(text, "\neditor_path=", sizeof(text));
    append_text(text, editor_current_path, sizeof(text));
    append_text(text, "\nmouse_speed=", sizeof(text));
    append_text(text, settings_mouse_speed == 1 ? "slow" : (settings_mouse_speed == 3 ? "fast" : "normal"), sizeof(text));
    append_text(text, "\n", sizeof(text));
    fs_write("/config/gui.conf", text);
}

static void gui_load_settings(void){
    const char* text;
    if(fs_read("/config/gui.conf", &text) != 0)
        return;
    if(text_has(text, "wallpaper=rain")) copy_text(saver_hint, "rain", sizeof(saver_hint));
    else if(text_has(text, "wallpaper=stars")) copy_text(saver_hint, "stars", sizeof(saver_hint));
    else if(text_has(text, "wallpaper=waves")) copy_text(saver_hint, "waves", sizeof(saver_hint));
    else if(text_has(text, "wallpaper=lava")) copy_text(saver_hint, "lava", sizeof(saver_hint));
    saver_live = text_has(text, "wallpaper_live=yes");
    if(text_has(text, "screensaver=rain")) copy_text(screensaver_hint, "rain", sizeof(screensaver_hint));
    else if(text_has(text, "screensaver=stars")) copy_text(screensaver_hint, "stars", sizeof(screensaver_hint));
    else if(text_has(text, "screensaver=waves")) copy_text(screensaver_hint, "waves", sizeof(screensaver_hint));
    else if(text_has(text, "screensaver=lava")) copy_text(screensaver_hint, "lava", sizeof(screensaver_hint));
    screensaver_calm = !text_has(text, "screensaver_calm=no");
    if(text_has(text, "cursor=cross")) fb_set_cursor_style("cross");
    else if(text_has(text, "cursor=target")) fb_set_cursor_style("target");
    else if(text_has(text, "cursor=dot")) fb_set_cursor_style("dot");
    if(text_has(text, "editor_mode=code")) editor_mode = 1;
    else if(text_has(text, "editor_mode=math")) editor_mode = 2;
    else if(text_has(text, "editor_mode=paper")) editor_mode = 0;
    if(text_has(text, "mouse_speed=slow")) settings_mouse_speed = 1;
    else if(text_has(text, "mouse_speed=fast")) settings_mouse_speed = 3;
    else if(text_has(text, "mouse_speed=normal")) settings_mouse_speed = 2;
}

static void gui_preview_screensaver(void){
    fb_run_saver(screensaver_hint, screensaver_calm ? 6 : 12);
    fb_draw_text(28, 28, screensaver_calm ? "Tabla Rusa calm screensaver - Esc returns to desktop" : "Tabla Rusa OS screensaver - Esc returns to desktop", 0xFFFFFF);
    fb_set_mouse(mouse_x(), mouse_y(), mouse_buttons());
}

static uint32_t text_len32(const char* s){
    uint32_t n = 0;
    while(s && s[n]) n++;
    return n;
}

static void editor_ensure_visible(void){
    if(editor_line >= GUI_EDITOR_MAX_LINES)
        editor_line = GUI_EDITOR_MAX_LINES - 1;
    if(editor_line < editor_top)
        editor_top = editor_line;
    if(editor_line >= editor_top + GUI_EDITOR_VISIBLE_LINES)
        editor_top = editor_line - GUI_EDITOR_VISIBLE_LINES + 1;
}

static void editor_scroll(int delta){
    if(delta > 0){
        uint32_t step = (uint32_t)delta;
        editor_top = step > editor_top ? 0 : editor_top - step;
    } else if(delta < 0){
        uint32_t max_top = GUI_EDITOR_MAX_LINES - GUI_EDITOR_VISIBLE_LINES;
        editor_top += (uint32_t)(-delta);
        if(editor_top > max_top)
            editor_top = max_top;
    }
    if(editor_line < editor_top)
        editor_line = editor_top;
    if(editor_line >= editor_top + GUI_EDITOR_VISIBLE_LINES)
        editor_line = editor_top + GUI_EDITOR_VISIBLE_LINES - 1;
    if(editor_col > text_len32(editor_buf[editor_line]))
        editor_col = text_len32(editor_buf[editor_line]);
}

static void editor_line_label(uint32_t line, char* out, uint32_t max){
    uint32_t n = line + 1;
    if(max < 4) return;
    out[0] = (n >= 10) ? (char)('0' + ((n / 10) % 10)) : ' ';
    out[1] = (char)('0' + (n % 10));
    out[2] = ' ';
    out[3] = 0;
}

static uint32_t editor_sel_lo(void){
    return editor_sel_start < editor_sel_end ? editor_sel_start : editor_sel_end;
}

static uint32_t editor_sel_hi(void){
    return editor_sel_start < editor_sel_end ? editor_sel_end : editor_sel_start;
}

static int editor_line_selected(uint32_t line){
    return editor_selection && line >= editor_sel_lo() && line <= editor_sel_hi();
}

static void editor_select_range(uint32_t a, uint32_t b){
    if(a >= GUI_EDITOR_MAX_LINES) a = GUI_EDITOR_MAX_LINES - 1;
    if(b >= GUI_EDITOR_MAX_LINES) b = GUI_EDITOR_MAX_LINES - 1;
    editor_sel_start = a;
    editor_sel_end = b;
    editor_selection = 1;
    editor_line = b;
    editor_clamp_cursor();
}

static void editor_clear_selection(void){
    editor_selection = 0;
}

static void editor_copy_range(void){
    uint32_t lo = editor_selection ? editor_sel_lo() : editor_line;
    uint32_t hi = editor_selection ? editor_sel_hi() : editor_line;
    uint32_t pos = 0;
    editor_clipboard[0] = 0;
    for(uint32_t row=lo; row<=hi && row<GUI_EDITOR_MAX_LINES && pos + 2 < sizeof(editor_clipboard); row++){
        for(uint32_t col=0; editor_buf[row][col] && pos + 1 < sizeof(editor_clipboard); col++)
            editor_clipboard[pos++] = editor_buf[row][col];
        editor_clipboard[pos++] = '\n';
    }
    editor_clipboard[pos] = 0;
}

static void editor_delete_range(void){
    uint32_t lo = editor_selection ? editor_sel_lo() : editor_line;
    uint32_t hi = editor_selection ? editor_sel_hi() : editor_line;
    uint32_t count = hi - lo + 1;
    if(lo >= GUI_EDITOR_MAX_LINES)
        return;
    for(uint32_t row=lo; row + count < GUI_EDITOR_MAX_LINES; row++)
        copy_text(editor_buf[row], editor_buf[row + count], sizeof(editor_buf[row]));
    for(uint32_t row=GUI_EDITOR_MAX_LINES - count; row<GUI_EDITOR_MAX_LINES; row++)
        editor_buf[row][0] = 0;
    editor_line = lo < GUI_EDITOR_MAX_LINES ? lo : GUI_EDITOR_MAX_LINES - 1;
    editor_col = 0;
    editor_dirty = 1;
    editor_clear_selection();
    editor_clamp_cursor();
}

static void editor_paste_range(void){
    uint32_t row = editor_line;
    uint32_t col = 0;
    if(editor_selection)
        editor_delete_range();
    row = editor_line;
    editor_buf[row][0] = 0;
    for(uint32_t i=0; editor_clipboard[i] && row < GUI_EDITOR_MAX_LINES; i++){
        if(editor_clipboard[i] == '\n'){
            editor_buf[row][col] = 0;
            row++;
            col = 0;
            if(row < GUI_EDITOR_MAX_LINES)
                editor_buf[row][0] = 0;
        } else if(col + 1 < GUI_EDITOR_COLS){
            editor_buf[row][col++] = editor_clipboard[i];
            editor_buf[row][col] = 0;
        }
    }
    editor_dirty = 1;
    editor_clamp_cursor();
}

static int editor_find(const char* needle){
    if(!needle || !needle[0])
        return -1;
    copy_text(editor_find_text, needle, sizeof(editor_find_text));
    for(uint32_t row=0; row<GUI_EDITOR_MAX_LINES; row++){
        if(text_has(editor_buf[row], needle)){
            editor_find_line = (int)row;
            editor_line = row;
            editor_col = 0;
            editor_clamp_cursor();
            return (int)row;
        }
    }
    editor_find_line = -1;
    return -1;
}

static void editor_seed(void){
    for(uint32_t i=0; i<GUI_EDITOR_MAX_LINES; i++)
        editor_buf[i][0] = 0;
    if(editor_mode == 1){
        copy_text(editor_buf[0], "import std", sizeof(editor_buf[0]));
        copy_text(editor_buf[1], "", sizeof(editor_buf[1]));
        copy_text(editor_buf[2], "fn main() {", sizeof(editor_buf[2]));
        copy_text(editor_buf[3], "  let force: int = 42", sizeof(editor_buf[3]));
        copy_text(editor_buf[4], "  print force", sizeof(editor_buf[4]));
        copy_text(editor_buf[5], "}", sizeof(editor_buf[5]));
    } else if(editor_mode == 2){
        copy_text(editor_buf[0], "# Tabla Rusa math notes", sizeof(editor_buf[0]));
        copy_text(editor_buf[1], "", sizeof(editor_buf[1]));
        copy_text(editor_buf[2], "Vector: dot([1,2,3], [4,5,6]) = 32", sizeof(editor_buf[2]));
        copy_text(editor_buf[3], "Matrix: det([[1,2],[3,4]]) = -2", sizeof(editor_buf[3]));
        copy_text(editor_buf[4], "Physics: F = G*M*m/r^2", sizeof(editor_buf[4]));
        copy_text(editor_buf[5], "LaTeX: use math latex mat2 1 2 3 4", sizeof(editor_buf[5]));
    } else {
        copy_text(editor_buf[0], "Title: Tabla Rusa field notes", sizeof(editor_buf[0]));
        copy_text(editor_buf[1], "", sizeof(editor_buf[1]));
        copy_text(editor_buf[2], "Abstract", sizeof(editor_buf[2]));
        copy_text(editor_buf[3], "A readable paper draft with sections,", sizeof(editor_buf[3]));
        copy_text(editor_buf[4], "paragraph flow, and save/open controls.", sizeof(editor_buf[4]));
    }
    editor_line = 0;
    editor_col = text_len32(editor_buf[0]);
    editor_top = 0;
    editor_clear_selection();
    editor_find_line = -1;
    editor_dirty = 0;
}

static void editor_load_file(void){
    const char* text;
    editor_seed();
    if(fs_read(editor_path(), &text) == 0){
        uint32_t row = 0;
        uint32_t col = 0;
        for(uint32_t i=0; text[i] && row < GUI_EDITOR_MAX_LINES; i++){
            if(text[i] == '\n'){
                editor_buf[row][col] = 0;
                row++;
                col = 0;
            } else if(col + 1 < GUI_EDITOR_COLS){
                editor_buf[row][col++] = text[i];
                editor_buf[row][col] = 0;
            }
        }
    }
    editor_line = 0;
    editor_col = text_len32(editor_buf[0]);
    editor_top = 0;
    editor_dirty = 0;
}

static void editor_save_file(void){
    char text[GUI_EDITOR_MAX_LINES * GUI_EDITOR_COLS];
    uint32_t pos = 0;
    uint32_t last = 0;
    text[0] = 0;
    for(uint32_t row=0; row<GUI_EDITOR_MAX_LINES; row++)
        if(editor_buf[row][0])
            last = row;
    for(uint32_t row=0; row<=last && pos + 2 < sizeof(text); row++){
        for(uint32_t col=0; editor_buf[row][col] && pos + 1 < sizeof(text); col++)
            text[pos++] = editor_buf[row][col];
        text[pos++] = '\n';
    }
    text[pos] = 0;
    fs_write(editor_path(), text);
    editor_dirty = 0;
}

static void editor_set_mode(int mode){
    if(editor_mode != mode){
        editor_mode = mode;
        if(editor_mode == 1)
            copy_text(editor_current_path, "/home/projects/demo.rusa", sizeof(editor_current_path));
        else if(editor_mode == 2)
            copy_text(editor_current_path, "/home/math/notes.md", sizeof(editor_current_path));
        else
            copy_text(editor_current_path, "/home/notes.txt", sizeof(editor_current_path));
        editor_load_file();
    } else {
        editor_mode = mode;
    }
}

static void editor_open_path(const char* path){
    if(path && path[0])
        copy_text(editor_current_path, path, sizeof(editor_current_path));
    if(text_has(editor_current_path, ".rusa"))
        editor_mode = 1;
    else if(text_has(editor_current_path, ".md") || text_has(editor_current_path, "/home/math"))
        editor_mode = 2;
    editor_load_file();
}

static void editor_dialog_select_first(void){
    char name[40];
    int type = 0;
    if(fs_child_name(editor_dialog_dir, 0, name, sizeof(name), &type) == 0)
        fs_join_path(editor_dialog_dir, name, editor_dialog_selected, sizeof(editor_dialog_selected));
    else
        copy_text(editor_dialog_selected, editor_dialog_dir, sizeof(editor_dialog_selected));
}

static void editor_dialog_open(int mode){
    editor_dialog_mode = mode;
    file_parent_path(editor_current_path, editor_dialog_dir, sizeof(editor_dialog_dir));
    editor_dialog_select_first();
    copy_text(launch_notice, mode == 2 ? "save as dialog" : "open dialog", sizeof(launch_notice));
}

static void editor_dialog_select_index(uint32_t index){
    char name[40];
    int type = 0;
    if(fs_child_name(editor_dialog_dir, (int)index, name, sizeof(name), &type) == 0)
        fs_join_path(editor_dialog_dir, name, editor_dialog_selected, sizeof(editor_dialog_selected));
}

static void editor_dialog_up(void){
    file_parent_path(editor_dialog_dir, editor_dialog_dir, sizeof(editor_dialog_dir));
    editor_dialog_select_first();
    copy_text(launch_notice, "dialog folder up", sizeof(launch_notice));
}

static void editor_dialog_confirm(void){
    int type = 0;
    size_t size = 0;
    if(editor_dialog_mode == 0)
        return;
    if(fs_stat(editor_dialog_selected, &type, &size) == 0 && type == 1){
        copy_text(editor_dialog_dir, editor_dialog_selected, sizeof(editor_dialog_dir));
        editor_dialog_select_first();
        copy_text(launch_notice, "dialog folder opened", sizeof(launch_notice));
        return;
    }
    if(editor_dialog_mode == 1){
        editor_open_path(editor_dialog_selected);
        copy_text(launch_notice, "dialog file opened", sizeof(launch_notice));
    } else {
        copy_text(editor_current_path, editor_dialog_selected, sizeof(editor_current_path));
        if(text_has(editor_current_path, ".rusa"))
            editor_mode = 1;
        editor_save_file();
        copy_text(launch_notice, "dialog file saved", sizeof(launch_notice));
    }
    gui_save_settings();
    editor_dialog_mode = 0;
}

static void editor_dialog_save_quick(int rusa_file){
    copy_text(editor_dialog_selected, rusa_file ? "/home/projects/untitled.rusa" : "/home/untitled.txt",
              sizeof(editor_dialog_selected));
    editor_dialog_mode = 2;
    editor_dialog_confirm();
}

static void editor_dialog_cancel(void){
    editor_dialog_mode = 0;
    copy_text(launch_notice, "dialog canceled", sizeof(launch_notice));
}

static void files_select_index(uint32_t index){
    char name[40];
    int type = 0;
    if(fs_child_name(file_dir, (int)index, name, sizeof(name), &type) != 0)
        return;
    fs_join_path(file_dir, name, file_selected, sizeof(file_selected));
}

static void file_parent_path(const char* path, char* out, uint32_t max){
    uint32_t last = 0;
    uint32_t i = 0;
    if(max == 0) return;
    if(!path || !path[0] || str_eq(path, "/")){
        copy_text(out, "/", max);
        return;
    }
    while(path[i]){
        if(path[i] == '/' && i > 0)
            last = i;
        i++;
    }
    if(last == 0){
        copy_text(out, "/", max);
        return;
    }
    for(i=0; i<last && i + 1 < max; i++)
        out[i] = path[i];
    out[i] = 0;
}

static void files_select_first(void){
    char name[40];
    int type = 0;
    if(fs_child_name(file_dir, 0, name, sizeof(name), &type) == 0)
        fs_join_path(file_dir, name, file_selected, sizeof(file_selected));
    else
        copy_text(file_selected, file_dir, sizeof(file_selected));
}

static void files_go_up(void){
    file_parent_path(file_dir, file_dir, sizeof(file_dir));
    files_select_first();
    copy_text(launch_notice, "folder up", sizeof(launch_notice));
}

static void files_new_file(const char* name){
    char path[96];
    fs_join_path(file_dir, name && name[0] ? name : "new.txt", path, sizeof(path));
    fs_write(path, "");
    copy_text(file_selected, path, sizeof(file_selected));
    copy_text(launch_notice, "file created", sizeof(launch_notice));
}

static void files_new_folder(const char* name){
    char path[96];
    fs_join_path(file_dir, name && name[0] ? name : "folder", path, sizeof(path));
    fs_mkdir(path);
    copy_text(file_selected, path, sizeof(file_selected));
    copy_text(launch_notice, "folder created", sizeof(launch_notice));
}

static void files_rename_selected(const char* name){
    if(!name || !name[0])
        name = "renamed.txt";
    if(fs_rename(file_selected, name) == 0){
        fs_join_path(file_dir, name, file_selected, sizeof(file_selected));
        copy_text(launch_notice, "item renamed", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "rename failed", sizeof(launch_notice));
    }
}

static const char* path_basename(const char* path){
    const char* base = path;
    for(uint32_t i=0; path && path[i]; i++)
        if(path[i] == '/' && path[i + 1])
            base = path + i + 1;
    return base;
}

static void files_copy_selected(const char* name){
    char path[96];
    const char* target = name && name[0] ? name : "copy.txt";
    if(!name || !name[0]){
        copy_text(path, "copy-", sizeof(path));
        append_text(path, path_basename(file_selected), sizeof(path));
        target = path;
    }
    fs_join_path(file_dir, target, path, sizeof(path));
    if(fs_copy(file_selected, path) == 0){
        copy_text(file_selected, path, sizeof(file_selected));
        copy_text(launch_notice, "item copied", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "copy failed", sizeof(launch_notice));
    }
}

static void files_move_selected(const char* name){
    char path[96];
    const char* target = name && name[0] ? name : "moved.txt";
    fs_join_path(file_dir, target, path, sizeof(path));
    if(fs_move(file_selected, path) == 0){
        copy_text(file_selected, path, sizeof(file_selected));
        copy_text(launch_notice, "item moved", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "move failed", sizeof(launch_notice));
    }
}

static void files_delete_selected(void){
    if(fs_rm(file_selected) == 0){
        files_select_first();
        copy_text(launch_notice, "item deleted", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "delete failed", sizeof(launch_notice));
    }
}

static void files_open_selected_editor(void){
    int type = 0;
    size_t size = 0;
    if(fs_stat(file_selected, &type, &size) == 0 && type == 2){
        editor_open_path(file_selected);
        gui_focus_app("editor");
        copy_text(launch_notice, "file opened in editor", sizeof(launch_notice));
    }
}

static void files_open_selected_terminal(void){
    int type = 0;
    size_t size = 0;
    char command[128];
    if(fs_stat(file_selected, &type, &size) != 0)
        return;
    copy_text(command, type == 1 ? "tree " : "cat ", sizeof(command));
    append_text(command, file_selected, sizeof(command));
    request_terminal_command(command, "open in terminal");
}

static void files_open_selected_rusa(void){
    if(text_has(file_selected, ".rusa")){
        editor_open_path(file_selected);
        copy_text(rusa_view, "Selected file ready for Rusa run/check.", sizeof(rusa_view));
        gui_focus_app("rusa");
        copy_text(launch_notice, "opened with Rusa", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "not a .rusa file", sizeof(launch_notice));
    }
}

static void files_open_selected_default(void){
    int type = 0;
    size_t size = 0;
    if(fs_stat(file_selected, &type, &size) != 0)
        return;
    if(type == 1){
        copy_text(file_dir, file_selected, sizeof(file_dir));
        files_select_first();
        copy_text(launch_notice, "folder opened", sizeof(launch_notice));
    } else if(text_has(file_selected, ".rusa")){
        files_open_selected_rusa();
    } else {
        files_open_selected_editor();
    }
}

static const char* app_title(void){
    return app_title_for(active_app);
}

static const char* editor_path(void){
    return editor_current_path;
}

static const char* rusa_tab_name(void){
    if(rusa_tab == 1) return "keywords";
    if(rusa_tab == 2) return "docs";
    if(rusa_tab == 3) return "check";
    if(rusa_tab == 4) return "run";
    if(rusa_tab == 5) return "diagnostics";
    if(rusa_tab == 6) return "packages";
    return "examples";
}

void gui_active_terminal_command(char* out, uint32_t max){
    if(max == 0) return;
    out[0] = 0;
    if(launch_override[0]){
        copy_text(out, launch_override, max);
        launch_override[0] = 0;
    } else if(active_is("files")){
        copy_text(out, "tree ", max);
        append_text(out, file_dir, max);
    }
    else if(active_is("taskman")) copy_text(out, "taskman top", max);
    else if(active_is("math")){
        if(math_tab == 1) copy_text(out, "math object det A", max);
        else if(math_tab == 2) copy_text(out, "math group units 12", max);
        else if(math_tab == 3) copy_text(out, "math phys fields 1 2 3 | 4 5 6", max);
        else if(math_tab == 4) copy_text(out, "math latex mat2 1 2 3 4", max);
        else if(math_tab == 5) copy_text(out, "taskman top", max);
        else copy_text(out, "math vec dot 1 2 3 | 4 5 6", max);
    }
    else if(active_is("rusa")){
        if(rusa_tab == 1) copy_text(out, "rusa keywords", max);
        else if(rusa_tab == 2) copy_text(out, "rusa docs", max);
        else if(rusa_tab == 3) copy_text(out, "rusa check /home/projects/demo.rusa", max);
        else if(rusa_tab == 4) copy_text(out, "rusa run /home/projects/demo.rusa", max);
        else if(rusa_tab == 5) copy_text(out, "rusa last-error", max);
        else if(rusa_tab == 6) copy_text(out, "pkg list", max);
        else copy_text(out, "rusa examples", max);
    }
    else if(active_is("privacy")){
        if(settings_tab == 1) copy_text(out, "hardware status", max);
        else if(settings_tab == 2) copy_text(out, "keyboard status", max);
        else if(settings_tab == 3) copy_text(out, "hardware gpu", max);
        else if(settings_tab == 4) copy_text(out, "mouse status", max);
        else copy_text(out, "privacy status", max);
    }
    else if(active_is("network")) copy_text(out, "net status", max);
    else if(active_is("saver")){
        copy_text(out, "fb saver ", max);
        append_text(out, screensaver_hint, max);
        append_text(out, screensaver_calm ? " 6" : " 12", max);
    }
    else if(active_is("projects")) copy_text(out, "project list", max);
    else if(active_is("packages")) copy_text(out, "pkg list", max);
    else if(active_is("logs")) copy_text(out, "log show system", max);
    else if(active_is("security")){
        if(security_tab == 1) copy_text(out, "net connections", max);
        else if(security_tab == 2) copy_text(out, "security permissions", max);
        else if(security_tab == 3) copy_text(out, "service list", max);
        else if(security_tab == 4) copy_text(out, "security scan /home/projects/demo.rusa", max);
        else if(security_tab == 5) copy_text(out, "security permissions", max);
        else if(security_tab == 6) copy_text(out, "security events", max);
        else copy_text(out, "security status", max);
    }
    else if(active_is("events")) copy_text(out, "event list", max);
    else if(active_is("storage")) copy_text(out, "block status", max);
    else if(active_is("inspector")) copy_text(out, "inspect memory", max);
    else if(active_is("editor")){
        copy_text(out, "edit ", max);
        append_text(out, editor_path(), max);
    } else if(active_is("terminal")) copy_text(out, terminal_view, max);
}

void gui_terminal_input_text(char* out, uint32_t max){
    copy_text(out, terminal_input, max);
}

void gui_terminal_clear_input(void){
    terminal_set_input("");
}

static void request_terminal_command(const char* command, const char* notice){
    copy_text(launch_override, command, sizeof(launch_override));
    copy_text(launch_notice, notice, sizeof(launch_notice));
    copy_text(terminal_view, command, sizeof(terminal_view));
    terminal_requested = 1;
}

static void gui_focus_app(const char* app){
    app = canonical_app(app);
    save_active_window_geometry();
    app_open(app);
    copy_text(active_app, app, sizeof(active_app));
    load_active_window_geometry();
    editor_focused = str_eq(app, "editor") ? editor_focused : 0;
    terminal_focused = str_eq(app, "terminal");
    if(str_eq(app, "terminal")){
        terminal_seed();
        window_focus("shell");
    }
    else if(str_eq(app, "inspector") || str_eq(app, "taskman")) window_focus("inspector");
    else if(str_eq(app, "editor") || str_eq(app, "rusa") || str_eq(app, "files") ||
            str_eq(app, "projects") || str_eq(app, "packages")) window_focus("editor");
    else if(str_eq(app, "network") || str_eq(app, "privacy") || str_eq(app, "saver") ||
            str_eq(app, "logs") || str_eq(app, "security") || str_eq(app, "events") ||
            str_eq(app, "storage")) window_focus("network");
}

static void draw_dock_item(uint32_t x, const char* app, const char* label, uint32_t color){
    uint32_t edge = active_is(app) && !app_is_minimized(app) ? 0xFFFFFF :
                    (app_is_minimized(app) ? 0xC8A848 : 0x202830);
    fb_fill_rect(x - 2, 728, 82, 34, edge);
    fb_fill_rect(x, 730, 78, 30, app_is_minimized(app) ? 0x3A3F46 : color);
    fb_draw_text(x + 8, 742, label, 0xFFFFFF);
    if(app_is_minimized(app))
        fb_fill_rect(x + 62, 754, 10, 2, 0xFFD28A);
}

static void draw_launcher_menu(void){
    if(!launcher_open)
        return;
    fb_fill_rect(18, 372, 314, 338, 0x101820);
    fb_fill_rect(22, 376, 306, 330, 0xF4F7FA);
    fb_fill_rect(22, 376, 306, 34, 0x253444);
    fb_draw_text(38, 398, "Start", 0xFFFFFF);
    for(uint32_t i=0; i<sizeof(launcher_entries)/sizeof(launcher_entries[0]); i++){
        uint32_t y = 426 + i * 26;
        uint32_t color = app_is_open(launcher_entries[i].app) ? 0xDDEBFF : 0xFFFFFF;
        fb_fill_rect(36, y - 14, 278, 22, color);
        fb_draw_text(48, y, launcher_entries[i].label, 0x223040);
        fb_draw_text(160, y, launcher_entries[i].hint, 0x536070);
    }
    fb_draw_text(42, 692, "Super opens menu  Alt+Tab cycles  Ctrl+Q closes", 0x536070);
}

static void draw_app_line(uint32_t row, const char* label, const char* value){
    uint32_t y = 166 + row * 34;
    app_draw_text(226, y, label, 0xCFE8FF);
    app_draw_text(378, y, value, 0xFFFFFF);
}

static void draw_icon_art(uint32_t x, uint32_t y, const char* app, uint32_t color){
    fb_fill_rect(x, y, 58, 52, 0xE8EEF4);
    if(str_eq(app, "files")){
        fb_fill_rect(x + 6, y + 14, 46, 30, color);
        fb_fill_rect(x + 10, y + 8, 22, 8, color);
    } else if(str_eq(app, "terminal")){
        fb_fill_rect(x + 6, y + 8, 46, 36, 0x101820);
        fb_fill_rect(x + 14, y + 18, 16, 4, 0x8EE8A0);
        fb_fill_rect(x + 30, y + 28, 16, 4, 0x8EE8A0);
    } else if(str_eq(app, "math")){
        fb_fill_rect(x + 12, y + 36, 36, 4, color);
        fb_fill_rect(x + 28, y + 10, 4, 34, color);
        fb_draw_text(x + 16, y + 22, "M", 0x101820);
    } else if(str_eq(app, "rusa")){
        fb_fill_rect(x + 12, y + 8, 34, 38, color);
        fb_draw_text(x + 22, y + 24, "R", 0xFFFFFF);
    } else if(str_eq(app, "privacy")){
        fb_fill_rect(x + 16, y + 24, 26, 20, color);
        fb_fill_rect(x + 22, y + 12, 14, 14, color);
        fb_fill_rect(x + 26, y + 18, 6, 8, 0xE8EEF4);
    } else if(str_eq(app, "taskman")){
        fb_fill_rect(x + 12, y + 32, 6, 12, color);
        fb_fill_rect(x + 24, y + 22, 6, 22, color);
        fb_fill_rect(x + 36, y + 14, 6, 30, color);
    } else if(str_eq(app, "network")){
        fb_fill_rect(x + 12, y + 14, 10, 10, color);
        fb_fill_rect(x + 38, y + 14, 10, 10, color);
        fb_fill_rect(x + 25, y + 34, 10, 10, color);
        fb_fill_rect(x + 20, y + 20, 24, 4, color);
        fb_fill_rect(x + 28, y + 22, 4, 18, color);
    } else if(str_eq(app, "saver")){
        fb_fill_rect(x + 10, y + 16, 38, 4, color);
        fb_fill_rect(x + 16, y + 26, 32, 4, color);
        fb_fill_rect(x + 10, y + 36, 38, 4, color);
    } else if(str_eq(app, "editor")){
        fb_fill_rect(x + 14, y + 8, 30, 38, color);
        fb_fill_rect(x + 20, y + 18, 18, 3, 0xFFFFFF);
        fb_fill_rect(x + 20, y + 26, 18, 3, 0xFFFFFF);
        fb_fill_rect(x + 20, y + 34, 12, 3, 0xFFFFFF);
    } else if(str_eq(app, "projects")){
        fb_fill_rect(x + 10, y + 12, 38, 28, color);
        fb_draw_text(x + 20, y + 30, "P", 0xFFFFFF);
    } else if(str_eq(app, "packages")){
        fb_fill_rect(x + 12, y + 14, 34, 28, color);
        fb_fill_rect(x + 18, y + 8, 22, 8, color);
        fb_draw_text(x + 22, y + 30, "K", 0xFFFFFF);
    } else if(str_eq(app, "logs")){
        fb_fill_rect(x + 12, y + 8, 34, 38, color);
        fb_fill_rect(x + 18, y + 18, 22, 3, 0xFFFFFF);
        fb_fill_rect(x + 18, y + 28, 22, 3, 0xFFFFFF);
        fb_fill_rect(x + 18, y + 38, 16, 3, 0xFFFFFF);
    } else if(str_eq(app, "security")){
        fb_fill_rect(x + 14, y + 18, 30, 24, color);
        fb_fill_rect(x + 22, y + 8, 14, 14, color);
        fb_draw_text(x + 23, y + 34, "S", 0xFFFFFF);
    } else if(str_eq(app, "events")){
        fb_fill_rect(x + 14, y + 12, 8, 8, color);
        fb_fill_rect(x + 36, y + 12, 8, 8, color);
        fb_fill_rect(x + 25, y + 34, 8, 8, color);
        fb_fill_rect(x + 20, y + 18, 20, 4, color);
        fb_fill_rect(x + 28, y + 22, 4, 16, color);
    } else if(str_eq(app, "storage")){
        fb_fill_rect(x + 12, y + 14, 34, 28, color);
        fb_fill_rect(x + 16, y + 34, 26, 4, 0xFFFFFF);
        fb_fill_rect(x + 36, y + 20, 4, 4, 0xFFFFFF);
    } else {
        fb_fill_rect(x + 12, y + 10, 34, 34, color);
    }
}

static void draw_desktop_icon(uint32_t x, uint32_t y, const char* app, const char* label, uint32_t color){
    uint32_t edge = active_is(app) ? 0xFFFFFF : (app_is_open(app) ? 0x89B8D8 : 0x14324A);
    fb_fill_rect(x - 8, y - 8, 104, 92, edge);
    fb_fill_rect(x - 5, y - 5, 98, 86, 0x14324A);
    draw_icon_art(x + 15, y, app, color);
    fb_draw_text(x + 8, y + 66, label, 0xFFFFFF);
}

static void draw_button(uint32_t x, uint32_t y, uint32_t w, const char* label, uint32_t color){
    app_fill_rect(x, y, w, 28, 0xB8C1CC);
    app_fill_rect(x + 1, y + 1, w - 2, 26, 0xFFFFFF);
    app_fill_rect(x + 2, y + 2, w - 4, 24, color);
    app_draw_text(x + 12, y + 18, label, 0xFFFFFF);
}

static void draw_mode_button(uint32_t x, uint32_t y, uint32_t w, const char* label, int selected, uint32_t color){
    app_fill_rect(x, y, w, 30, selected ? 0xFFFFFF : 0xB8C1CC);
    app_fill_rect(x + 1, y + 1, w - 2, 28, selected ? color : 0xF2F5F8);
    app_draw_text(x + 12, y + 19, label, selected ? 0xFFFFFF : 0x223040);
}

static void draw_editor_surface(void){
    char prefix[8];
    char line_info[36];
    app_fill_rect(226, 328, 700, 260, 0xFFFFFF);
    app_fill_rect(226, 328, 150, 260, 0x26313C);
    app_draw_text(242, 352, "Explorer", 0xCFE8FF);
    app_draw_text(242, 386, editor_mode == 1 ? "demo.rusa" : (editor_mode == 2 ? "notes.md" : "notes.txt"), 0xFFFFFF);
    app_draw_text(242, 420, editor_dirty ? "unsaved" : "saved", editor_dirty ? 0xFFD28A : 0xCFE8FF);
    app_draw_text(242, 454, editor_focused ? "typing on" : "click page", 0xCFE8FF);
    app_fill_rect(388, 346, 518, 220, 0xF8FAFC);
    for(uint32_t row=0; row<GUI_EDITOR_VISIBLE_LINES; row++){
        uint32_t doc_row = editor_top + row;
        uint32_t y = 372 + row * 20;
        prefix[0] = (doc_row == editor_line && editor_focused) ? '>' : ' ';
        editor_line_label(doc_row, prefix + 1, sizeof(prefix) - 1);
        if(editor_line_selected(doc_row))
            app_fill_rect(398, y - 4, 500, 16, 0xDDEBFF);
        else if((int)doc_row == editor_find_line)
            app_fill_rect(398, y - 4, 500, 16, 0xFFF0B8);
        app_draw_text(404, y, prefix, 0x6A7580);
        app_draw_text(438, y, editor_buf[doc_row], 0x223040);
        if(doc_row == editor_line && editor_focused){
            uint32_t cx = 438 + editor_col * GUI_FONT_ADVANCE;
            if(cx > 894) cx = 894;
            app_fill_rect(cx, y, 2, 8, 0x111820);
        }
    }
    app_fill_rect(912, 346, 6, 220, 0xD0D8E0);
    app_fill_rect(912, 346 + (editor_top * 180) / (GUI_EDITOR_MAX_LINES - GUI_EDITOR_VISIBLE_LINES),
                 6, 40, 0x536070);
    copy_text(line_info, "Line ", sizeof(line_info));
    char num[8];
    u32_text(editor_line + 1, num, sizeof(num));
    append_text(line_info, num, sizeof(line_info));
    append_text(line_info, "/", sizeof(line_info));
    u32_text(GUI_EDITOR_MAX_LINES, num, sizeof(num));
    append_text(line_info, num, sizeof(line_info));
    app_draw_text(410, 562, line_info, 0x2E6B4C);
    app_draw_text(538, 562, editor_mode == 1 ? "Code: arrows/wheel/Page edit, Save writes demo.rusa" :
                                      (editor_mode == 2 ? "Math: markdown notes, formulas, Save writes notes.md" :
                                       "Paper: arrows/wheel/Page edit, Save writes notes.txt"), 0x2E6B4C);
}

static void draw_editor_file_dialog(void){
    char name[40];
    char row[84];
    int type = 0;
    if(!editor_dialog_mode)
        return;
    app_fill_rect(286, 218, 560, 330, 0x1C2630);
    app_fill_rect(292, 224, 548, 318, 0xF6F8FA);
    app_fill_rect(292, 224, 548, 36, editor_dialog_mode == 2 ? 0x4F7088 : 0x345A7A);
    app_draw_text(312, 246, editor_dialog_mode == 2 ? "Save As" : "Open File", 0xFFFFFF);
    app_draw_text(312, 286, "Folder", 0x345A7A);
    app_draw_text(402, 286, editor_dialog_dir, 0x223040);
    app_draw_text(312, 314, "Selected", 0x345A7A);
    app_draw_text(402, 314, editor_dialog_selected, 0x223040);
    draw_button(312, 336, 64, "Up", 0x345A7A);
    draw_button(390, 336, 96, editor_dialog_mode == 2 ? "Save" : "Open", 0x3C704C);
    draw_button(500, 336, 96, "Cancel", 0xA84A4A);
    if(editor_dialog_mode == 2){
        draw_button(610, 336, 88, ".txt", 0x4F7088);
        draw_button(710, 336, 88, ".rusa", 0x725C9A);
    }
    app_fill_rect(312, 380, 500, 132, 0xFFFFFF);
    for(uint32_t i=0; i<5; i++){
        if(fs_child_name(editor_dialog_dir, (int)i, name, sizeof(name), &type) != 0)
            break;
        fs_join_path(editor_dialog_dir, name, row, sizeof(row));
        if(str_eq(row, editor_dialog_selected))
            app_fill_rect(320, 390 + i * 24, 480, 18, 0xDDEBFF);
        copy_text(row, type == 1 ? "[dir]  " : "[file] ", sizeof(row));
        append_text(row, name, sizeof(row));
        app_draw_text(330, 404 + i * 24, row, 0x223040);
    }
}

static void draw_rusa_surface(void){
    app_fill_rect(226, 328, 700, 230, 0xFFFFFF);
    app_fill_rect(226, 328, 160, 230, 0x2C243C);
    app_draw_text(244, 354, "Rusa", 0xFFFFFF);
    app_draw_text(244, 388, ".rusa files", 0xCFE8FF);
    app_draw_text(244, 422, "packages", 0xCFE8FF);
    app_draw_text(244, 456, "diagnostics", 0xCFE8FF);
    app_fill_rect(404, 348, 500, 186, 0xF8FAFC);
    app_draw_text(426, 374, "Tab", 0x223040);
    app_draw_text(510, 374, rusa_tab_name(), 0x5B3C9A);
    if(rusa_tab == 1){
        app_draw_text(426, 414, "Keywords: import let set fn return if else while repeat", 0x223040);
        app_draw_text(426, 448, "Objects: file process service window program math phys", 0x223040);
    } else if(rusa_tab == 2){
        app_draw_text(426, 414, "Docs: readable syntax, curly blocks, typed values", 0x223040);
        app_draw_text(426, 448, "Use Open Terminal for the matching docs command.", 0x223040);
    } else if(rusa_tab == 3){
        for(uint32_t i=0; i<5; i++)
            app_draw_text(426, 414 + i * 24, rusa_lines[i], i == 0 ? 0x5B3C9A : 0x223040);
    } else if(rusa_tab == 4){
        for(uint32_t i=0; i<5; i++)
            app_draw_text(426, 414 + i * 24, rusa_lines[i], i == 0 ? 0x5B3C9A : 0x223040);
    } else if(rusa_tab == 5){
        for(uint32_t i=0; i<5; i++)
            app_draw_text(426, 414 + i * 24, rusa_lines[i], i == 0 ? 0xA84A4A : 0x223040);
    } else if(rusa_tab == 6){
        app_draw_text(426, 414, "Packages: std, docs, math, physics, gui, net", 0x223040);
        app_draw_text(426, 448, "Imports: current source -> /lib/rusa/std.rusa", 0x223040);
        app_draw_text(426, 482, "Open Terminal runs: pkg list", 0x223040);
    } else {
        app_draw_text(426, 414, "Source", 0x5B3C9A);
        app_draw_text(510, 414, editor_path(), 0x223040);
        app_draw_text(426, 448, "Examples: variables, functions, loops, imports, events.", 0x223040);
        app_draw_text(426, 482, "Use Check/Run tabs for the current editor file.", 0x223040);
    }
}

static void draw_math_surface(void){
    uint32_t colors[6] = {0x345A7A, 0x3C704C, 0x725C9A, 0x386878, 0x887034, 0x4F7088};
    app_fill_rect(226, 328, 700, 230, 0xFFFFFF);
    app_fill_rect(226, 328, 170, 230, 0x203044);
    app_draw_text(246, 354, "Math Lab", 0xFFFFFF);
    app_draw_text(246, 388, "linear algebra", 0xCFE8FF);
    app_draw_text(246, 422, "algebra", 0xCFE8FF);
    app_draw_text(246, 456, "physics", 0xCFE8FF);
    app_draw_text(246, 490, "publishing", 0xCFE8FF);
    app_fill_rect(416, 348, 488, 186, 0xF8FAFC);
    app_fill_rect(416, 348, 488, 28, colors[math_tab]);
    app_draw_text(432, 366, math_tab_name(), 0xFFFFFF);
    for(uint32_t i=0; i<5; i++)
        app_draw_text(432, 408 + i * 24, math_lines[i], i == 0 ? colors[math_tab] : 0x223040);
    app_fill_rect(632, 486, 248, 34, 0xFFFFFF);
    if(math_tab == 0){
        app_fill_rect(646, 506, 32, 8, 0x345A7A);
        app_fill_rect(690, 498, 44, 16, 0x3C704C);
        app_fill_rect(746, 490, 56, 24, 0x725C9A);
        app_draw_text(812, 512, "dot", 0x345A7A);
    } else if(math_tab == 1){
        app_fill_rect(650, 492, 38, 18, 0xDDEBFF);
        app_fill_rect(692, 492, 38, 18, 0xEAF6EA);
        app_fill_rect(650, 512, 38, 18, 0xF8E8E8);
        app_fill_rect(692, 512, 38, 18, 0xF8F0D8);
        app_draw_text(750, 512, "2x2", 0x3C704C);
    } else if(math_tab == 3){
        app_fill_rect(646, 504, 84, 3, 0x3A86A8);
        app_fill_rect(646, 514, 114, 3, 0xA84A4A);
        app_fill_rect(646, 494, 3, 32, 0x223040);
        app_draw_text(778, 512, "E/B", 0x386878);
    } else if(math_tab == 4){
        app_draw_text(646, 512, "\\begin{bmatrix}...\\end{bmatrix}", 0x887034);
    } else {
        app_draw_text(646, 512, "jobs -> taskman", colors[math_tab]);
    }
}

static void draw_terminal_surface(void){
    char prompt[120];
    uint32_t cx;
    app_fill_rect(226, 208, 700, 350, 0x101820);
    app_fill_rect(226, 208, 700, 28, 0x1E2A36);
    app_draw_text(244, 226, "GUI Terminal", 0x8EE8A0);
    uint32_t visible = terminal_count - terminal_top;
    if(visible > 8) visible = 8;
    for(uint32_t i=0; i<visible; i++){
        uint32_t row = terminal_top + i;
        if(terminal_line_selected(row))
            app_fill_rect(244, 252 + i * 24, 660, 20, 0x294058);
        app_draw_text(250, 268 + i * 24, terminal_lines[row], terminal_line_selected(row) ? 0x8EE8A0 : 0xFFFFFF);
    }
    copy_text(prompt, "tr:gui $ ", sizeof(prompt));
    append_text(prompt, terminal_input, sizeof(prompt));
    app_fill_rect(244, 502, 660, 32, terminal_focused ? 0x213040 : 0x18222C);
    app_draw_text(252, 522, prompt, terminal_focused ? 0x8EE8A0 : 0xCFE8FF);
    if(terminal_focused){
        cx = 252 + (9 + terminal_cursor) * GUI_FONT_ADVANCE;
        if(cx > 894) cx = 894;
        app_fill_rect(cx, 522, 2, 9, 0x8EE8A0);
    }
    app_draw_text(250, 548, terminal_clipboard[0] ? "select/copy/paste available. Clipboard has terminal text." : "Click input area, type command, Enter runs in place.", 0xCFE8FF);
}

struct gui_render_context {
    char active[16];
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t restore_x;
    uint32_t restore_y;
    uint32_t restore_w;
    uint32_t restore_h;
    int maximized;
};

static void gui_capture_render_context(struct gui_render_context* ctx){
    copy_text(ctx->active, active_app, sizeof(ctx->active));
    ctx->x = gui_win_x;
    ctx->y = gui_win_y;
    ctx->w = gui_win_w;
    ctx->h = gui_win_h;
    ctx->restore_x = gui_restore_x;
    ctx->restore_y = gui_restore_y;
    ctx->restore_w = gui_restore_w;
    ctx->restore_h = gui_restore_h;
    ctx->maximized = gui_window_maximized;
}

static void gui_restore_render_context(const struct gui_render_context* ctx){
    copy_text(active_app, ctx->active, sizeof(active_app));
    gui_win_x = ctx->x;
    gui_win_y = ctx->y;
    gui_win_w = ctx->w;
    gui_win_h = ctx->h;
    gui_restore_x = ctx->restore_x;
    gui_restore_y = ctx->restore_y;
    gui_restore_w = ctx->restore_w;
    gui_restore_h = ctx->restore_h;
    gui_window_maximized = ctx->maximized;
}

static void gui_bind_window_for_render(const struct gui_app_window* win){
    copy_text(active_app, win->app, sizeof(active_app));
    gui_win_x = win->x;
    gui_win_y = win->y;
    gui_win_w = win->w;
    gui_win_h = win->h;
    gui_restore_x = win->restore_x;
    gui_restore_y = win->restore_y;
    gui_restore_w = win->restore_w;
    gui_restore_h = win->restore_h;
    gui_window_maximized = win->maximized;
}

static void draw_inactive_window(struct gui_app_window* win){
    struct gui_render_context ctx;
    if(!win || app_is_minimized(win->app) || active_is(win->app))
        return;
    gui_capture_render_context(&ctx);
    gui_bind_window_for_render(win);
    draw_active_app_detail();
    gui_inactive_live_renders++;
    fb_fill_rect(win->x + 2, win->y + 2, win->w - 4, 38, 0x394858);
    fb_fill_rect(win->x + 14, win->y + 15, 10, 10, 0xA84A4A);
    fb_fill_rect(win->x + 30, win->y + 15, 10, 10, 0xC8A848);
    fb_fill_rect(win->x + 46, win->y + 15, 10, 10, 0x4A9A68);
    fb_draw_text(win->x + 72, win->y + 24, app_title_for(win->app), 0xFFFFFF);
    fb_fill_rect(win->x + win->w - 42, win->y + 10, 26, 22, 0xA84A4A);
    fb_fill_rect(win->x + win->w - 74, win->y + 10, 26, 22, 0x4A9A68);
    fb_fill_rect(win->x + win->w - 106, win->y + 10, 26, 22, 0xC8A848);
    fb_draw_text(win->x + win->w - 34, win->y + 26, "x", 0xFFFFFF);
    fb_draw_text(win->x + win->w - 66, win->y + 26, win->maximized ? "r" : "+", 0xFFFFFF);
    fb_draw_text(win->x + win->w - 98, win->y + 26, "-", 0xFFFFFF);
    gui_restore_render_context(&ctx);
}

static void draw_visible_windows(void){
    int drawn[16];
    for(uint32_t i=0; i<app_window_count(); i++)
        drawn[i] = 0;
    for(uint32_t i=0; i<window_z_count; i++){
        uint32_t index = window_z_order[i];
        if(index < app_window_count()){
            drawn[index] = 1;
            if(open_apps & app_windows[index].mask)
                draw_inactive_window(&app_windows[index]);
        }
    }
    for(uint32_t i=0; i<app_window_count(); i++)
        if(!drawn[i] && (open_apps & app_windows[i].mask))
            draw_inactive_window(&app_windows[i]);
    draw_active_app_detail();
}

static void draw_active_app_detail(void){
    char num[16];
    char line[72];
    char row[128];
    if(active_is("desktop") || !app_is_visible(active_app)){
        fb_draw_text(204, 74, "Dynamic desktop wallpaper", 0xFFFFFF);
        fb_draw_text(204, 102, "Click an icon or taskbar app to open a window.", 0xFFFFFF);
        return;
    }
    fb_fill_rect(gui_win_x + 10, gui_win_y + 10, gui_win_w, gui_win_h, 0x12202A);
    fb_fill_rect(gui_win_x, gui_win_y, gui_win_w, gui_win_h, 0x8EA0B2);
    fb_fill_rect(gui_win_x + 2, gui_win_y + 2, gui_win_w - 4, gui_win_h - 4, 0xF4F7FA);
    fb_fill_rect(gui_win_x + 2, gui_win_y + 2, gui_win_w - 4, 38, 0x223040);
    fb_fill_rect(gui_win_x + 14, gui_win_y + 15, 10, 10, 0xA84A4A);
    fb_fill_rect(gui_win_x + 30, gui_win_y + 15, 10, 10, 0xC8A848);
    fb_fill_rect(gui_win_x + 46, gui_win_y + 15, 10, 10, 0x4A9A68);
    fb_draw_text(gui_win_x + 72, gui_win_y + 24, app_title(), 0xFFFFFF);
    fb_fill_rect(gui_win_x + gui_win_w - 42, gui_win_y + 10, 26, 22, 0xA84A4A);
    fb_fill_rect(gui_win_x + gui_win_w - 74, gui_win_y + 10, 26, 22, 0x4A9A68);
    fb_fill_rect(gui_win_x + gui_win_w - 106, gui_win_y + 10, 26, 22, 0xC8A848);
    fb_draw_text(gui_win_x + gui_win_w - 34, gui_win_y + 26, "x", 0xFFFFFF);
    fb_draw_text(gui_win_x + gui_win_w - 66, gui_win_y + 26, gui_window_maximized ? "r" : "+", 0xFFFFFF);
    fb_draw_text(gui_win_x + gui_win_w - 98, gui_win_y + 26, "-", 0xFFFFFF);
    app_draw_text(226, 132, "GUI window. Use buttons below, or Open Terminal for command mode.", 0x223040);
    draw_button(812, 124, 130, "Open Terminal", 0x345A7A);
    draw_button(652, 124, 130, "Close Window", 0xA84A4A);
    draw_button(512, 124, 116, "Minimize", 0x887034);
    draw_button(376, 124, 112, gui_window_maximized ? "Restore" : "Maximize", 0x3C704C);
    if(active_is("files")){
        char name[40];
        char row[80];
        int type = 0;
        size_t selected_size = 0;
        char meta[72];
        char num[16];
        draw_app_line(1, "Folder", file_dir);
        draw_app_line(2, "Selected", file_selected);
        draw_button(226, 248, 58, "Up", 0x345A7A);
        draw_button(296, 248, 82, "New File", 0x3C704C);
        draw_button(390, 248, 82, "New Dir", 0x3C704C);
        draw_button(484, 248, 82, "Rename", 0x887034);
        draw_button(578, 248, 82, "Delete", 0xA84A4A);
        draw_button(672, 248, 72, "Copy", 0x4F7088);
        draw_button(756, 248, 72, "Move", 0x386878);
        draw_button(226, 286, 72, "Open", 0x3C704C);
        draw_button(310, 286, 72, "Edit", 0x345A7A);
        draw_button(394, 286, 96, "Terminal", 0x725C9A);
        draw_button(502, 286, 72, "Rusa", 0x5B3C9A);
        if(fs_stat(file_selected, &type, &selected_size) == 0){
            copy_text(meta, type == 1 ? "dir " : "file ", sizeof(meta));
            u32_text((uint32_t)selected_size, num, sizeof(num));
            append_text(meta, "size=", sizeof(meta));
            append_text(meta, num, sizeof(meta));
            append_text(meta, " perms=rw owner=root", sizeof(meta));
            draw_app_line(3, "Metadata", meta);
        }
        app_fill_rect(226, 328, 700, 220, 0xF8FAFC);
        for(uint32_t i=0; i<7; i++){
            if(fs_child_name(file_dir, (int)i, name, sizeof(name), &type) != 0)
                break;
            copy_text(row, type == 1 ? "[dir]  " : "[file] ", sizeof(row));
            append_text(row, name, sizeof(row));
            app_draw_text(250, 358 + i * 24, row, 0x223040);
        }
        app_draw_text(250, 536, "Click selects; double-click opens. Open With buttons choose app.", 0x2E6B4C);
    } else if(active_is("taskman")){
        const struct process_info* selected = process_find(taskman_selected);
        const struct window_info* focused = window_focused();
        draw_app_line(1, "Processes", "pid state priority ticks memory handles window");
        copy_text(line, selected ? selected->name : "none", sizeof(line));
        append_text(line, " -> ", sizeof(line));
        append_text(line, selected ? taskman_app_for_process(selected->name) : "none", sizeof(line));
        append_text(line, "  focused=", sizeof(line));
        append_text(line, focused ? focused->name : "none", sizeof(line));
        draw_app_line(2, "Selected", line);
        draw_button(226, 268, 86, "Focus", 0x345A7A);
        draw_button(326, 268, 92, "Restart", 0x3C704C);
        draw_button(432, 268, 72, "Kill", 0x884C4C);
        draw_button(518, 268, 82, "Boost", 0x725C9A);
        app_fill_rect(226, 318, 700, 150, 0xF8FAFC);
        for(uint32_t i=0; i<process_count() && i<6; i++){
            const struct process_info* proc = process_at(i);
            if(!proc)
                continue;
            copy_text(row, proc->name, sizeof(row));
            append_text(row, proc->running ? " " : " stopped ", sizeof(row));
            append_text(row, process_state_name(proc->state), sizeof(row));
            append_text(row, " ", sizeof(row));
            append_text(row, "p=", sizeof(row));
            u32_text(proc->priority, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " ticks=", sizeof(row));
            u32_text(proc->ticks, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " mem=", sizeof(row));
            u32_text(proc->memory_kib, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, "K h=", sizeof(row));
            u32_text(process_handles_for_pid(proc->pid), num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " win=", sizeof(row));
            append_text(row, taskman_app_for_process(proc->name), sizeof(row));
            append_text(row, " conn=", sizeof(row));
            u32_text(taskman_connection_count(proc->pid), num, sizeof(num));
            append_text(row, num, sizeof(row));
            app_draw_text(246, 344 + i * 20, row, str_eq(proc->name, taskman_selected) ? 0x884C4C : 0x223040);
        }
        app_fill_rect(226, 490, 700, 58, 0x101820);
        for(uint32_t i=0, row_i=0; i<jobs_count() && row_i<2; i++){
            const struct job_info* job = jobs_at(i);
            if(!job || !job->active)
                continue;
            copy_text(row, job->name, sizeof(row));
            append_text(row, " ", sizeof(row));
            append_text(row, job->class_name, sizeof(row));
            append_text(row, " prio=", sizeof(row));
            u32_text(job->priority, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " ticks=", sizeof(row));
            u32_text(job->ticks, num, sizeof(num));
            append_text(row, num, sizeof(row));
            app_draw_text(246, 512 + row_i * 22, row, 0xD8E8FF);
            row_i++;
        }
        draw_app_line(3, "Action", "Open Terminal runs: taskman top");
    } else if(active_is("math")){
        draw_app_line(1, "Workspace", "interactive science notebook surfaces");
        draw_app_line(2, "Tab", math_tab_name());
        draw_mode_button(226, 268, 82, "Vector", math_tab == 0, 0x345A7A);
        draw_mode_button(320, 268, 82, "Matrix", math_tab == 1, 0x3C704C);
        draw_mode_button(414, 268, 82, "Group", math_tab == 2, 0x725C9A);
        draw_mode_button(508, 268, 92, "Physics", math_tab == 3, 0x386878);
        draw_mode_button(612, 268, 82, "LaTeX", math_tab == 4, 0x887034);
        draw_mode_button(706, 268, 72, "Jobs", math_tab == 5, 0x4F7088);
        draw_math_surface();
    } else if(active_is("rusa")){
        draw_app_line(1, "Standalone", "language workbench for .rusa source files");
        draw_app_line(2, "Tab", rusa_tab_name());
        draw_mode_button(226, 268, 86, "Examples", rusa_tab == 0, 0x5B3C9A);
        draw_mode_button(326, 268, 86, "Keywords", rusa_tab == 1, 0x5B3C9A);
        draw_mode_button(426, 268, 72, "Docs", rusa_tab == 2, 0x5B3C9A);
        draw_mode_button(512, 268, 72, "Check", rusa_tab == 3, 0x5B3C9A);
        draw_mode_button(598, 268, 64, "Run", rusa_tab == 4, 0x5B3C9A);
        draw_mode_button(676, 268, 112, "Diagnostics", rusa_tab == 5, 0x5B3C9A);
        draw_mode_button(802, 268, 96, "Packages", rusa_tab == 6, 0x5B3C9A);
        draw_rusa_surface();
    } else if(active_is("privacy")){
        draw_app_line(1, "Settings", "privacy hardware keyboard display input");
        draw_mode_button(226, 236, 92, "Privacy", settings_tab == 0, 0x884C4C);
        draw_mode_button(330, 236, 98, "Hardware", settings_tab == 1, 0x345A7A);
        draw_mode_button(440, 236, 104, "Keyboard", settings_tab == 2, 0x3C704C);
        draw_mode_button(556, 236, 92, "Display", settings_tab == 3, 0x725C9A);
        draw_mode_button(660, 236, 82, "Input", settings_tab == 4, 0x4F7088);
        if(settings_tab == 1){
            copy_text(line, "cpu=i386 fb=", sizeof(line));
            append_text(line, fb_hardware_ready() ? "hardware" : "soft", sizeof(line));
            draw_app_line(2, "CPU/GPU", line);
            copy_text(line, "memory KiB ", sizeof(line));
            u32_text(memory_usable_kib(), num, sizeof(num));
            append_text(line, num, sizeof(line));
            draw_app_line(3, "Memory", line);
            draw_button(226, 312, 112, "CPU Info", 0x345A7A);
            draw_button(356, 312, 112, "GPU Info", 0x386878);
            draw_button(486, 312, 112, "Memory", 0x4F7088);
        } else if(settings_tab == 2){
            copy_text(line, keyboard_type(), sizeof(line));
            draw_app_line(2, "Keyboard", line);
            copy_text(line, "caps ", sizeof(line));
            append_text(line, keyboard_caps_on() ? "on" : "off", sizeof(line));
            append_text(line, " num ", sizeof(line));
            append_text(line, keyboard_num_on() ? "on" : "off", sizeof(line));
            draw_app_line(3, "Locks", line);
            draw_button(226, 312, 96, "Caps", 0x3C704C);
            draw_button(346, 312, 96, "Num", 0x3C704C);
            draw_button(466, 312, 96, "Keys", 0x345A7A);
        } else if(settings_tab == 3){
            copy_text(line, "mode ", sizeof(line));
            u32_text(fb_width(), num, sizeof(num));
            append_text(line, num, sizeof(line));
            append_text(line, "x", sizeof(line));
            u32_text(fb_height(), num, sizeof(num));
            append_text(line, num, sizeof(line));
            append_text(line, " cursor ", sizeof(line));
            append_text(line, fb_cursor_style(), sizeof(line));
            draw_app_line(2, "Display", line);
            draw_button(226, 312, 96, "Dot", 0x345A7A);
            draw_button(346, 312, 96, "Cross", 0x345A7A);
            draw_button(466, 312, 96, "Target", 0x725C9A);
        } else if(settings_tab == 4){
            copy_text(line, "mouse speed ", sizeof(line));
            append_text(line, settings_mouse_speed == 1 ? "slow" : (settings_mouse_speed == 3 ? "fast" : "normal"), sizeof(line));
            append_text(line, "  pointer ", sizeof(line));
            append_text(line, fb_cursor_style(), sizeof(line));
            draw_app_line(2, "Mouse", line);
            copy_text(line, keyboard_type(), sizeof(line));
            append_text(line, "  repeat via keyboard repeat slow|normal|fast", sizeof(line));
            draw_app_line(3, "Keyboard", line);
            draw_button(226, 312, 96, "Slow", 0x4F7088);
            draw_button(346, 312, 96, "Normal", 0x4F7088);
            draw_button(466, 312, 96, "Fast", 0x4F7088);
            draw_button(586, 312, 96, "Mouse", 0x345A7A);
        } else {
            copy_text(line, privacy_master_enabled() ? "master on " : "master off ", sizeof(line));
            append_text(line, privacy_network_enabled() ? "network on " : "network off ", sizeof(line));
            append_text(line, "cookies ", sizeof(line));
            append_text(line, privacy_cookie_policy(), sizeof(line));
            draw_app_line(2, "Master", line);
            copy_text(line, privacy_devices_enabled() ? "devices on " : "devices off ", sizeof(line));
            append_text(line, privacy_telemetry_enabled() ? "telemetry on" : "telemetry off", sizeof(line));
            draw_app_line(3, "Access", line);
            draw_button(226, 312, 96, "Master", 0x884C4C);
            draw_button(346, 312, 96, "Network", 0x345A7A);
            draw_button(466, 312, 96, "Cookies", 0x725C9A);
            draw_button(586, 312, 96, "Devices", 0x3C704C);
            draw_button(706, 312, 96, "Telemetry", 0x4F7088);
        }
    } else if(active_is("network")){
        u32_text(net_packet_count(), num, sizeof(num));
        copy_text(line, net_is_link_up() ? "link up packets=" : "link down packets=", sizeof(line));
        append_text(line, num, sizeof(line));
        draw_app_line(1, "Stack", line);
        copy_text(line, net_shield_enabled() ? "shield on mask " : "shield off mask ", sizeof(line));
        append_text(line, net_ip_masking_enabled() ? "on" : "off", sizeof(line));
        append_text(line, " threshold=", sizeof(line));
        u32_text(net_flood_threshold(), num, sizeof(num));
        append_text(line, num, sizeof(line));
        draw_app_line(2, "Shield", line);
        draw_button(226, 268, 82, "Open", 0x345A7A);
        draw_button(322, 268, 82, "Send", 0x3C704C);
        draw_button(418, 268, 82, "Flush", 0x725C9A);
        draw_button(514, 268, 82, "Shield", 0x884C4C);
        app_fill_rect(226, 318, 700, 160, 0xF8FAFC);
        for(uint32_t i=0; i<net_socket_count() && i<4; i++){
            struct net_socket_info sock;
            if(net_socket_at(i, &sock) != 0 || !sock.used)
                continue;
            copy_text(row, "sock ", sizeof(row));
            u32_text((uint32_t)sock.id, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " ", sizeof(row));
            append_text(row, sock.proto, sizeof(row));
            append_text(row, " ", sizeof(row));
            append_text(row, sock.state, sizeof(row));
            append_text(row, " local=", sizeof(row));
            u32_text(sock.local_port, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " tx=", sizeof(row));
            u32_text(sock.tx_packets, num, sizeof(num));
            append_text(row, num, sizeof(row));
            append_text(row, " rx=", sizeof(row));
            u32_text(sock.rx_packets, num, sizeof(num));
            append_text(row, num, sizeof(row));
            app_draw_text(246, 346 + i * 28, row, 0x223040);
        }
        draw_app_line(3, "Action", "Open Terminal runs: net status");
    } else if(active_is("projects")){
        draw_app_line(1, "Workspace", "/home/projects");
        draw_app_line(2, "Tools", "new run docs edit");
        draw_button(226, 268, 96, "List", 0x345A7A);
        draw_button(346, 268, 116, "New Demo", 0x3C704C);
        draw_button(486, 268, 96, "Run", 0x725C9A);
        draw_app_line(4, "Action", "Open Terminal runs: project list");
    } else if(active_is("packages")){
        draw_app_line(1, "Registry", "core editor math network gui rusa docs");
        draw_app_line(2, "Compat", "pkg compat checks ABI and architecture");
        draw_button(226, 268, 96, "List", 0x345A7A);
        draw_button(346, 268, 96, "Compat", 0x3C704C);
        draw_button(466, 268, 116, "Rusa Docs", 0x725C9A);
        draw_app_line(4, "Action", "Open Terminal runs: pkg list");
    } else if(active_is("logs")){
        draw_app_line(1, "Streams", "system security network");
        draw_app_line(2, "Use", "audit boot net privacy package events");
        draw_button(226, 268, 96, "System", 0x345A7A);
        draw_button(346, 268, 96, "Security", 0x884C4C);
        draw_button(466, 268, 96, "Network", 0x386878);
        draw_app_line(4, "Action", "Open Terminal runs: log show system");
    } else if(active_is("security")){
        draw_mode_button(226, 236, 82, "Overview", security_tab == 0, 0x345A7A);
        draw_mode_button(320, 236, 82, "Conn", security_tab == 1, 0x386878);
        draw_mode_button(414, 236, 82, "Perms", security_tab == 2, 0x725C9A);
        draw_mode_button(508, 236, 92, "Services", security_tab == 3, 0x3C704C);
        draw_mode_button(612, 236, 82, "Scan", security_tab == 4, 0x884C4C);
        draw_mode_button(706, 236, 82, "Devices", security_tab == 5, 0x4F7088);
        draw_mode_button(800, 236, 72, "Events", security_tab == 6, 0x887034);
        draw_button(226, 276, 72, "Lock", 0x3C704C);
        draw_button(312, 276, 86, "Unlock", 0x884C4C);
        draw_button(412, 276, 82, "Ports", 0x386878);
        draw_button(508, 276, 72, "Scan", 0x725C9A);
        draw_button(594, 276, 72, "Audit", 0x4F7088);
        draw_button(680, 276, 96, "Privacy", 0x345A7A);
        app_fill_rect(226, 326, 700, 220, 0xF8FAFC);
        if(security_tab == 1){
            struct net_connection_info conns[4];
            uint32_t n = net_connection_list(conns, 4);
            for(uint32_t i=0; i<n && i<4; i++){
                copy_text(row, "sock=", sizeof(row));
                u32_text(conns[i].socket_id, num, sizeof(num));
                append_text(row, num, sizeof(row));
                append_text(row, " state=", sizeof(row));
                append_text(row, conns[i].state == NET_CONN_CONNECTED ? "connected" :
                                 (conns[i].state == NET_CONN_LISTENING ? "listening" : "other"), sizeof(row));
                append_text(row, " owner=", sizeof(row));
                u32_text(conns[i].owner_pid, num, sizeof(num));
                append_text(row, num, sizeof(row));
                append_text(row, " bytes=", sizeof(row));
                u32_text(conns[i].bytes_tx + conns[i].bytes_rx, num, sizeof(num));
                append_text(row, num, sizeof(row));
                app_draw_text(250, 356 + i * 26, row, 0x223040);
            }
            if(n == 0) app_draw_text(250, 356, "No active connections.", 0x223040);
            draw_button(250, 490, 128, "Disconnect 0", 0x884C4C);
            draw_button(394, 490, 104, "Firewall", 0x386878);
        } else if(security_tab == 2){
            char perms[256];
            security_list_permissions("", perms, sizeof(perms));
            app_draw_text(250, 356, "Permissions: allow, deny, ask, inherited, default-deny", 0x223040);
            app_draw_text(250, 386, perms[0] ? perms : "No permissions registered.", 0x5B3C9A);
            app_draw_text(250, 416, "Use: allow APP RESOURCE or deny APP RESOURCE", 0x223040);
            draw_button(250, 456, 128, "Allow Demo", 0x3C704C);
            draw_button(394, 456, 128, "Deny Demo", 0x884C4C);
        } else if(security_tab == 3){
            struct service_registry_info services[6];
            uint32_t n = service_list_registry(services, 6);
            for(uint32_t i=0; i<n && i<5; i++){
                copy_text(row, services[i].name, sizeof(row));
                append_text(row, " ", sizeof(row));
                append_text(row, services[i].state, sizeof(row));
                append_text(row, " port=", sizeof(row));
                u32_text(services[i].port, num, sizeof(num));
                append_text(row, num, sizeof(row));
                append_text(row, " health=", sizeof(row));
                append_text(row, services[i].health, sizeof(row));
                app_draw_text(250, 356 + i * 26, row, 0x223040);
            }
            draw_button(250, 490, 116, "List", 0x345A7A);
            draw_button(382, 490, 116, "Stop Net", 0x884C4C);
        } else if(security_tab == 4){
            char scan[128];
            security_scan_app("/home/projects/demo.rusa", scan, sizeof(scan));
            app_draw_text(250, 356, "Scanner is lightweight, pattern-based, and not comprehensive.", 0x884C4C);
            app_draw_text(250, 386, scan, 0x223040);
            app_draw_text(250, 416, "Use Security Center to inspect and then decide.", 0x223040);
        } else if(security_tab == 5){
            app_draw_text(250, 356, "Keyboard: permission device.keyboard", 0x223040);
            app_draw_text(250, 386, "Mouse: permission device.mouse", 0x223040);
            app_draw_text(250, 416, "Framebuffer: permission device.framebuffer", 0x223040);
            app_draw_text(250, 446, "Storage: permission device.storage", 0x223040);
        } else if(security_tab == 6){
            struct security_event_info events[4];
            uint32_t n = security_get_events(events, 4);
            for(uint32_t i=0; i<n && i<4; i++){
                copy_text(row, "#", sizeof(row));
                u32_text(events[i].id, num, sizeof(num));
                append_text(row, num, sizeof(row));
                append_text(row, " ", sizeof(row));
                append_text(row, events[i].category, sizeof(row));
                append_text(row, " ", sizeof(row));
                append_text(row, events[i].decision, sizeof(row));
                append_text(row, " ", sizeof(row));
                append_text(row, events[i].app, sizeof(row));
                app_draw_text(250, 356 + i * 26, row, 0x223040);
            }
        } else {
            copy_text(line, security_is_locked() ? "secure mode on user " : "permissive mode user ", sizeof(line));
            append_text(line, security_current_user(), sizeof(line));
            app_draw_text(250, 356, line, 0x223040);
            copy_text(line, privacy_allows_network() ? "network visible cookies " : "network disconnected cookies ", sizeof(line));
            append_text(line, privacy_cookie_policy(), sizeof(line));
            app_draw_text(250, 386, line, 0x223040);
            copy_text(line, net_shield_enabled() ? "shield on mask " : "shield off mask ", sizeof(line));
            append_text(line, net_ip_masking_enabled() ? "on" : "off", sizeof(line));
            append_text(line, " packets=", sizeof(line));
            u32_text(net_packet_count(), num, sizeof(num));
            append_text(line, num, sizeof(line));
            app_draw_text(250, 416, line, 0x223040);
        }
        draw_app_line(4, "Plain", "Every panel uses network/security tables, not decorative labels.");
    } else if(active_is("events")){
        draw_app_line(1, "Rules", "service net fs scheduler Rusa handlers");
        draw_app_line(2, "Model", "persistent event reactions from source files");
        draw_button(226, 268, 96, "List", 0x345A7A);
        draw_button(346, 268, 96, "Emit", 0x3C704C);
        draw_button(466, 268, 116, "Rusa Ev", 0x725C9A);
        draw_app_line(4, "Action", "Open Terminal runs: event list");
    } else if(active_is("storage")){
        draw_app_line(1, "Block", "8-sector RAM disk plus VFS bridges");
        draw_app_line(2, "Handles", "fd open/read/write/close per process");
        draw_button(226, 268, 96, "Block", 0x345A7A);
        draw_button(346, 268, 96, "Mounts", 0x3C704C);
        draw_button(466, 268, 96, "FDs", 0x725C9A);
        draw_app_line(4, "Action", "Open Terminal runs: block status");
    } else if(active_is("saver")){
        draw_app_line(1, "Modes", "lava rain stars waves");
        if(saver_backdrop)
            draw_app_line(2, "Wallpaper", saver_live ? "slow live wallpaper on" : "calm still wallpaper on");
        else
            draw_app_line(2, "Wallpaper", "dynamic wallpaper off");
        copy_text(line, screensaver_hint, sizeof(line));
        append_text(line, screensaver_calm ? " calm preview" : " full preview", sizeof(line));
        draw_app_line(3, "Screensaver", line);
        draw_button(226, 268, 72, "Lava", 0x8A4A40);
        draw_button(318, 268, 72, "Rain", 0x3A86A8);
        draw_button(410, 268, 72, "Stars", 0x604A88);
        draw_button(502, 268, 72, "Waves", 0x386878);
        draw_button(594, 268, 72, "Live", 0x3C704C);
        draw_button(686, 268, 92, "Preview", 0x725C9A);
        draw_button(798, 268, 72, "Off", 0x555A60);
        draw_button(226, 302, 72, "S Lava", 0x8A4A40);
        draw_button(318, 302, 72, "S Rain", 0x3A86A8);
        draw_button(410, 302, 72, "S Stars", 0x604A88);
        draw_button(502, 302, 72, "S Waves", 0x386878);
        draw_button(594, 302, 72, "Calm", 0x4F7088);
        app_fill_rect(226, 328, 700, 220, 0x101820);
        app_draw_text(250, 358, "Wallpaper is calm by default to avoid blinking.", 0xCFE8FF);
        app_draw_text(250, 398, "Top row changes wallpaper; second row changes saver preview.", 0xFFFFFF);
        app_draw_text(250, 438, "Use gui wallpaper live MODE for slow animation.", 0xFFFFFF);
        draw_app_line(4, "Action", "Open Terminal runs: fb saver MODE 12");
    } else if(active_is("editor")){
        draw_app_line(1, "Mode", editor_mode == 1 ? "code workspace" : (editor_mode == 2 ? "math notes" : "paper drafting"));
        draw_app_line(2, "File", editor_path());
        draw_mode_button(226, 236, 92, "Paper", editor_mode == 0, 0x345A7A);
        draw_mode_button(330, 236, 82, "Code", editor_mode == 1, 0x3C704C);
        draw_mode_button(424, 236, 82, "Math", editor_mode == 2, 0x725C9A);
        draw_button(226, 278, 96, "New", 0x345A7A);
        draw_button(346, 278, 96, "Open", 0x3C704C);
        draw_button(466, 278, 96, "Save", 0x887034);
        draw_button(586, 278, 118, "Open File", 0x725C9A);
        draw_button(226, 310, 72, "Select", 0x345A7A);
        draw_button(310, 310, 72, "Copy", 0x3C704C);
        draw_button(394, 310, 72, "Cut", 0xA84A4A);
        draw_button(478, 310, 72, "Paste", 0x725C9A);
        draw_button(562, 310, 72, "Find", 0x887034);
        draw_button(646, 310, 82, "Save As", 0x4F7088);
        draw_editor_surface();
        draw_editor_file_dialog();
    } else if(active_is("inspector")){
        draw_app_line(1, "System", "memory scheduler trace replay logs");
        draw_app_line(2, "Terminal", "Enter opens: inspect memory");
    } else if(active_is("terminal")){
        draw_app_line(1, "Shell", "GUI terminal surface");
        draw_app_line(2, "Last", terminal_view);
        draw_terminal_surface();
    } else {
        draw_app_line(1, "Shell", "object language commands live here");
        draw_app_line(2, "Terminal", "Click terminal or press Enter");
    }
    fb_fill_rect(gui_win_x + gui_win_w - 18, gui_win_y + gui_win_h - 18, 12, 12, 0x8EA0B2);
    fb_fill_rect(gui_win_x + gui_win_w - 14, gui_win_y + gui_win_h - 14, 8, 2, 0x223040);
    fb_fill_rect(gui_win_x + gui_win_w - 10, gui_win_y + gui_win_h - 10, 4, 2, 0x223040);
}

static void gui_draw_desktop_core(int emit_console){
    const struct window_info* focused = window_focused();
    gui_full_repaints++;
    if(saver_backdrop)
        fb_draw_wallpaper(saver_hint, saver_live);
    else
        fb_clear(0x14324A);
    fb_fill_rect(0, 0, 1024, 34, 0x151B22);
    if(!saver_backdrop)
        fb_fill_rect(0, 34, 1024, 686, 0x14324A);
    fb_fill_rect(0, 720, 1024, 48, 0x151B22);
    fb_fill_rect(0, 720, 112, 48, 0x253444);
    fb_draw_text(18, 16, "Tabla Rusa", 0xFFFFFF);
    fb_draw_text(836, 16, "Net", net_is_link_up() ? 0x8EE8A0 : 0xFF8888);
    fb_draw_text(892, 16, privacy_allows_network() ? "Private" : "Offline", 0xFFFFFF);
    draw_desktop_icon(32, 70, "files", "Files", 0xD6A84A);
    draw_desktop_icon(32, 170, "terminal", "Terminal", 0x3C4B58);
    draw_desktop_icon(32, 270, "math", "Math", 0x5B6FC8);
    draw_desktop_icon(32, 370, "rusa", "Rusa", 0x7A5BC8);
    draw_desktop_icon(32, 470, "privacy", "Settings", 0xA84A4A);
    draw_desktop_icon(32, 570, "editor", "Editor", 0x4F7088);
    draw_desktop_icon(882, 70, "network", "Network", 0x3A86A8);
    draw_desktop_icon(882, 170, "saver", "Saver", 0x8A66B8);
    draw_desktop_icon(882, 270, "taskman", "Tasks", 0x4A9A68);
    draw_desktop_icon(882, 370, "projects", "Projects", 0x3C704C);
    draw_desktop_icon(882, 470, "packages", "Packages", 0x725C9A);
    draw_desktop_icon(882, 570, "logs", "Logs", 0x884C4C);
    draw_visible_windows();
    fb_draw_text(26, 746, "Start", 0xFFFFFF);
    draw_dock_item(124, "files", "Files", 0x385C82);
    draw_dock_item(212, "terminal", "Term", 0x304858);
    draw_dock_item(300, "math", "Math", 0x5B6FC8);
    draw_dock_item(388, "rusa", "Rusa", 0x725C9A);
    draw_dock_item(476, "privacy", "Set", 0x884C4C);
    draw_dock_item(564, "editor", "Edit", 0x4F7088);
    draw_dock_item(652, "taskman", "Tasks", 0x3C704C);
    draw_dock_item(740, "network", "Net", 0x386878);
    if(launch_notice[0]){
        fb_fill_rect(742, 726, 270, 32, 0xC87A20);
        fb_draw_text(756, 744, launch_notice, 0xFFFFFF);
    } else {
        fb_draw_text(778, 746, "Click launches app command", 0xFFFFFF);
    }
    draw_launcher_menu();
    fb_set_mouse(mouse_x(), mouse_y(), mouse_buttons());
    if(!emit_console)
        return;
    console_clear_output();
    console_puts("+------------------------------------------------------------------------------+\n");
    console_puts("| Tabla Rusa GUI :: desktop icons + taskbar         tabs: shell editor saver net |\n");
    console_puts("+------------------------------------------------------------------------------+\n");
    console_puts("| focused: ");
    console_puts(focused->name);
    console_puts("  surface=");
    console_puts(focused->surface);
    console_puts("  pos=");
    console_write_dec((uint32_t)focused->x);
    console_putc(',');
    console_write_dec((uint32_t)focused->y);
    console_puts("\n");
    console_puts("|                                                                              |\n");
    console_puts("|  desktop icons: Files Terminal Math Rusa Settings Editor + system tools      |\n");
    console_puts("|  app window: selected icon opens a focused GUI surface                       |\n");
    console_puts("|  taskbar: Start, Files, Term, Math, Rusa, Set, Edit, Tasks, Net              |\n");
    console_puts("|  Click or Enter launches the selected app's shell command                    |\n");
    console_puts("+------------------------------------------------------------------------------+\n");
    if(desktop_mode)
        console_puts("| GUI mode: click icons/taskbar apps, S previews saver, Esc returns here       |\n");
}

static void gui_redraw_active_window(void){
    if(!desktop_mode || active_is("desktop") || !app_is_visible(active_app)){
        gui_draw_desktop_core(0);
        return;
    }
    fb_begin_paint();
    draw_active_app_detail();
    fb_set_mouse(mouse_x(), mouse_y(), mouse_buttons());
    gui_window_repaints++;
}

static void gui_draw_desktop(void){
    gui_draw_desktop_core(1);
}

void gui_init(void){
    fs_mkdir("/system/boot");
    fs_mkdir("/system/gui");
    fs_write("/system/gui/state.txt", "state=ready\nautostart=on\nsurface=desktop\npointer=crosshair\n");
    fs_write("/system/boot/startup.txt",
        "default=gui-desktop\n"
        "recovery=gui boot recovery\n"
        "safe_graphics=gui boot safe\n"
        "logs=gui boot logs\n");
    gui_load_settings();
    editor_seed();
    editor_load_file();
    fs_append_line("/var/log/system.log", "gui: text compositor foundation ready");
}

int gui_is_running(void){
    return running;
}

int gui_is_desktop_visible(void){
    return active_is("desktop") || !app_is_visible(active_app);
}

uint32_t gui_full_repaint_count(void){
    return gui_full_repaints;
}

uint32_t gui_window_repaint_count(void){
    return gui_window_repaints;
}

uint32_t gui_inactive_live_render_count(void){
    return gui_inactive_live_renders;
}

int gui_launcher_is_open(void){
    return launcher_open;
}

void gui_tick(void){
    uint32_t now = timer_ticks();
    if(!running || !desktop_mode || !saver_backdrop || !saver_live)
        return;
    if(gui_busy_until && now < gui_busy_until)
        return;
    if(now == saver_last_tick || (now - saver_last_tick) < GUI_WALLPAPER_TICKS)
        return;
    saver_last_tick = now;
    gui_draw_desktop_core(0);
}

void gui_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        console_puts("gui=");
        console_puts(running ? "running" : "stopped");
        console_puts(" autostart=");
        console_puts(autostarted ? "yes" : "no");
        console_puts(" mode=");
        console_puts(desktop_mode ? "desktop" : "shell");
        console_puts(" app=");
        console_puts(active_app);
        console_puts(" saver=");
        console_puts(saver_hint);
        console_puts(saver_backdrop ? " backdrop=on" : " backdrop=off");
        console_puts(saver_live ? " live=on" : " live=off");
        console_puts(" backend=soft-framebuffer+vga-text\n");
        console_puts("repaint full=");
        console_write_dec(gui_full_repaints);
        console_puts(" window=");
        console_write_dec(gui_window_repaints);
        console_puts(" inactive_live=");
        console_write_dec(gui_inactive_live_renders);
        console_putc('\n');
        console_puts("objects: compositor window-manager tab-strip input-router desktop screensaver\n");
        console_puts("crosshair=");
        console_write_dec(mouse_x());
        console_putc(',');
        console_write_dec(mouse_y());
        console_puts(" buttons=");
        console_write_dec(mouse_buttons());
        console_putc('\n');
    } else if(str_eq(action, "start")){
        if(running && desktop_mode){
            launcher_open = !launcher_open;
            copy_text(launch_notice, launcher_open ? "start menu open" : "start menu closed", sizeof(launch_notice));
            gui_draw_desktop();
        } else {
            running = 1;
            autostarted = 1;
            desktop_mode = 1;
            copy_text(active_app, "desktop", sizeof(active_app));
            editor_focused = 0;
            service_set_running("gui", 1);
            process_set_running("gui", 1);
            fs_write("/system/gui/state.txt", "state=running\nautostart=on\nsurface=desktop\npointer=crosshair\n");
            fs_append_line("/var/log/system.log", "gui: compositor foundation started");
            console_puts("gui: compositor foundation started\n");
            gui_draw_desktop();
        }
    } else if(str_eq(action, "stop")){
        running = 0;
        service_set_running("gui", 0);
        process_set_running("gui", 0);
        fs_write("/system/gui/state.txt", "state=stopped\nautostart=on\nsurface=desktop\npointer=crosshair\n");
        fs_append_line("/var/log/system.log", "gui: compositor foundation stopped");
        console_puts("gui: stopped\n");
    } else if(str_eq(action, "desktop")){
        gui_enter_desktop();
    } else if(str_eq(action, "draw")){
        gui_draw_desktop();
    } else if(str_eq(action, "app")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0){
            console_puts("usage: gui app files|editor|projects|packages|logs|security|events|storage|tasks|math|rusa|settings|privacy|net|saver|terminal\n");
            return;
        }
        name = canonical_app(name);
        gui_focus_app(name);
        gui_draw_desktop();
    } else if(str_eq(action, "terminal") || str_eq(action, "term")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "select")){
            const char* a_arg = first_arg(rest, &rest);
            const char* b_arg = first_arg(rest, &rest);
            uint32_t a = parse_u32(a_arg);
            uint32_t b = b_arg[0] ? parse_u32(b_arg) : a;
            terminal_select_range(a, b);
            copy_text(launch_notice, "terminal lines selected", sizeof(launch_notice));
            console_puts("gui terminal: selected lines\n");
        } else if(str_eq(sub, "copy")){
            terminal_copy_selection();
            copy_text(launch_notice, terminal_clipboard[0] ? "terminal copied" : "terminal copy empty", sizeof(launch_notice));
            console_puts(terminal_clipboard[0] ? "gui terminal: copied\n" : "gui terminal: nothing selected\n");
        } else if(str_eq(sub, "paste")){
            terminal_paste_clipboard();
            terminal_focused = 1;
            copy_text(launch_notice, "terminal pasted", sizeof(launch_notice));
            console_puts("gui terminal: pasted\n");
        } else if(str_eq(sub, "clear")){
            terminal_count = 0;
            terminal_top = 0;
            terminal_selection = 0;
            terminal_clipboard[0] = 0;
            terminal_seed();
            console_puts("gui terminal: cleared\n");
        } else {
            console_puts("usage: gui terminal select A [B] | copy | paste | clear\n");
            return;
        }
        gui_focus_app("terminal");
        gui_draw_desktop();
    } else if(str_eq(action, "cursor") || str_eq(action, "pointer")){
        const char* style = first_arg(rest, &rest);
        if(style[0] == 0){
            console_puts("usage: gui cursor dot|cross|target\n");
            return;
        }
        fb_set_cursor_style(style);
        gui_save_settings();
        gui_focus_app("privacy");
        copy_text(launch_notice, "cursor style changed", sizeof(launch_notice));
        gui_draw_desktop();
    } else if(str_eq(action, "backdrop") || str_eq(action, "wallpaper")){
        const char* name = first_arg(rest, &rest);
        char* tail;
        const char* option;
        int live = 0;
        if(str_eq(name, "off")){
            saver_backdrop = 0;
            saver_live = 0;
            gui_save_settings();
            copy_text(launch_notice, "wallpaper off", sizeof(launch_notice));
        } else {
            if(str_eq(name, "live")){
                live = 1;
                name = first_arg(rest, &rest);
            }
            option = first_arg(rest, &tail);
            if(str_eq(option, "live") || str_eq(option, "animate") || str_eq(option, "animated"))
                live = 1;
            copy_text(saver_hint, name[0] ? name : "lava", sizeof(saver_hint));
            saver_backdrop = 1;
            saver_live = live;
            gui_save_settings();
            copy_text(launch_notice, saver_live ? "slow live wallpaper on" : "calm wallpaper on", sizeof(launch_notice));
        }
        gui_focus_app("saver");
        gui_draw_desktop();
    } else if(str_eq(action, "files")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "up")){
            files_go_up();
        } else if(str_eq(sub, "new") || str_eq(sub, "newfile")){
            const char* name = first_arg(rest, &rest);
            files_new_file(name[0] ? name : "new.txt");
        } else if(str_eq(sub, "mkdir") || str_eq(sub, "newdir")){
            const char* name = first_arg(rest, &rest);
            files_new_folder(name[0] ? name : "folder");
        } else if(str_eq(sub, "rename")){
            const char* name = first_arg(rest, &rest);
            files_rename_selected(name[0] ? name : "renamed.txt");
        } else if(str_eq(sub, "copy") || str_eq(sub, "cp")){
            const char* name = first_arg(rest, &rest);
            files_copy_selected(name);
        } else if(str_eq(sub, "move") || str_eq(sub, "mv")){
            const char* name = first_arg(rest, &rest);
            files_move_selected(name[0] ? name : "moved.txt");
        } else if(str_eq(sub, "delete") || str_eq(sub, "rm")){
            files_delete_selected();
        } else if(str_eq(sub, "select")){
            files_select_index(parse_u32(rest));
            copy_text(launch_notice, "file selected", sizeof(launch_notice));
        } else if(str_eq(sub, "open")){
            const char* target = first_arg(rest, &rest);
            if(str_eq(target, "editor")) files_open_selected_editor();
            else if(str_eq(target, "terminal") || str_eq(target, "term")) files_open_selected_terminal();
            else if(str_eq(target, "rusa")) files_open_selected_rusa();
            else files_open_selected_default();
        } else if(sub[0]){
            console_puts("usage: gui files up|new NAME|mkdir NAME|rename NAME|copy NAME|move NAME|delete|select N|open [editor|terminal|rusa]\n");
            return;
        }
        gui_focus_app("files");
        gui_draw_desktop();
    } else if(str_eq(action, "editor")){
        const char* mode = first_arg(rest, &rest);
        if(str_eq(mode, "paper") || str_eq(mode, "draft")){
            editor_set_mode(0);
            copy_text(launch_notice, "editor paper mode", sizeof(launch_notice));
        } else if(str_eq(mode, "code")){
            editor_set_mode(1);
            copy_text(launch_notice, "editor code mode", sizeof(launch_notice));
        } else if(str_eq(mode, "math") || str_eq(mode, "notes")){
            editor_set_mode(2);
            copy_text(launch_notice, "editor math notes", sizeof(launch_notice));
        } else if(str_eq(mode, "open")){
            const char* path = first_arg(rest, &rest);
            if(path[0]){
                editor_open_path(path);
                copy_text(launch_notice, "editor file opened", sizeof(launch_notice));
            } else {
                editor_dialog_open(1);
            }
        } else if(str_eq(mode, "save")){
            const char* path = first_arg(rest, &rest);
            if(path[0])
                copy_text(editor_current_path, path, sizeof(editor_current_path));
            editor_save_file();
            gui_save_settings();
            copy_text(launch_notice, "editor file saved", sizeof(launch_notice));
        } else if(str_eq(mode, "saveas")){
            const char* path = first_arg(rest, &rest);
            if(path[0]){
                copy_text(editor_current_path, path, sizeof(editor_current_path));
                editor_save_file();
                gui_save_settings();
                copy_text(launch_notice, "editor saved as", sizeof(launch_notice));
            } else {
                editor_dialog_open(2);
            }
        } else if(str_eq(mode, "openas")){
            const char* path = first_arg(rest, &rest);
            editor_open_path(path[0] ? path : editor_current_path);
            gui_save_settings();
            copy_text(launch_notice, "editor open dialog path", sizeof(launch_notice));
        } else if(str_eq(mode, "dialog")){
            const char* sub = first_arg(rest, &rest);
            if(str_eq(sub, "open")) editor_dialog_open(1);
            else if(str_eq(sub, "save") || str_eq(sub, "saveas")) editor_dialog_open(2);
            else if(str_eq(sub, "up")) editor_dialog_up();
            else if(str_eq(sub, "select")) editor_dialog_select_index(parse_u32(rest));
            else if(str_eq(sub, "confirm")) editor_dialog_confirm();
            else if(str_eq(sub, "txt")) editor_dialog_save_quick(0);
            else if(str_eq(sub, "rusa")) editor_dialog_save_quick(1);
            else if(str_eq(sub, "cancel")) editor_dialog_cancel();
            else {
                console_puts("usage: gui editor dialog open|save|up|select N|confirm|txt|rusa|cancel\n");
                return;
            }
        } else if(str_eq(mode, "new")){
            const char* path = first_arg(rest, &rest);
            if(path[0]){
                copy_text(editor_current_path, path, sizeof(editor_current_path));
                if(text_has(editor_current_path, ".rusa")) editor_mode = 1;
                else if(text_has(editor_current_path, ".md") || text_has(editor_current_path, "/home/math")) editor_mode = 2;
                else editor_mode = 0;
            }
            editor_seed();
            editor_dirty = 1;
            copy_text(launch_notice, "new editor file", sizeof(launch_notice));
        } else if(str_eq(mode, "copy")){
            editor_copy_range();
            copy_text(launch_notice, "line copied", sizeof(launch_notice));
        } else if(str_eq(mode, "cut")){
            editor_copy_range();
            editor_delete_range();
            copy_text(launch_notice, "selection cut", sizeof(launch_notice));
        } else if(str_eq(mode, "paste")){
            editor_paste_range();
            copy_text(launch_notice, "clipboard pasted", sizeof(launch_notice));
        } else if(str_eq(mode, "select")){
            uint32_t a = parse_u32(first_arg(rest, &rest));
            uint32_t b = parse_u32(first_arg(rest, &rest));
            if(a > 0) a--;
            if(b > 0) b--;
            editor_select_range(a, b);
            copy_text(launch_notice, "lines selected", sizeof(launch_notice));
        } else if(str_eq(mode, "find")){
            const char* needle = first_arg(rest, &rest);
            copy_text(launch_notice, editor_find(needle) >= 0 ? "find matched" : "find missed", sizeof(launch_notice));
        } else if(mode[0]){
            console_puts("usage: gui editor paper|code|math|new [PATH]|open [PATH]|openas PATH|save [PATH]|saveas PATH|dialog ACTION|select A B|copy|cut|paste|find TEXT\n");
            return;
        }
        gui_save_settings();
        gui_focus_app("editor");
        gui_draw_desktop();
    } else if(str_eq(action, "rusa")){
        const char* tab = first_arg(rest, &rest);
        if(str_eq(tab, "examples")) rusa_tab = 0;
        else if(str_eq(tab, "keywords")) rusa_tab = 1;
        else if(str_eq(tab, "docs")) rusa_tab = 2;
        else if(str_eq(tab, "check")){
            rusa_tab = 3;
            rusa_workbench_action(0);
        }
        else if(str_eq(tab, "run")){
            rusa_tab = 4;
            rusa_workbench_action(1);
        }
        else if(str_eq(tab, "diagnostics") || str_eq(tab, "errors")) rusa_tab = 5;
        else if(str_eq(tab, "packages") || str_eq(tab, "imports")) rusa_tab = 6;
        else if(tab[0]){
            console_puts("usage: gui rusa examples|keywords|docs|check|run|diagnostics|packages\n");
            return;
        }
        copy_text(launch_notice, "rusa tab changed", sizeof(launch_notice));
        gui_focus_app("rusa");
        gui_draw_desktop();
    } else if(str_eq(action, "math")){
        const char* tab = first_arg(rest, &rest);
        if(str_eq(tab, "vector") || str_eq(tab, "vec")) math_workbench_select(0);
        else if(str_eq(tab, "matrix") || str_eq(tab, "mat")) math_workbench_select(1);
        else if(str_eq(tab, "group")) math_workbench_select(2);
        else if(str_eq(tab, "physics") || str_eq(tab, "phys")) math_workbench_select(3);
        else if(str_eq(tab, "latex") || str_eq(tab, "tex")) math_workbench_select(4);
        else if(str_eq(tab, "jobs") || str_eq(tab, "compute")) math_workbench_select(5);
        else if(tab[0]){
            console_puts("usage: gui math vector|matrix|group|physics|latex|jobs\n");
            return;
        }
        copy_text(launch_notice, "math tab changed", sizeof(launch_notice));
        gui_focus_app("math");
        gui_draw_desktop();
    } else if(str_eq(action, "taskman") || str_eq(action, "tasks")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "select")){
            taskman_select_process(first_arg(rest, &rest));
            copy_text(launch_notice, "task selected", sizeof(launch_notice));
        } else if(str_eq(sub, "kill")){
            const char* name = first_arg(rest, &rest);
            if(!name[0]) name = taskman_selected;
            if(process_stop(name) == 0){
                taskman_select_process(name);
                copy_text(launch_notice, "process stopped", sizeof(launch_notice));
            } else {
                copy_text(launch_notice, "protected process", sizeof(launch_notice));
            }
        } else if(str_eq(sub, "restart")){
            const char* name = first_arg(rest, &rest);
            if(!name[0]) name = taskman_selected;
            if(process_find(name)){
                process_set_running(name, 1);
                taskman_select_process(name);
                copy_text(launch_notice, "process restarted", sizeof(launch_notice));
            }
        } else if(str_eq(sub, "focus")){
            const char* name = first_arg(rest, &rest);
            if(!name[0]) name = taskman_selected;
            taskman_select_process(name);
            gui_open_app(taskman_app_for_process(taskman_selected));
            return;
        } else if(str_eq(sub, "boost")){
            taskman_boost_compute();
            copy_text(launch_notice, "compute boosted", sizeof(launch_notice));
        } else if(sub[0]){
            console_puts("usage: gui taskman select|kill|restart|focus NAME | boost\n");
            return;
        }
        gui_focus_app("taskman");
        gui_draw_desktop();
    } else if(str_eq(action, "security") || str_eq(action, "sec")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "overview")) security_tab = 0;
        else if(str_eq(sub, "connections") || str_eq(sub, "conn")) security_tab = 1;
        else if(str_eq(sub, "permissions") || str_eq(sub, "perms")) security_tab = 2;
        else if(str_eq(sub, "services")) security_tab = 3;
        else if(str_eq(sub, "scanner") || str_eq(sub, "scan-panel")) security_tab = 4;
        else if(str_eq(sub, "devices")) security_tab = 5;
        else if(str_eq(sub, "events")) security_tab = 6;
        else if(str_eq(sub, "lock")){
            char cmd[] = "lock";
            security_cmd(cmd);
            copy_text(launch_notice, "secure mode on", sizeof(launch_notice));
        } else if(str_eq(sub, "unlock")){
            char cmd[] = "unlock";
            security_cmd(cmd);
            copy_text(launch_notice, "permissive mode", sizeof(launch_notice));
        } else if(str_eq(sub, "ports")){
            request_terminal_command("privacy ports", "ports and connections");
            return;
        } else if(str_eq(sub, "scan")){
            request_terminal_command("lang scan /home/projects/demo.rusa", "malicious-code scan");
            return;
        } else if(str_eq(sub, "audit")){
            request_terminal_command("security audit", "security audit");
            return;
        } else if(str_eq(sub, "privacy")){
            request_terminal_command("privacy status", "privacy center");
            return;
        } else if(str_eq(sub, "disconnect")){
            const char* id_text = first_arg(rest, &rest);
            uint32_t id = id_text[0] ? parse_u32(id_text) : 0;
            if(net_socket_close(id) == 0)
                copy_text(launch_notice, "connection disconnected", sizeof(launch_notice));
            else
                copy_text(launch_notice, "connection not found", sizeof(launch_notice));
            security_tab = 1;
        } else if(str_eq(sub, "allow") || str_eq(sub, "deny") || str_eq(sub, "ask")){
            const char* app = first_arg(rest, &rest);
            const char* resource = first_arg(rest, &rest);
            enum security_permission_decision decision = SECURITY_PERMISSION_ASK;
            if(str_eq(sub, "allow")) decision = SECURITY_PERMISSION_ALLOW;
            else if(str_eq(sub, "deny")) decision = SECURITY_PERMISSION_DENY;
            if(!app[0] || !resource[0]){
                console_puts("usage: gui security allow|deny|ask APP RESOURCE\n");
                return;
            }
            security_set_permission(app, resource, decision);
            copy_text(launch_notice, "permission updated", sizeof(launch_notice));
            security_tab = 2;
        } else if(str_eq(sub, "firewall")){
            request_terminal_command("firewall list", "firewall rules");
            return;
        } else if(str_eq(sub, "stop-net")){
            request_terminal_command("service stop network", "stop network service");
            return;
        } else if(sub[0]){
            console_puts("usage: gui security lock|unlock|ports|scan|audit|privacy|disconnect|allow|deny|firewall\n");
            return;
        }
        gui_focus_app("security");
        gui_draw_desktop();
    } else if(str_eq(action, "network") || str_eq(action, "net")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "open")){
            char cmd[] = "open udp 9999";
            net_cmd(cmd);
            copy_text(launch_notice, "socket opened", sizeof(launch_notice));
        } else if(str_eq(sub, "send")){
            char cmd[] = "send 0 gui-ping";
            net_cmd(cmd);
            copy_text(launch_notice, "loopback packet sent", sizeof(launch_notice));
        } else if(str_eq(sub, "flush")){
            char cmd[] = "flush";
            net_cmd(cmd);
            copy_text(launch_notice, "packet queue flushed", sizeof(launch_notice));
        } else if(str_eq(sub, "shield")){
            char cmd[16];
            copy_text(cmd, net_shield_enabled() ? "shield off" : "shield on", sizeof(cmd));
            net_cmd(cmd);
            copy_text(launch_notice, "network shield toggled", sizeof(launch_notice));
        } else if(sub[0]){
            console_puts("usage: gui network open|send|flush|shield\n");
            return;
        }
        gui_focus_app("network");
        gui_draw_desktop();
    } else if(str_eq(action, "boot")){
        const char* sub = first_arg(rest, &rest);
        if(str_eq(sub, "safe")){
            saver_backdrop = 0;
            saver_live = 0;
            fb_set_cursor_style("dot");
            fs_write("/system/gui/safe-mode.txt", "safe_graphics=on\nwallpaper=off\ncursor=dot\n");
            gui_save_settings();
            copy_text(launch_notice, "safe graphics mode", sizeof(launch_notice));
            gui_focus_app("logs");
            gui_draw_desktop();
        } else if(str_eq(sub, "recovery")){
            fs_write("/system/boot/recovery.txt", "mode=terminal-requested\nreturn=gui start\n");
            request_terminal_command("log show system", "recovery terminal");
            return;
        } else if(str_eq(sub, "logs") || str_eq(sub, "startup")){
            request_terminal_command("log show system", "startup logs");
            return;
        } else if(sub[0]){
            console_puts("usage: gui boot safe|recovery|logs\n");
            return;
        } else {
            gui_focus_app("logs");
            gui_draw_desktop();
        }
    } else if(str_eq(action, "settings")){
        const char* tab = first_arg(rest, &rest);
        launch_notice[0] = 0;
        if(str_eq(tab, "privacy")) settings_tab = 0;
        else if(str_eq(tab, "hardware")) settings_tab = 1;
        else if(str_eq(tab, "keyboard")) settings_tab = 2;
        else if(str_eq(tab, "display") || str_eq(tab, "gpu")) settings_tab = 3;
        else if(str_eq(tab, "input") || str_eq(tab, "mouse")) settings_tab = 4;
        else if(str_eq(tab, "master")){
            privacy_set_master(!privacy_master_enabled());
            settings_tab = 0;
            copy_text(launch_notice, privacy_master_enabled() ? "master privacy on" : "master privacy off", sizeof(launch_notice));
        } else if(str_eq(tab, "network")){
            privacy_set_network(!privacy_network_enabled());
            settings_tab = 0;
            copy_text(launch_notice, privacy_network_enabled() ? "network allowed" : "network disconnected", sizeof(launch_notice));
        } else if(str_eq(tab, "cookies")){
            const char* policy = privacy_cookie_policy();
            privacy_set_cookie_policy(str_eq(policy, "ask") ? "block" : (str_eq(policy, "block") ? "allow" : "ask"));
            settings_tab = 0;
            copy_text(launch_notice, "cookie policy changed", sizeof(launch_notice));
        } else if(str_eq(tab, "devices")){
            privacy_set_devices(!privacy_devices_enabled());
            settings_tab = 0;
            copy_text(launch_notice, privacy_devices_enabled() ? "devices allowed" : "devices blocked", sizeof(launch_notice));
        } else if(str_eq(tab, "telemetry")){
            privacy_set_telemetry(!privacy_telemetry_enabled());
            settings_tab = 0;
            copy_text(launch_notice, privacy_telemetry_enabled() ? "telemetry allowed" : "telemetry off", sizeof(launch_notice));
        }
        else if(tab[0]){
            console_puts("usage: gui settings privacy|hardware|keyboard|display|input|master|network|cookies|devices|telemetry\n");
            return;
        }
        if(!launch_notice[0])
            copy_text(launch_notice, "settings tab changed", sizeof(launch_notice));
        gui_focus_app("privacy");
        gui_draw_desktop();
    } else if(str_eq(action, "close")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0) name = active_app;
        name = canonical_app(name);
        app_close(name);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
        gui_draw_desktop();
    } else if(str_eq(action, "minimize")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0 || str_eq(name, "active")) name = active_app;
        name = canonical_app(name);
        app_minimize(name);
        copy_text(launch_notice, "window minimized", sizeof(launch_notice));
        gui_draw_desktop();
    } else if(str_eq(action, "restore")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0 || str_eq(name, "active")) name = active_app;
        name = canonical_app(name);
        app_restore(name);
        copy_text(launch_notice, "window restored", sizeof(launch_notice));
        gui_draw_desktop();
    } else if(str_eq(action, "maximize")){
        const char* name = first_arg(rest, &rest);
        if(name[0] && !str_eq(name, "active")){
            name = canonical_app(name);
            gui_focus_app(name);
        }
        window_toggle_maximize();
        gui_draw_desktop();
    } else if(str_eq(action, "saver")){
        const char* name = first_arg(rest, &rest);
        const char* mode = first_arg(rest, &rest);
        gui_focus_app("saver");
        if(str_eq(name, "calm")){
            if(str_eq(mode, "off") || str_eq(mode, "no")) screensaver_calm = 0;
            else screensaver_calm = 1;
            gui_save_settings();
            copy_text(launch_notice, screensaver_calm ? "calm screensaver on" : "full screensaver on", sizeof(launch_notice));
            gui_draw_desktop();
        } else if(str_eq(mode, "calm") || str_eq(mode, "full") || str_eq(mode, "select") || str_eq(mode, "set")){
            copy_text(screensaver_hint, name[0] ? name : screensaver_hint, sizeof(screensaver_hint));
            if(str_eq(mode, "full")) screensaver_calm = 0;
            else if(str_eq(mode, "calm")) screensaver_calm = 1;
            gui_save_settings();
            copy_text(launch_notice, screensaver_calm ? "calm screensaver ready" : "full screensaver ready", sizeof(launch_notice));
            gui_draw_desktop();
        } else if(str_eq(mode, "backdrop") || str_eq(mode, "live") || str_eq(mode, "wallpaper")){
            copy_text(saver_hint, name[0] ? name : screensaver_hint, sizeof(saver_hint));
            saver_backdrop = 1;
            saver_live = str_eq(mode, "live");
            copy_text(launch_notice, saver_live ? "slow live wallpaper" : "calm wallpaper", sizeof(launch_notice));
            gui_save_settings();
            gui_draw_desktop();
        } else {
            if(!str_eq(name, "preview") && name[0])
                copy_text(screensaver_hint, name, sizeof(screensaver_hint));
            gui_save_settings();
            copy_text(launch_notice, "screensaver preview", sizeof(launch_notice));
            gui_preview_screensaver();
        }
    } else if(str_eq(action, "windows") || str_eq(action, "tabs")){
        window_list();
        gui_window_list();
    } else if(str_eq(action, "tab") || str_eq(action, "next")){
        window_focus_next();
        gui_draw_desktop();
    } else if(str_eq(action, "focus")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0){
            console_puts("usage: gui focus WINDOW\n");
            return;
        }
        if(app_mask(name))
            gui_focus_app(name);
        else
            window_focus(name);
        gui_draw_desktop();
    } else if(str_eq(action, "move")){
        char* yarg;
        const char* name = first_arg(rest, &rest);
        uint32_t x = parse_u32(rest);
        first_arg(rest, &yarg);
        uint32_t y = parse_u32(yarg);
        if(name[0] == 0){
            console_puts("usage: gui move WINDOW|active X Y\n");
            return;
        }
        if(str_eq(name, "active")){
            gui_win_x = x;
            gui_win_y = y;
            clamp_window_geometry();
        }
        window_move(name, (int)x, (int)y);
        gui_draw_desktop();
    } else if(str_eq(action, "resize")){
        const char* name = first_arg(rest, &rest);
        uint32_t w = parse_u32(first_arg(rest, &rest));
        uint32_t h = parse_u32(first_arg(rest, &rest));
        if(w < 520) w = 520;
        if(h < 380) h = 380;
        if(w > 900) w = 900;
        if(h > 640) h = 640;
        if(name[0] == 0 || str_eq(name, "active")){
            gui_win_w = w;
            gui_win_h = h;
            clamp_window_geometry();
        } else {
            window_resize(name, (int)w, (int)h);
        }
        gui_draw_desktop();
    } else if(str_eq(action, "click")){
        uint32_t x = parse_u32(first_arg(rest, &rest));
        uint32_t y = parse_u32(first_arg(rest, &rest));
        mouse_set((int)x, (int)y);
        mouse_button(0, 1);
        mouse_button(0, 0);
        gui_draw_desktop();
    } else {
        console_puts("usage: gui status | start | stop | desktop | draw | boot safe|recovery|logs | app NAME | editor MODE | rusa TAB | math TAB | wallpaper MODE|off | saver NAME [preview|calm|full|live] | windows | tab | focus NAME | move NAME X Y | resize active W H | minimize|restore|maximize [APP] | click X Y\n");
    }
}

void gui_enter_desktop(void){
    running = 1;
    autostarted = 1;
    desktop_mode = 1;
    copy_text(active_app, "desktop", sizeof(active_app));
    editor_focused = 0;
    launch_notice[0] = 0;
    service_set_running("gui", 1);
    process_set_running("gui", 1);
    fs_write("/system/gui/state.txt", "state=running\nautostart=on\nmode=desktop\nsurface=desktop\npointer=crosshair\nterminal=hidden\n");
    gui_draw_desktop();
    console_input_write("GUI desktop - Enter opens terminal");
}

static void editor_clamp_cursor(void){
    uint32_t len = text_len32(editor_buf[editor_line]);
    if(editor_line >= GUI_EDITOR_MAX_LINES) editor_line = GUI_EDITOR_MAX_LINES - 1;
    if(editor_col > len) editor_col = len;
    editor_ensure_visible();
}

static void editor_insert_char(char c){
    char* line = editor_buf[editor_line];
    uint32_t len = text_len32(line);
    if(len + 1 >= GUI_EDITOR_COLS) return;
    for(uint32_t i=len + 1; i>editor_col; i--)
        line[i] = line[i - 1];
    line[editor_col++] = c;
    editor_dirty = 1;
}

static void editor_backspace(void){
    char* line = editor_buf[editor_line];
    uint32_t len = text_len32(line);
    if(editor_col > 0){
        for(uint32_t i=editor_col - 1; i<len; i++)
            line[i] = line[i + 1];
        editor_col--;
        editor_dirty = 1;
    } else if(editor_line > 0){
        uint32_t prev_len = text_len32(editor_buf[editor_line - 1]);
        uint32_t room = GUI_EDITOR_COLS - prev_len - 1;
        uint32_t take = len < room ? len : room;
        for(uint32_t i=0; i<take; i++)
            editor_buf[editor_line - 1][prev_len + i] = line[i];
        editor_buf[editor_line - 1][prev_len + take] = 0;
        for(uint32_t row=editor_line; row + 1 < GUI_EDITOR_MAX_LINES; row++)
            copy_text(editor_buf[row], editor_buf[row + 1], sizeof(editor_buf[row]));
        editor_buf[GUI_EDITOR_MAX_LINES - 1][0] = 0;
        editor_line--;
        editor_col = prev_len;
        editor_dirty = 1;
    }
}

static void editor_delete(void){
    char* line = editor_buf[editor_line];
    uint32_t len = text_len32(line);
    if(editor_col < len){
        for(uint32_t i=editor_col; i<len; i++)
            line[i] = line[i + 1];
        editor_dirty = 1;
    } else if(editor_line + 1 < GUI_EDITOR_MAX_LINES && editor_buf[editor_line + 1][0]){
        uint32_t room = GUI_EDITOR_COLS - len - 1;
        uint32_t take = text_len32(editor_buf[editor_line + 1]);
        if(take > room) take = room;
        for(uint32_t i=0; i<take; i++)
            line[len + i] = editor_buf[editor_line + 1][i];
        line[len + take] = 0;
        for(uint32_t row=editor_line + 1; row + 1 < GUI_EDITOR_MAX_LINES; row++)
            copy_text(editor_buf[row], editor_buf[row + 1], sizeof(editor_buf[row]));
        editor_buf[GUI_EDITOR_MAX_LINES - 1][0] = 0;
        editor_dirty = 1;
    }
}

static void editor_newline(void){
    char tail[GUI_EDITOR_COLS];
    char* line = editor_buf[editor_line];
    uint32_t len = text_len32(line);
    if(editor_line + 1 >= GUI_EDITOR_MAX_LINES) return;
    copy_text(tail, line + editor_col, sizeof(tail));
    line[editor_col] = 0;
    for(uint32_t row=GUI_EDITOR_MAX_LINES - 1; row>editor_line + 1; row--)
        copy_text(editor_buf[row], editor_buf[row - 1], sizeof(editor_buf[row]));
    copy_text(editor_buf[editor_line + 1], tail, sizeof(editor_buf[editor_line + 1]));
    editor_line++;
    editor_col = 0;
    (void)len;
    editor_dirty = 1;
}

static int editor_handle_key(int key){
    if(!(active_is("editor") && app_is_open("editor") && editor_focused)) return 0;
    gui_note_activity();
    if(keyboard_ctrl_down() && (key == 'c' || key == 'C')){
        editor_copy_range();
        copy_text(launch_notice, "line copied", sizeof(launch_notice));
        gui_redraw_active_window();
        return 1;
    }
    if(keyboard_ctrl_down() && (key == 'x' || key == 'X')){
        editor_copy_range();
        editor_delete_range();
        copy_text(launch_notice, "selection cut", sizeof(launch_notice));
        gui_redraw_active_window();
        return 1;
    }
    if(keyboard_ctrl_down() && (key == 'v' || key == 'V')){
        editor_paste_range();
        copy_text(launch_notice, "line pasted", sizeof(launch_notice));
        gui_redraw_active_window();
        return 1;
    }
    if(key == KB_KEY_LEFT){
        if(editor_col > 0) editor_col--;
        else if(editor_line > 0){
            editor_line--;
            editor_col = text_len32(editor_buf[editor_line]);
        }
    } else if(key == KB_KEY_RIGHT){
        uint32_t len = text_len32(editor_buf[editor_line]);
        if(editor_col < len) editor_col++;
        else if(editor_line + 1 < GUI_EDITOR_MAX_LINES){
            editor_line++;
            editor_col = 0;
        }
    } else if(key == KB_KEY_UP){
        if(editor_line > 0) editor_line--;
        editor_clamp_cursor();
    } else if(key == KB_KEY_DOWN){
        if(editor_line + 1 < GUI_EDITOR_MAX_LINES) editor_line++;
        editor_clamp_cursor();
    } else if(key == KB_KEY_PAGE_UP){
        editor_scroll(GUI_EDITOR_VISIBLE_LINES);
    } else if(key == KB_KEY_PAGE_DOWN){
        editor_scroll(-((int)GUI_EDITOR_VISIBLE_LINES));
    } else if(key == KB_KEY_HOME){
        editor_col = 0;
    } else if(key == KB_KEY_END){
        editor_col = text_len32(editor_buf[editor_line]);
    } else if(key == KB_KEY_DELETE){
        editor_delete();
    } else if(key == '\n'){
        editor_newline();
    } else if(key == 8 || key == 127){
        editor_backspace();
    } else if(key >= 32 && key <= 126){
        editor_clear_selection();
        editor_insert_char((char)key);
    } else {
        return 0;
    }
    editor_clamp_cursor();
    gui_redraw_active_window();
    console_input_write("Tabla Editor - typing in GUI document");
    return 1;
}

static int terminal_handle_key(int key){
    uint32_t len;
    if(!(active_is("terminal") && app_is_open("terminal") && terminal_focused))
        return 0;
    gui_note_activity();
    len = text_len32(terminal_input);
    if(key == '\n'){
        terminal_run_input();
    } else if(key == 8 || key == 127){
        terminal_backspace();
    } else if(key == KB_KEY_DELETE){
        terminal_delete_char();
    } else if(key == KB_KEY_LEFT){
        if(terminal_cursor > 0)
            terminal_cursor--;
    } else if(key == KB_KEY_RIGHT){
        if(terminal_cursor < len)
            terminal_cursor++;
    } else if(key == KB_KEY_HOME){
        terminal_cursor = 0;
    } else if(key == KB_KEY_END){
        terminal_cursor = len;
    } else if(key == KB_KEY_UP){
        terminal_history_prev_local();
    } else if(key == KB_KEY_DOWN){
        terminal_history_next_local();
    } else if(key == KB_KEY_PAGE_UP){
        terminal_top = terminal_top > 0 ? terminal_top - 1 : 0;
    } else if(key == KB_KEY_PAGE_DOWN){
        if(terminal_top + 8 < terminal_count)
            terminal_top++;
    } else if(key >= 32 && key <= 126){
        terminal_insert_char((char)key);
    } else {
        return 0;
    }
    gui_redraw_active_window();
    console_input_write("GUI Terminal - type commands in the window");
    return 1;
}

int gui_key_captures(int key){
    if(active_is("terminal") && app_is_open("terminal") && terminal_focused)
        return key == '\n' || key == 8 || key == 127 ||
               key == KB_KEY_LEFT || key == KB_KEY_RIGHT || key == KB_KEY_UP || key == KB_KEY_DOWN ||
               key == KB_KEY_HOME || key == KB_KEY_END || key == KB_KEY_DELETE ||
               key == KB_KEY_PAGE_UP || key == KB_KEY_PAGE_DOWN ||
               (key >= 32 && key <= 126);
    if(active_is("editor") && app_is_open("editor") && editor_focused)
        return key == '\n' || key == 8 || key == 127 ||
               key == KB_KEY_LEFT || key == KB_KEY_RIGHT || key == KB_KEY_UP || key == KB_KEY_DOWN ||
               key == KB_KEY_PAGE_UP || key == KB_KEY_PAGE_DOWN ||
               key == KB_KEY_HOME || key == KB_KEY_END || key == KB_KEY_DELETE ||
               (key >= 32 && key <= 126);
    return 0;
}

void gui_handle_key(int key){
    gui_note_activity();
    if((key == '\t' && keyboard_alt_down())){
        launcher_open = 0;
        gui_focus_next_open_app();
        gui_draw_desktop();
        console_input_write("GUI desktop - Alt+Tab cycled windows");
        return;
    }
    if((key == 'q' || key == 'Q') && keyboard_ctrl_down()){
        launcher_open = 0;
        app_close(active_app);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
        gui_draw_desktop();
        console_input_write("GUI desktop - Ctrl+Q closed window");
        return;
    }
    if(key == KB_KEY_SUPER_LEFT || key == KB_KEY_SUPER_RIGHT || key == KB_KEY_F1){
        launcher_open = !launcher_open;
        copy_text(launch_notice, launcher_open ? "start menu open" : "start menu closed", sizeof(launch_notice));
        gui_draw_desktop();
        console_input_write("GUI desktop - Start menu");
        return;
    }
    if(terminal_handle_key(key))
        return;
    if(editor_handle_key(key))
        return;
    if(key == '\t' || key == KB_KEY_RIGHT || key == KB_KEY_DOWN){
        window_focus_next();
        const struct window_info* focused = window_focused();
        if(str_eq(focused->name, "shell")) gui_focus_app("terminal");
        else if(str_eq(focused->name, "inspector")) gui_focus_app("inspector");
        else if(str_eq(focused->name, "editor")) gui_focus_app("editor");
        else gui_focus_app("network");
    } else if(key == KB_KEY_LEFT || key == KB_KEY_UP){
        window_focus_next();
    } else if(key == '1'){
        gui_focus_app("terminal");
    } else if(key == '2'){
        gui_focus_app("inspector");
    } else if(key == '3'){
        gui_focus_app("editor");
    } else if(key == '4'){
        gui_focus_app("network");
    } else if(key == '5'){
        gui_focus_app("files");
    } else if(key == '6'){
        gui_focus_app("math");
    } else if(key == '7'){
        gui_focus_app("privacy");
    } else if(key == '8'){
        gui_focus_app("taskman");
    } else if(key == 's' || key == 'S'){
        gui_focus_app("saver");
        gui_preview_screensaver();
        console_input_write("Screensaver preview - Esc returns to GUI desktop");
        return;
    }
    gui_draw_desktop();
    console_input_write("GUI desktop - Enter opens terminal");
}

int gui_handle_scroll(int amount){
    uint32_t sx = design_x_from_screen(mouse_x());
    uint32_t sy = design_y_from_screen(mouse_y());
    if(desktop_mode && active_is("terminal") && app_is_open("terminal")){
        if(sx >= 226 && sx < 926 && sy >= 208 && sy < 558){
            if(amount > 0)
                terminal_top = terminal_top > 0 ? terminal_top - 1 : 0;
            else if(terminal_top + 8 < terminal_count)
                terminal_top++;
            gui_redraw_active_window();
            return 1;
        }
    }
    if(!(desktop_mode && active_is("editor") && app_is_open("editor")))
        return 0;
    if(sx < 388 || sx >= 926 || sy < 346 || sy >= 566)
        return 0;
    gui_note_activity();
    editor_focused = 1;
    editor_scroll(amount > 0 ? 3 : -3);
    gui_redraw_active_window();
    console_input_write("Tabla Editor - scrolled document");
    return 1;
}

void gui_handle_drag(uint32_t x, uint32_t y, uint32_t buttons){
    if(!desktop_mode || !app_is_visible(active_app))
        return;
    if(!(buttons & 1)){
        gui_drag_mode = 0;
        return;
    }
    if(gui_drag_mode == 1){
        gui_window_maximized = 0;
        gui_win_x = x > gui_drag_dx ? x - gui_drag_dx : 112;
        gui_win_y = y > gui_drag_dy ? y - gui_drag_dy : 38;
        clamp_window_geometry();
        gui_draw_desktop_core(0);
    } else if(gui_drag_mode == 2){
        gui_window_maximized = 0;
        gui_win_w = x > gui_win_x ? x - gui_win_x + gui_drag_dx : 520;
        gui_win_h = y > gui_win_y ? y - gui_win_y + gui_drag_dy : 380;
        clamp_window_geometry();
        gui_draw_desktop_core(0);
    }
}

static int icon_hit(uint32_t x, uint32_t y, uint32_t ix, uint32_t iy){
    return x >= ix - 8 && x < ix + 96 && y >= iy - 8 && y < iy + 84;
}

static int task_hit(uint32_t x, uint32_t start){
    return x >= start - 2 && x < start + 82;
}

static int inactive_window_action_at(uint32_t x, uint32_t y, int* consumed){
    for(int zi=(int)window_z_count - 1; zi>=0; zi--){
        struct gui_app_window* win = &app_windows[window_z_order[zi]];
        if(!(open_apps & win->mask) || app_is_minimized(win->app) || active_is(win->app))
            continue;
        if(x < win->x || x >= win->x + win->w || y < win->y || y >= win->y + win->h)
            continue;
        if(consumed)
            *consumed = 1;
        if(y >= win->y + 8 && y < win->y + 28){
            if(x >= win->x + win->w - 42 && x < win->x + win->w - 16){
                app_close(win->app);
                copy_text(launch_notice, "window closed", sizeof(launch_notice));
                return 1;
            }
            if(x >= win->x + win->w - 106 && x < win->x + win->w - 80){
                app_minimize(win->app);
                copy_text(launch_notice, "window minimized", sizeof(launch_notice));
                return 1;
            }
            if(x >= win->x + win->w - 74 && x < win->x + win->w - 48){
                gui_focus_app(win->app);
                window_toggle_maximize();
                return 1;
            }
        }
        gui_focus_app(win->app);
        if(x >= gui_win_x + gui_win_w - 24 && x < gui_win_x + gui_win_w &&
           y >= gui_win_y + gui_win_h - 24 && y < gui_win_y + gui_win_h){
            gui_drag_mode = 2;
            gui_drag_dx = gui_win_x + gui_win_w > x ? gui_win_x + gui_win_w - x : 0;
            gui_drag_dy = gui_win_y + gui_win_h > y ? gui_win_y + gui_win_h - y : 0;
            copy_text(launch_notice, "resize window", sizeof(launch_notice));
            return 1;
        }
        if(x >= gui_win_x + 58 && x < gui_win_x + gui_win_w - 112 &&
           y >= gui_win_y + 4 && y < gui_win_y + 40){
            gui_drag_mode = 1;
            gui_drag_dx = x - gui_win_x;
            gui_drag_dy = y - gui_win_y;
            copy_text(launch_notice, "drag window", sizeof(launch_notice));
            return 1;
        }
        if(consumed)
            *consumed = 0;
        copy_text(launch_notice, "window focused", sizeof(launch_notice));
        return 1;
    }
    return 0;
}

static void gui_window_list(void){
    console_puts("gui windows bottom->top:\n");
    for(uint32_t zi=0; zi<window_z_count; zi++){
        struct gui_app_window* win = &app_windows[window_z_order[zi]];
        console_puts(active_is(win->app) ? "[focus] " : "[     ] ");
        console_puts(win->app);
        console_puts((open_apps & win->mask) ? " open " : " closed ");
        console_puts(app_is_minimized(win->app) ? "min " : "shown ");
        console_write_dec(win->x);
        console_putc(',');
        console_write_dec(win->y);
        console_puts(" ");
        console_write_dec(win->w);
        console_putc('x');
        console_write_dec(win->h);
        console_putc('\n');
    }
}

static void gui_open_app(const char* app){
    gui_focus_app(app);
    launcher_open = 0;
    if(str_eq(app, "terminal")){
        copy_text(launch_notice, "opening terminal", sizeof(launch_notice));
        terminal_seed();
        terminal_focused = 1;
    } else if(str_eq(app, "saver")){
        saver_backdrop = 1;
        saver_live = 0;
        copy_text(launch_notice, "calm wallpaper controls", sizeof(launch_notice));
    } else {
        copy_text(launch_notice, "window opened", sizeof(launch_notice));
    }
}

void gui_handle_click(uint32_t x, uint32_t y){
    int inactive_consumed = 0;
    int routed_from_inactive = 0;
    gui_note_activity();
    uint32_t sx = design_x_from_screen(x);
    uint32_t sy = design_y_from_screen(y);
    if(!(app_is_visible(active_app) && x >= gui_win_x && x < gui_win_x + gui_win_w &&
         y >= gui_win_y && y < gui_win_y + gui_win_h) &&
       inactive_window_action_at(x, y, &inactive_consumed)){
        sx = design_x_from_screen(x);
        sy = design_y_from_screen(y);
        routed_from_inactive = !inactive_consumed;
        if(inactive_consumed){
            gui_draw_desktop();
            return;
        }
    }
    if(app_is_visible(active_app) && x >= gui_win_x + gui_win_w - 42 && x < gui_win_x + gui_win_w - 16 &&
       y >= gui_win_y + 10 && y < gui_win_y + 32){
        app_close(active_app);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
    } else if(app_is_visible(active_app) && x >= gui_win_x + gui_win_w - 74 && x < gui_win_x + gui_win_w - 48 &&
              y >= gui_win_y + 10 && y < gui_win_y + 32){
        window_toggle_maximize();
    } else if(app_is_visible(active_app) && x >= gui_win_x + gui_win_w - 106 && x < gui_win_x + gui_win_w - 80 &&
              y >= gui_win_y + 10 && y < gui_win_y + 32){
        app_minimize(active_app);
        copy_text(launch_notice, "window minimized", sizeof(launch_notice));
    } else if(app_is_visible(active_app) && x >= gui_win_x + gui_win_w - 24 && x < gui_win_x + gui_win_w &&
              y >= gui_win_y + gui_win_h - 24 && y < gui_win_y + gui_win_h){
        gui_drag_mode = 2;
        gui_drag_dx = gui_win_x + gui_win_w > x ? gui_win_x + gui_win_w - x : 0;
        gui_drag_dy = gui_win_y + gui_win_h > y ? gui_win_y + gui_win_h - y : 0;
        copy_text(launch_notice, "resize window", sizeof(launch_notice));
    } else if(app_is_visible(active_app) && x >= gui_win_x + 58 && x < gui_win_x + gui_win_w - 112 &&
              y >= gui_win_y + 4 && y < gui_win_y + 40){
        gui_drag_mode = 1;
        gui_drag_dx = x - gui_win_x;
        gui_drag_dy = y - gui_win_y;
        copy_text(launch_notice, "drag window", sizeof(launch_notice));
    } else if(app_is_visible(active_app) && sx >= 376 && sx < 488 && sy >= 124 && sy < 154){
        window_toggle_maximize();
    } else if(app_is_visible(active_app) && sx >= 512 && sx < 628 && sy >= 124 && sy < 154){
        app_minimize(active_app);
        copy_text(launch_notice, "window minimized", sizeof(launch_notice));
    } else if(app_is_visible(active_app) &&
              ((sx >= 652 && sx < 782 && sy >= 124 && sy < 154) ||
               (x >= app_x(652) && x < app_x(782) && y >= app_y(124) && y < app_y(154)))){
        app_close(active_app);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
    } else if(app_is_visible(active_app) && sx >= 812 && sx < 942 && sy >= 124 && sy < 154){
        copy_text(launch_notice, "opening terminal", sizeof(launch_notice));
        terminal_requested = 1;
    } else if(active_is("editor") && app_is_open("editor") && editor_dialog_mode &&
              sx >= 286 && sx < 846 && sy >= 218 && sy < 548){
        editor_focused = 0;
        if(sy >= 336 && sy < 364 && sx >= 312 && sx < 798){
            if(sx < 376) editor_dialog_up();
            else if(sx < 486) editor_dialog_confirm();
            else if(sx < 596) editor_dialog_cancel();
            else if(editor_dialog_mode == 2 && sx < 698) editor_dialog_save_quick(0);
            else if(editor_dialog_mode == 2) editor_dialog_save_quick(1);
        } else if(sy >= 390 && sy < 510 && sx >= 312 && sx < 812){
            editor_dialog_select_index((sy - 390) / 24);
            copy_text(launch_notice, "dialog item selected", sizeof(launch_notice));
        }
    } else if(active_is("rusa") && app_is_open("rusa") && sy >= 268 && sy < 298 && sx >= 226 && sx < 898){
        if(sx < 312) rusa_tab = 0;
        else if(sx < 412) rusa_tab = 1;
        else if(sx < 498) rusa_tab = 2;
        else if(sx < 584){
            rusa_tab = 3;
            rusa_workbench_action(0);
        }
        else if(sx < 662){
            rusa_tab = 4;
            rusa_workbench_action(1);
        }
        else if(sx < 788) rusa_tab = 5;
        else rusa_tab = 6;
        copy_text(launch_notice, "rusa tab changed", sizeof(launch_notice));
    } else if(active_is("math") && app_is_open("math") && sy >= 268 && sy < 298 && sx >= 226 && sx < 778){
        if(sx < 308) math_workbench_select(0);
        else if(sx < 402) math_workbench_select(1);
        else if(sx < 496) math_workbench_select(2);
        else if(sx < 600) math_workbench_select(3);
        else if(sx < 694) math_workbench_select(4);
        else math_workbench_select(5);
        copy_text(launch_notice, "math tab changed", sizeof(launch_notice));
    } else if(active_is("taskman") && app_is_open("taskman") && sy >= 268 && sy < 296 && sx >= 226 && sx < 600){
        if(sx < 312){
            gui_open_app(taskman_app_for_process(taskman_selected));
            return;
        } else if(sx < 418){
            process_set_running(taskman_selected, 1);
            copy_text(launch_notice, "process restarted", sizeof(launch_notice));
        } else if(sx < 504){
            copy_text(launch_notice, process_stop(taskman_selected) == 0 ? "process stopped" : "protected process", sizeof(launch_notice));
        } else {
            taskman_boost_compute();
            copy_text(launch_notice, "compute boosted", sizeof(launch_notice));
        }
    } else if(active_is("taskman") && app_is_open("taskman") && sy >= 344 && sy < 464 && sx >= 226 && sx < 926){
        uint32_t index = (sy - 344) / 20;
        const struct process_info* proc = process_at(index);
        if(proc){
            taskman_select_process(proc->name);
            copy_text(launch_notice, "task selected", sizeof(launch_notice));
        }
    } else if(active_is("files") && app_is_open("files") && sy >= 248 && sy < 276 && sx >= 226 && sx < 828){
        if(sx < 284) files_go_up();
        else if(sx < 378) files_new_file("new.txt");
        else if(sx < 472) files_new_folder("folder");
        else if(sx < 566) files_rename_selected("renamed.txt");
        else if(sx < 660) files_delete_selected();
        else if(sx < 744) files_copy_selected("");
        else files_move_selected("moved.txt");
    } else if(active_is("files") && app_is_open("files") && sy >= 286 && sy < 314 && sx >= 226 && sx < 574){
        if(sx < 298) files_open_selected_default();
        else if(sx < 382) files_open_selected_editor();
        else if(sx < 490) files_open_selected_terminal();
        else files_open_selected_rusa();
    } else if(active_is("files") && app_is_open("files") && sy >= 358 && sy < 526 && sx >= 226 && sx < 926){
        uint32_t index = (sy - 358) / 24;
        uint32_t now = timer_ticks();
        files_select_index(index);
        if(file_last_click_index == index && now - file_last_click_tick < 40)
            files_open_selected_default();
        else
            copy_text(launch_notice, "file selected", sizeof(launch_notice));
        file_last_click_index = index;
        file_last_click_tick = now;
    } else if(active_is("network") && app_is_open("network") && sy >= 268 && sy < 296 && sx >= 226 && sx < 596){
        if(sx < 308){
            char cmd[] = "open udp 9999";
            net_cmd(cmd);
            copy_text(launch_notice, "socket opened", sizeof(launch_notice));
        } else if(sx < 404){
            char cmd[] = "send 0 gui-ping";
            net_cmd(cmd);
            copy_text(launch_notice, "loopback packet sent", sizeof(launch_notice));
        } else if(sx < 500){
            char cmd[] = "flush";
            net_cmd(cmd);
            copy_text(launch_notice, "packet queue flushed", sizeof(launch_notice));
        } else {
            char cmd[16];
            copy_text(cmd, net_shield_enabled() ? "shield off" : "shield on", sizeof(cmd));
            net_cmd(cmd);
            copy_text(launch_notice, "network shield toggled", sizeof(launch_notice));
        }
    } else if(active_is("saver") && app_is_open("saver") && sy >= 268 && sy < 296 && sx >= 226 && sx < 870){
        if(sx < 298){ copy_text(saver_hint, "lava", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 390){ copy_text(saver_hint, "rain", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 482){ copy_text(saver_hint, "stars", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 574){ copy_text(saver_hint, "waves", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 666){ saver_backdrop = 1; saver_live = 1; gui_save_settings(); }
        else if(sx < 778){
            gui_preview_screensaver();
            copy_text(launch_notice, "screensaver preview", sizeof(launch_notice));
            terminal_requested = 1;
            return;
        } else { saver_backdrop = 0; saver_live = 0; gui_save_settings(); }
        if(sx < 666 || sx >= 778)
            copy_text(launch_notice, saver_backdrop ? (saver_live ? "slow live wallpaper on" : "calm wallpaper on") : "backdrop off", sizeof(launch_notice));
    } else if(active_is("saver") && app_is_open("saver") && sy >= 302 && sy < 332 && sx >= 226 && sx < 666){
        if(sx < 298) copy_text(screensaver_hint, "lava", sizeof(screensaver_hint));
        else if(sx < 390) copy_text(screensaver_hint, "rain", sizeof(screensaver_hint));
        else if(sx < 482) copy_text(screensaver_hint, "stars", sizeof(screensaver_hint));
        else if(sx < 574) copy_text(screensaver_hint, "waves", sizeof(screensaver_hint));
        else screensaver_calm = !screensaver_calm;
        gui_save_settings();
        copy_text(launch_notice, screensaver_calm ? "calm screensaver ready" : "full screensaver ready", sizeof(launch_notice));
    } else if(active_is("privacy") && app_is_open("privacy") && sy >= 236 && sy < 266 && sx >= 226 && sx < 742){
        if(sx < 318) settings_tab = 0;
        else if(sx < 428) settings_tab = 1;
        else if(sx < 544) settings_tab = 2;
        else if(sx < 648) settings_tab = 3;
        else settings_tab = 4;
        copy_text(launch_notice, "settings tab changed", sizeof(launch_notice));
    } else if(active_is("privacy") && app_is_open("privacy") && sy >= 312 && sy < 340 && sx >= 226 && sx < 812){
        if(settings_tab == 0){
            if(sx < 322){
                privacy_set_master(!privacy_master_enabled());
                copy_text(launch_notice, privacy_master_enabled() ? "master privacy on" : "master privacy off", sizeof(launch_notice));
            } else if(sx < 442){
                privacy_set_network(!privacy_network_enabled());
                copy_text(launch_notice, privacy_network_enabled() ? "network allowed" : "network disconnected", sizeof(launch_notice));
            } else if(sx < 562){
                const char* policy = privacy_cookie_policy();
                privacy_set_cookie_policy(str_eq(policy, "ask") ? "block" : (str_eq(policy, "block") ? "allow" : "ask"));
                copy_text(launch_notice, "cookie policy changed", sizeof(launch_notice));
            } else if(sx < 682){
                privacy_set_devices(!privacy_devices_enabled());
                copy_text(launch_notice, privacy_devices_enabled() ? "devices allowed" : "devices blocked", sizeof(launch_notice));
            } else {
                privacy_set_telemetry(!privacy_telemetry_enabled());
                copy_text(launch_notice, privacy_telemetry_enabled() ? "telemetry allowed" : "telemetry off", sizeof(launch_notice));
            }
        } else if(settings_tab == 1){
            if(sx < 338) request_terminal_command("hardware cpu", "cpu info");
            else if(sx < 468) request_terminal_command("hardware gpu", "gpu info");
            else request_terminal_command("hardware memory", "memory info");
        } else if(settings_tab == 2){
            if(sx < 322){
                char cmd[] = "caps";
                keyboard_cmd(cmd);
            } else if(sx < 442){
                char cmd[] = "num";
                keyboard_cmd(cmd);
            } else request_terminal_command("keyboard keys", "keyboard keys");
            copy_text(launch_notice, "keyboard setting changed", sizeof(launch_notice));
        } else if(settings_tab == 3){
            if(sx < 322) fb_set_cursor_style("dot");
            else if(sx < 442) fb_set_cursor_style("cross");
            else fb_set_cursor_style("target");
            gui_save_settings();
            copy_text(launch_notice, "display setting changed", sizeof(launch_notice));
        } else {
            if(sx < 322) settings_mouse_speed = 1;
            else if(sx < 442) settings_mouse_speed = 2;
            else if(sx < 562) settings_mouse_speed = 3;
            else request_terminal_command("mouse status", "mouse status");
            gui_save_settings();
            copy_text(launch_notice, "input setting changed", sizeof(launch_notice));
        }
    } else if(active_is("terminal") && app_is_open("terminal") && sx >= 244 && sx < 904 && sy >= 502 && sy < 534){
        terminal_focused = 1;
        copy_text(launch_notice, "terminal input ready", sizeof(launch_notice));
    } else if(active_is("editor") && app_is_open("editor") && sx >= 388 && sx < 906 && sy >= 346 && sy < 566){
        editor_focused = 1;
        editor_line = editor_top + ((sy > 372) ? (sy - 372) / 20 : 0);
        if(editor_line >= GUI_EDITOR_MAX_LINES) editor_line = GUI_EDITOR_MAX_LINES - 1;
        editor_col = sx > 438 ? (sx - 438) / GUI_FONT_ADVANCE : 0;
        editor_clamp_cursor();
        copy_text(launch_notice, "editor ready for typing", sizeof(launch_notice));
    } else if(active_is("editor") && app_is_open("editor") && sy >= 236 && sy < 266 && sx >= 226 && sx < 506){
        if(sx < 318) editor_set_mode(0);
        else if(sx < 412) editor_set_mode(1);
        else editor_set_mode(2);
        editor_focused = 1;
        copy_text(launch_notice, editor_mode == 1 ? "editor code mode" : (editor_mode == 2 ? "editor math notes" : "editor paper mode"), sizeof(launch_notice));
    } else if(active_is("editor") && app_is_open("editor") && sy >= 278 && sy < 306 && sx >= 226 && sx < 704){
        editor_focused = 1;
        if(sx < 322){
            editor_seed();
            editor_dirty = 1;
            copy_text(launch_notice, "new document ready", sizeof(launch_notice));
        }
        else if(sx < 442){
            editor_dialog_open(1);
        }
        else if(sx < 562){
            editor_save_file();
            gui_save_settings();
            copy_text(launch_notice, "document saved", sizeof(launch_notice));
        }
        else {
            editor_dialog_open(1);
        }
    } else if(active_is("editor") && app_is_open("editor") && sy >= 310 && sy < 338 && sx >= 226 && sx < 728){
        editor_focused = 1;
        if(sx < 298){
            editor_select_range(editor_line, editor_line + 2);
            copy_text(launch_notice, "lines selected", sizeof(launch_notice));
        } else if(sx < 382){
            editor_copy_range();
            copy_text(launch_notice, "selection copied", sizeof(launch_notice));
        } else if(sx < 466){
            editor_copy_range();
            editor_delete_range();
            copy_text(launch_notice, "selection cut", sizeof(launch_notice));
        } else if(sx < 550){
            editor_paste_range();
            copy_text(launch_notice, "clipboard pasted", sizeof(launch_notice));
        } else if(sx < 634){
            copy_text(launch_notice, editor_find(editor_mode ? "print" : "Title") >= 0 ? "find matched" : "find missed", sizeof(launch_notice));
        } else {
            editor_dialog_open(2);
        }
    } else if(active_is("projects") && app_is_open("projects") && sy >= 268 && sy < 296 && sx >= 226 && sx < 582){
        if(sx < 322) request_terminal_command("project list", "project list");
        else if(sx < 462) request_terminal_command("project new demo", "new demo project");
        else request_terminal_command("project run demo", "run demo project");
    } else if(active_is("packages") && app_is_open("packages") && sy >= 268 && sy < 296 && sx >= 226 && sx < 582){
        if(sx < 322) request_terminal_command("pkg list", "package list");
        else if(sx < 442) request_terminal_command("pkg compat", "package compat");
        else request_terminal_command("pkg info rusa-docs", "rusa docs package");
    } else if(active_is("logs") && app_is_open("logs") && sy >= 268 && sy < 296 && sx >= 226 && sx < 562){
        if(sx < 322) request_terminal_command("log show system", "system log");
        else if(sx < 442) request_terminal_command("log show security", "security log");
        else request_terminal_command("log show network", "network log");
    } else if(active_is("security") && app_is_open("security") && sy >= 236 && sy < 266 && sx >= 226 && sx < 872){
        if(sx < 308) security_tab = 0;
        else if(sx < 402) security_tab = 1;
        else if(sx < 496) security_tab = 2;
        else if(sx < 600) security_tab = 3;
        else if(sx < 694) security_tab = 4;
        else if(sx < 788) security_tab = 5;
        else security_tab = 6;
        copy_text(launch_notice, "security panel changed", sizeof(launch_notice));
    } else if(active_is("security") && app_is_open("security") && sy >= 276 && sy < 304 && sx >= 226 && sx < 776){
        if(sx < 298){
            char cmd[] = "lock";
            security_cmd(cmd);
            copy_text(launch_notice, "secure mode on", sizeof(launch_notice));
        } else if(sx < 398){
            char cmd[] = "unlock";
            security_cmd(cmd);
            copy_text(launch_notice, "permissive mode", sizeof(launch_notice));
        } else if(sx < 494) request_terminal_command("privacy ports", "ports and connections");
        else if(sx < 580) request_terminal_command("lang scan /home/projects/demo.rusa", "malicious-code scan");
        else if(sx < 666) request_terminal_command("security audit", "security audit");
        else request_terminal_command("privacy status", "privacy center");
    } else if(active_is("security") && app_is_open("security") && security_tab == 1 && sy >= 490 && sy < 518 && sx >= 250 && sx < 498){
        if(sx < 378){
            if(net_socket_close(0) == 0)
                copy_text(launch_notice, "connection 0 disconnected", sizeof(launch_notice));
            else
                copy_text(launch_notice, "connection 0 not active", sizeof(launch_notice));
        } else {
            request_terminal_command("firewall list", "firewall rules");
            return;
        }
    } else if(active_is("security") && app_is_open("security") && security_tab == 2 && sy >= 456 && sy < 484 && sx >= 250 && sx < 522){
        if(sx < 378){
            security_set_permission("demo", "network.client", SECURITY_PERMISSION_ALLOW);
            copy_text(launch_notice, "demo network allowed", sizeof(launch_notice));
        } else {
            security_set_permission("demo", "network.client", SECURITY_PERMISSION_DENY);
            copy_text(launch_notice, "demo network denied", sizeof(launch_notice));
        }
    } else if(active_is("security") && app_is_open("security") && security_tab == 3 && sy >= 490 && sy < 518 && sx >= 250 && sx < 498){
        if(sx < 366) request_terminal_command("services", "service registry");
        else request_terminal_command("service stop network", "stop network service");
        return;
    } else if(active_is("events") && app_is_open("events") && sy >= 268 && sy < 296 && sx >= 226 && sx < 582){
        if(sx < 322) request_terminal_command("event list", "event list");
        else if(sx < 442) request_terminal_command("event emit fs.write", "emit fs event");
        else request_terminal_command("event list", "rusa events");
    } else if(active_is("storage") && app_is_open("storage") && sy >= 268 && sy < 296 && sx >= 226 && sx < 562){
        if(sx < 322) request_terminal_command("block status", "block status");
        else if(sx < 442) request_terminal_command("mounts", "mount table");
        else request_terminal_command("fd all", "file descriptors");
    } else if(launcher_open && x >= 36 && x < 314 && y >= 412 && y < 686){
        uint32_t index = (y - 412) / 26;
        if(index < sizeof(launcher_entries)/sizeof(launcher_entries[0])){
            gui_open_app(launcher_entries[index].app);
            launcher_open = 0;
            copy_text(launch_notice, "launcher opened app", sizeof(launch_notice));
        }
    } else if(launcher_open && !(x >= 18 && x < 332 && y >= 372 && y < 710)){
        launcher_open = 0;
        copy_text(launch_notice, "start menu closed", sizeof(launch_notice));
    } else if(routed_from_inactive){
        copy_text(launch_notice, "window focused", sizeof(launch_notice));
    } else if(icon_hit(x, y, 32, 70)) gui_open_app("files");
    else if(icon_hit(x, y, 32, 170)) gui_open_app("terminal");
    else if(icon_hit(x, y, 32, 270)) gui_open_app("math");
    else if(icon_hit(x, y, 32, 370)) gui_open_app("rusa");
    else if(icon_hit(x, y, 32, 470)) gui_open_app("privacy");
    else if(icon_hit(x, y, 32, 570)) gui_open_app("editor");
    else if(icon_hit(x, y, 882, 70)) gui_open_app("network");
    else if(icon_hit(x, y, 882, 170)){
        gui_open_app("saver");
    } else if(icon_hit(x, y, 882, 270)) gui_open_app("taskman");
    else if(icon_hit(x, y, 882, 370)) gui_open_app("projects");
    else if(icon_hit(x, y, 882, 470)) gui_open_app("packages");
    else if(icon_hit(x, y, 882, 570)) gui_open_app("logs");
    else if(y >= 720){
        if(x < 112){
            launcher_open = !launcher_open;
            copy_text(launch_notice, launcher_open ? "start menu open" : "start menu closed", sizeof(launch_notice));
        }
        else if(task_hit(x, 124)) gui_open_app("files");
        else if(task_hit(x, 212)) gui_open_app("terminal");
        else if(task_hit(x, 300)) gui_open_app("math");
        else if(task_hit(x, 388)) gui_open_app("rusa");
        else if(task_hit(x, 476)) gui_open_app("privacy");
        else if(task_hit(x, 564)) gui_open_app("editor");
        else if(task_hit(x, 652)) gui_open_app("taskman");
        else if(task_hit(x, 740)) gui_open_app("network");
    }
    gui_draw_desktop();
}

int gui_take_terminal_request(void){
    int requested = terminal_requested;
    terminal_requested = 0;
    return requested;
}
