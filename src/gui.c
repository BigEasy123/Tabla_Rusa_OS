#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "mouse.h"
#include "keyboard.h"
#include "lang.h"
#include "mathlib.h"
#include "memory.h"
#include "net.h"
#include "privacy.h"
#include "process.h"
#include "service.h"
#include "shell.h"
#include "timer.h"
#include "window.h"
#include "gui.h"

static int running = 0;
static int autostarted = 0;
static int desktop_mode = 0;
static int terminal_requested = 0;
static char active_app[16] = "desktop";
static char saver_hint[16] = "lava";
static char launch_notice[48] = "";
static char launch_override[128] = "";
static uint32_t open_apps = 0;
static int saver_backdrop = 1;
static int saver_live = 0;
static uint32_t saver_last_tick = 0;
static uint32_t gui_busy_until = 0;
static int editor_mode = 0; /* 0=paper, 1=code */
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
static char terminal_lines[24][96];
static uint32_t terminal_count = 0;
static uint32_t terminal_top = 0;
static int terminal_focused = 0;
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
static void gui_focus_app(const char* app);
static void request_terminal_command(const char* command, const char* notice);
static void draw_active_app_detail(void);
static void gui_window_list(void);
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

static void terminal_seed(void){
    if(terminal_count)
        return;
    terminal_add_line("Tabla Rusa GUI Terminal");
    terminal_add_line("Type commands here, Enter runs them.");
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
    terminal_input[0] = 0;
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

static void math_workbench_select(int tab){
    char cmd[96];
    math_tab = tab;
    if(math_tab == 0){
        math_set_line(0, "Vector lab");
        math_set_line(1, "dot([1,2,3], [4,5,6]) = 32");
        math_set_line(2, "norm-ish workload: vector profile priority=98");
        math_set_line(3, "Terminal command: math vec dot 1 2 3 | 4 5 6");
        math_set_line(4, "Good for quick linear-algebra sanity checks.");
        copy_text(cmd, "vec dot 1 2 3 | 4 5 6", sizeof(cmd));
    } else if(math_tab == 1){
        math_set_line(0, "Matrix lab");
        math_set_line(1, "det([[1,2],[3,4]]) = -2");
        math_set_line(2, "stored object path can save/load matrices.");
        math_set_line(3, "Terminal command: math object matrix A 1 2 3 4");
        math_set_line(4, "Next: GUI matrix editor grid.");
        copy_text(cmd, "object matrix A 1 2 3 4", sizeof(cmd));
    } else if(math_tab == 2){
        math_set_line(0, "Group theory lab");
        math_set_line(1, "Units mod 12 = {1,5,7,11}");
        math_set_line(2, "Cyclic/order helpers are available in terminal math.");
        math_set_line(3, "Terminal command: math group units 12");
        math_set_line(4, "Useful for algebra-first computational experiments.");
        copy_text(cmd, "group units 12", sizeof(cmd));
    } else if(math_tab == 3){
        math_set_line(0, "First-principles physics");
        math_set_line(1, "gravity: F = G*M*m/r^2, scaled G=667");
        math_set_line(2, "electric, magnetic, orbit, and field energy are wired.");
        math_set_line(3, "Terminal command: math phys fields 1 2 3 | 4 5 6");
        math_set_line(4, "Jobs account this as a physics-worker workload.");
        copy_text(cmd, "phys fields 1 2 3 | 4 5 6", sizeof(cmd));
    } else if(math_tab == 4){
        math_set_line(0, "LaTeX converter");
        math_set_line(1, "vec 1 2 3 -> \\begin{bmatrix}1\\\\2\\\\3\\end{bmatrix}");
        math_set_line(2, "Matrices, fractions, polynomials, and objects convert.");
        math_set_line(3, "Terminal command: math latex mat2 1 2 3 4");
        math_set_line(4, "Copy raw output from Terminal for papers.");
        copy_text(cmd, "latex mat2 1 2 3 4", sizeof(cmd));
    } else {
        math_set_line(0, "Scientific job accounting");
        math_set_line(1, "Math actions mark compute workload and job ticks.");
        math_set_line(2, "Vector, matrix, group, and physics jobs get priority hints.");
        math_set_line(3, "Terminal command: taskman top");
        math_set_line(4, "This is the Tabla Rusa scientific-computing lane.");
        copy_text(cmd, "bench vector", sizeof(cmd));
    }
    math_cmd(cmd);
    fs_append_line("/var/log/system.log", "gui: Math Lab tab selected");
}

static void gui_save_settings(void){
    char text[256];
    copy_text(text, "wallpaper=", sizeof(text));
    append_text(text, saver_hint, sizeof(text));
    append_text(text, "\nwallpaper_live=", sizeof(text));
    append_text(text, saver_live ? "yes" : "no", sizeof(text));
    append_text(text, "\ncursor=", sizeof(text));
    append_text(text, fb_cursor_style(), sizeof(text));
    append_text(text, "\neditor_mode=", sizeof(text));
    append_text(text, editor_mode ? "code" : "paper", sizeof(text));
    append_text(text, "\neditor_path=", sizeof(text));
    append_text(text, editor_current_path, sizeof(text));
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
    if(text_has(text, "cursor=cross")) fb_set_cursor_style("cross");
    else if(text_has(text, "cursor=target")) fb_set_cursor_style("target");
    else if(text_has(text, "cursor=dot")) fb_set_cursor_style("dot");
    if(text_has(text, "editor_mode=code")) editor_mode = 1;
    else if(text_has(text, "editor_mode=paper")) editor_mode = 0;
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
    if(editor_mode){
        copy_text(editor_buf[0], "import std", sizeof(editor_buf[0]));
        copy_text(editor_buf[1], "", sizeof(editor_buf[1]));
        copy_text(editor_buf[2], "fn main() {", sizeof(editor_buf[2]));
        copy_text(editor_buf[3], "  let force: int = 42", sizeof(editor_buf[3]));
        copy_text(editor_buf[4], "  print force", sizeof(editor_buf[4]));
        copy_text(editor_buf[5], "}", sizeof(editor_buf[5]));
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
        copy_text(editor_current_path, editor_mode ? "/home/projects/demo.rusa" : "/home/notes.txt", sizeof(editor_current_path));
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
        else copy_text(out, "rusa examples", max);
    }
    else if(active_is("privacy")){
        if(settings_tab == 1) copy_text(out, "hardware status", max);
        else if(settings_tab == 2) copy_text(out, "keyboard status", max);
        else if(settings_tab == 3) copy_text(out, "hardware gpu", max);
        else copy_text(out, "privacy status", max);
    }
    else if(active_is("network")) copy_text(out, "net status", max);
    else if(active_is("projects")) copy_text(out, "project list", max);
    else if(active_is("packages")) copy_text(out, "pkg list", max);
    else if(active_is("logs")) copy_text(out, "log show system", max);
    else if(active_is("security")) copy_text(out, "security status", max);
    else if(active_is("events")) copy_text(out, "event list", max);
    else if(active_is("storage")) copy_text(out, "block status", max);
    else if(active_is("saver")){
        copy_text(out, "fb saver ", max);
        append_text(out, saver_hint, max);
        append_text(out, " 12", max);
    } else if(active_is("inspector")) copy_text(out, "inspect memory", max);
    else if(active_is("editor")){
        copy_text(out, "edit ", max);
        append_text(out, editor_path(), max);
    } else if(active_is("terminal")) copy_text(out, terminal_view, max);
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
    app_draw_text(242, 386, editor_mode ? "demo.rusa" : "notes.txt", 0xFFFFFF);
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
    app_draw_text(538, 562, editor_mode ? "Code: arrows/wheel/Page edit, Save writes demo.rusa" :
                                      "Paper: arrows/wheel/Page edit, Save writes notes.txt", 0x2E6B4C);
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
    } else {
        app_draw_text(426, 414, "Examples: variables, functions, loops, imports, events.", 0x223040);
        app_draw_text(426, 448, "Open Terminal runs: rusa examples", 0x223040);
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
}

static void draw_terminal_surface(void){
    char prompt[120];
    app_fill_rect(226, 208, 700, 350, 0x101820);
    app_fill_rect(226, 208, 700, 28, 0x1E2A36);
    app_draw_text(244, 226, "GUI Terminal", 0x8EE8A0);
    uint32_t visible = terminal_count - terminal_top;
    if(visible > 8) visible = 8;
    for(uint32_t i=0; i<visible; i++)
        app_draw_text(250, 268 + i * 24, terminal_lines[terminal_top + i], 0xFFFFFF);
    copy_text(prompt, "tr:gui $ ", sizeof(prompt));
    append_text(prompt, terminal_input, sizeof(prompt));
    app_fill_rect(244, 502, 660, 32, terminal_focused ? 0x213040 : 0x18222C);
    app_draw_text(252, 522, prompt, terminal_focused ? 0x8EE8A0 : 0xCFE8FF);
    app_draw_text(250, 548, "Click input area, type command, Enter runs in place. Open Terminal switches full-screen.", 0xCFE8FF);
}

static const char* app_summary_for(const char* app){
    if(str_eq(app, "files")) return file_dir;
    if(str_eq(app, "terminal")) return terminal_view;
    if(str_eq(app, "math")) return math_tab_name();
    if(str_eq(app, "rusa")) return rusa_tab_name();
    if(str_eq(app, "privacy")) return "hardware privacy keyboard display";
    if(str_eq(app, "network")) return net_is_link_up() ? "network link up" : "network link down";
    if(str_eq(app, "saver")) return saver_hint;
    if(str_eq(app, "editor")) return editor_path();
    if(str_eq(app, "taskman")) return "processes jobs services";
    if(str_eq(app, "projects")) return "/home/projects";
    if(str_eq(app, "packages")) return "package registry";
    if(str_eq(app, "logs")) return "system security network";
    if(str_eq(app, "security")) return "secure mode audit users";
    if(str_eq(app, "events")) return "event rules";
    if(str_eq(app, "storage")) return "block fd mounts";
    return "system inspector";
}

static void draw_inactive_window(struct gui_app_window* win){
    if(!win || app_is_minimized(win->app) || active_is(win->app))
        return;
    uint32_t x = win->x;
    uint32_t y = win->y;
    uint32_t w = win->w;
    uint32_t h = win->h;
    fb_fill_rect(x + 8, y + 8, w, h, 0x12202A);
    fb_fill_rect(x, y, w, h, 0x778899);
    fb_fill_rect(x + 2, y + 2, w - 4, h - 4, 0xEEF2F6);
    fb_fill_rect(x + 2, y + 2, w - 4, 30, 0x394858);
    fb_fill_rect(x + 14, y + 12, 8, 8, 0xA84A4A);
    fb_fill_rect(x + 28, y + 12, 8, 8, 0xC8A848);
    fb_fill_rect(x + 42, y + 12, 8, 8, 0x4A9A68);
    fb_draw_text(x + 72, y + 21, app_title_for(win->app), 0xFFFFFF);
    fb_fill_rect(x + w - 42, y + 8, 26, 18, 0xA84A4A);
    fb_fill_rect(x + w - 74, y + 8, 26, 18, 0x4A9A68);
    fb_fill_rect(x + w - 106, y + 8, 26, 18, 0xC8A848);
    fb_draw_text(x + w - 34, y + 22, "x", 0xFFFFFF);
    fb_draw_text(x + w - 66, y + 22, win->maximized ? "r" : "+", 0xFFFFFF);
    fb_draw_text(x + w - 98, y + 22, "-", 0xFFFFFF);
    fb_draw_text(x + 18, y + 64, app_summary_for(win->app), 0x223040);
    fb_draw_text(x + 18, y + 92, "Click to focus; controls work after focus.", 0x536070);
    if(w > 320 && h > 160){
        fb_fill_rect(x + 18, y + 120, w - 36, h - 140, 0xF8FAFC);
        fb_draw_text(x + 34, y + 150, "Inactive live surface", 0x536070);
    }
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

static int focus_inactive_window_at(uint32_t x, uint32_t y){
    for(int zi=(int)window_z_count - 1; zi>=0; zi--){
        struct gui_app_window* win = &app_windows[window_z_order[zi]];
        if(!(open_apps & win->mask) || app_is_minimized(win->app) || active_is(win->app))
            continue;
        if(x >= win->x && x < win->x + win->w && y >= win->y && y < win->y + win->h){
            gui_focus_app(win->app);
            copy_text(launch_notice, "window focused", sizeof(launch_notice));
            return 1;
        }
    }
    for(int i=(int)app_window_count() - 1; i>=0; i--){
        struct gui_app_window* win = &app_windows[i];
        if(!(open_apps & win->mask) || app_is_minimized(win->app) || active_is(win->app))
            continue;
        if(x >= win->x && x < win->x + win->w && y >= win->y && y < win->y + win->h){
            gui_focus_app(win->app);
            copy_text(launch_notice, "window focused", sizeof(launch_notice));
            return 1;
        }
    }
    return 0;
}

static void draw_active_app_detail(void){
    char num[16];
    char line[72];
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
        draw_app_line(1, "Folder", file_dir);
        draw_app_line(2, "Selected", file_selected);
        draw_button(226, 248, 58, "Up", 0x345A7A);
        draw_button(296, 248, 82, "New File", 0x3C704C);
        draw_button(390, 248, 82, "New Dir", 0x3C704C);
        draw_button(484, 248, 82, "Rename", 0x887034);
        draw_button(578, 248, 82, "Delete", 0xA84A4A);
        draw_button(226, 286, 72, "Open", 0x3C704C);
        draw_button(310, 286, 72, "Edit", 0x345A7A);
        draw_button(394, 286, 96, "Terminal", 0x725C9A);
        draw_button(502, 286, 72, "Rusa", 0x5B3C9A);
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
        struct process_info* compute = process_find("compute");
        u32_text(compute ? compute->ticks : 0, num, sizeof(num));
        copy_text(line, "compute ticks=", sizeof(line));
        append_text(line, num, sizeof(line));
        draw_app_line(1, "Processes", "scheduler jobs services process table");
        draw_app_line(2, "Compute", line);
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
        draw_rusa_surface();
    } else if(active_is("privacy")){
        draw_app_line(1, "Settings", "privacy hardware keyboard display");
        draw_mode_button(226, 236, 92, "Privacy", settings_tab == 0, 0x884C4C);
        draw_mode_button(330, 236, 98, "Hardware", settings_tab == 1, 0x345A7A);
        draw_mode_button(440, 236, 104, "Keyboard", settings_tab == 2, 0x3C704C);
        draw_mode_button(556, 236, 92, "Display", settings_tab == 3, 0x725C9A);
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
        } else {
            copy_text(line, privacy_allows_network() ? "network on cookies " : "network off cookies ", sizeof(line));
            append_text(line, privacy_cookie_policy(), sizeof(line));
            draw_app_line(2, "Master", line);
            copy_text(line, "cursor ", sizeof(line));
            append_text(line, fb_cursor_style(), sizeof(line));
            append_text(line, "  use: gui cursor dot", sizeof(line));
            draw_app_line(3, "Pointer", line);
            draw_button(226, 312, 96, "Security", 0x884C4C);
            draw_button(346, 312, 96, "Events", 0x725C9A);
            draw_button(466, 312, 96, "Storage", 0x345A7A);
        }
    } else if(active_is("network")){
        u32_text(net_packet_count(), num, sizeof(num));
        copy_text(line, net_is_link_up() ? "link up packets=" : "link down packets=", sizeof(line));
        append_text(line, num, sizeof(line));
        draw_app_line(1, "Stack", line);
        draw_app_line(2, "Shield", "privacy gate mask ports packet queue");
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
        draw_app_line(1, "Mode", "secure mode capabilities audit namespace guard");
        draw_app_line(2, "Users", "root guest capability grants");
        draw_button(226, 268, 96, "Status", 0x345A7A);
        draw_button(346, 268, 96, "Audit", 0x884C4C);
        draw_button(466, 268, 96, "Users", 0x725C9A);
        draw_app_line(4, "Action", "Open Terminal runs: security status");
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
        draw_button(226, 268, 72, "Lava", 0x8A4A40);
        draw_button(318, 268, 72, "Rain", 0x3A86A8);
        draw_button(410, 268, 72, "Stars", 0x604A88);
        draw_button(502, 268, 72, "Waves", 0x386878);
        draw_button(594, 268, 72, "Live", 0x3C704C);
        draw_button(686, 268, 92, "Preview", 0x725C9A);
        draw_button(798, 268, 72, "Off", 0x555A60);
        app_fill_rect(226, 328, 700, 220, 0x101820);
        app_draw_text(250, 358, "Wallpaper is calm by default to avoid blinking.", 0xCFE8FF);
        app_draw_text(250, 398, "Click Lava, Rain, Stars, or Waves for still wallpaper.", 0xFFFFFF);
        app_draw_text(250, 438, "Use gui wallpaper live MODE for slow animation.", 0xFFFFFF);
        draw_app_line(4, "Action", "Open Terminal runs: fb saver MODE 12");
    } else if(active_is("editor")){
        draw_app_line(1, "Mode", editor_mode ? "code workspace" : "paper drafting");
        draw_app_line(2, "File", editor_path());
        draw_mode_button(226, 236, 104, "Paper", editor_mode == 0, 0x345A7A);
        draw_mode_button(346, 236, 104, "Code", editor_mode == 1, 0x3C704C);
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

static void gui_draw_desktop(void){
    gui_draw_desktop_core(1);
}

void gui_init(void){
    fs_mkdir("/system/gui");
    fs_write("/system/gui/state.txt", "state=ready\nautostart=on\nsurface=desktop\npointer=crosshair\n");
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
        console_puts("objects: compositor window-manager tab-strip input-router desktop screensaver\n");
        console_puts("crosshair=");
        console_write_dec(mouse_x());
        console_putc(',');
        console_write_dec(mouse_y());
        console_puts(" buttons=");
        console_write_dec(mouse_buttons());
        console_putc('\n');
    } else if(str_eq(action, "start")){
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
            console_puts("usage: gui files up|new NAME|mkdir NAME|rename NAME|delete|select N|open [editor|terminal|rusa]\n");
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
            if(path[0])
                copy_text(editor_current_path, path, sizeof(editor_current_path));
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
            console_puts("usage: gui editor paper|code|new [PATH]|open [PATH]|openas PATH|save [PATH]|saveas PATH|dialog ACTION|select A B|copy|cut|paste|find TEXT\n");
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
        else if(tab[0]){
            console_puts("usage: gui rusa examples|keywords|docs|check|run|diagnostics\n");
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
    } else if(str_eq(action, "settings")){
        const char* tab = first_arg(rest, &rest);
        if(str_eq(tab, "privacy")) settings_tab = 0;
        else if(str_eq(tab, "hardware")) settings_tab = 1;
        else if(str_eq(tab, "keyboard")) settings_tab = 2;
        else if(str_eq(tab, "display") || str_eq(tab, "gpu")) settings_tab = 3;
        else if(tab[0]){
            console_puts("usage: gui settings privacy|hardware|keyboard|display\n");
            return;
        }
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
        copy_text(saver_hint, name[0] ? name : "lava", sizeof(saver_hint));
        gui_focus_app("saver");
        if(str_eq(mode, "backdrop") || str_eq(mode, "live")){
            saver_backdrop = 1;
            saver_live = str_eq(mode, "live");
            copy_text(launch_notice, saver_live ? "slow live wallpaper" : "calm wallpaper", sizeof(launch_notice));
            gui_draw_desktop();
        } else {
            fb_run_saver(saver_hint, 12);
            fb_draw_text(28, 28, "Tabla Rusa OS screensaver - Esc returns to desktop", 0xFFFFFF);
            fb_set_mouse(mouse_x(), mouse_y(), mouse_buttons());
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
        console_puts("usage: gui status | start | stop | desktop | draw | app NAME | editor MODE | rusa TAB | math TAB | wallpaper MODE|off | saver [NAME] [backdrop] | windows | tab | focus NAME | move NAME X Y | resize active W H | minimize|restore|maximize [APP] | click X Y\n");
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
        gui_draw_desktop_core(0);
        return 1;
    }
    if(keyboard_ctrl_down() && (key == 'x' || key == 'X')){
        editor_copy_range();
        editor_delete_range();
        copy_text(launch_notice, "selection cut", sizeof(launch_notice));
        gui_draw_desktop_core(0);
        return 1;
    }
    if(keyboard_ctrl_down() && (key == 'v' || key == 'V')){
        editor_paste_range();
        copy_text(launch_notice, "line pasted", sizeof(launch_notice));
        gui_draw_desktop_core(0);
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
    gui_draw_desktop_core(0);
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
        if(len)
            terminal_input[len - 1] = 0;
    } else if(key == KB_KEY_PAGE_UP || key == KB_KEY_UP){
        terminal_top = terminal_top > 0 ? terminal_top - 1 : 0;
    } else if(key == KB_KEY_PAGE_DOWN || key == KB_KEY_DOWN){
        if(terminal_top + 8 < terminal_count)
            terminal_top++;
    } else if(key >= 32 && key <= 126 && len + 1 < sizeof(terminal_input)){
        terminal_input[len] = (char)key;
        terminal_input[len + 1] = 0;
    } else {
        return 0;
    }
    gui_draw_desktop_core(0);
    console_input_write("GUI Terminal - type commands in the window");
    return 1;
}

int gui_key_captures(int key){
    if(active_is("terminal") && app_is_open("terminal") && terminal_focused)
        return key == '\n' || key == 8 || key == 127 ||
               key == KB_KEY_UP || key == KB_KEY_DOWN ||
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
        copy_text(saver_hint, "lava", sizeof(saver_hint));
        fb_run_saver(saver_hint, 12);
        fb_draw_text(28, 28, "Tabla Rusa OS screensaver - Esc returns to desktop", 0xFFFFFF);
        fb_set_mouse(mouse_x(), mouse_y(), mouse_buttons());
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
            gui_draw_desktop_core(0);
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
    gui_draw_desktop_core(0);
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

static int inactive_window_action_at(uint32_t x, uint32_t y){
    for(int zi=(int)window_z_count - 1; zi>=0; zi--){
        struct gui_app_window* win = &app_windows[window_z_order[zi]];
        if(!(open_apps & win->mask) || app_is_minimized(win->app) || active_is(win->app))
            continue;
        if(x < win->x || x >= win->x + win->w || y < win->y || y >= win->y + win->h)
            continue;
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
        copy_text(launch_notice, "window focused", sizeof(launch_notice));
        return 1;
    }
    return focus_inactive_window_at(x, y);
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
    gui_note_activity();
    uint32_t sx = design_x_from_screen(x);
    uint32_t sy = design_y_from_screen(y);
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
    } else if(!(app_is_visible(active_app) && x >= gui_win_x && x < gui_win_x + gui_win_w &&
                y >= gui_win_y && y < gui_win_y + gui_win_h) && inactive_window_action_at(x, y)){
        /* Focus handled above. */
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
    } else if(active_is("rusa") && app_is_open("rusa") && sy >= 268 && sy < 298 && sx >= 226 && sx < 788){
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
        else rusa_tab = 5;
        copy_text(launch_notice, "rusa tab changed", sizeof(launch_notice));
    } else if(active_is("math") && app_is_open("math") && sy >= 268 && sy < 298 && sx >= 226 && sx < 778){
        if(sx < 308) math_workbench_select(0);
        else if(sx < 402) math_workbench_select(1);
        else if(sx < 496) math_workbench_select(2);
        else if(sx < 600) math_workbench_select(3);
        else if(sx < 694) math_workbench_select(4);
        else math_workbench_select(5);
        copy_text(launch_notice, "math tab changed", sizeof(launch_notice));
    } else if(active_is("files") && app_is_open("files") && sy >= 248 && sy < 276 && sx >= 226 && sx < 660){
        if(sx < 284) files_go_up();
        else if(sx < 378) files_new_file("new.txt");
        else if(sx < 472) files_new_folder("folder");
        else if(sx < 566) files_rename_selected("renamed.txt");
        else files_delete_selected();
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
    } else if(active_is("saver") && app_is_open("saver") && sy >= 268 && sy < 296 && sx >= 226 && sx < 870){
        if(sx < 298){ copy_text(saver_hint, "lava", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 390){ copy_text(saver_hint, "rain", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 482){ copy_text(saver_hint, "stars", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 574){ copy_text(saver_hint, "waves", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(sx < 666){ saver_backdrop = 1; saver_live = 1; gui_save_settings(); }
        else if(sx < 778){
            copy_text(launch_notice, "screensaver preview", sizeof(launch_notice));
            terminal_requested = 1;
        } else { saver_backdrop = 0; saver_live = 0; gui_save_settings(); }
        if(sx < 666 || sx >= 778)
            copy_text(launch_notice, saver_backdrop ? (saver_live ? "slow live wallpaper on" : "calm wallpaper on") : "backdrop off", sizeof(launch_notice));
    } else if(active_is("privacy") && app_is_open("privacy") && sy >= 236 && sy < 266 && sx >= 226 && sx < 648){
        if(sx < 318) settings_tab = 0;
        else if(sx < 428) settings_tab = 1;
        else if(sx < 544) settings_tab = 2;
        else settings_tab = 3;
        copy_text(launch_notice, "settings tab changed", sizeof(launch_notice));
    } else if(active_is("privacy") && app_is_open("privacy") && sy >= 312 && sy < 340 && sx >= 226 && sx < 598){
        if(settings_tab == 0){
            if(sx < 322) gui_open_app("security");
            else if(sx < 442) gui_open_app("events");
            else gui_open_app("storage");
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
        } else {
            if(sx < 322) fb_set_cursor_style("dot");
            else if(sx < 442) fb_set_cursor_style("cross");
            else fb_set_cursor_style("target");
            gui_save_settings();
            copy_text(launch_notice, "display setting changed", sizeof(launch_notice));
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
    } else if(active_is("editor") && app_is_open("editor") && sy >= 236 && sy < 266 && sx >= 226 && sx < 450){
        editor_set_mode(sx < 330 ? 0 : 1);
        editor_focused = 1;
        copy_text(launch_notice, editor_mode ? "editor code mode" : "editor paper mode", sizeof(launch_notice));
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
    } else if(active_is("security") && app_is_open("security") && sy >= 268 && sy < 296 && sx >= 226 && sx < 562){
        if(sx < 322) request_terminal_command("security status", "security status");
        else if(sx < 442) request_terminal_command("security audit", "security audit");
        else request_terminal_command("user list", "user list");
    } else if(active_is("events") && app_is_open("events") && sy >= 268 && sy < 296 && sx >= 226 && sx < 582){
        if(sx < 322) request_terminal_command("event list", "event list");
        else if(sx < 442) request_terminal_command("event emit fs.write", "emit fs event");
        else request_terminal_command("event list", "rusa events");
    } else if(active_is("storage") && app_is_open("storage") && sy >= 268 && sy < 296 && sx >= 226 && sx < 562){
        if(sx < 322) request_terminal_command("block status", "block status");
        else if(sx < 442) request_terminal_command("mounts", "mount table");
        else request_terminal_command("fd all", "file descriptors");
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
        if(x < 112) gui_open_app("terminal");
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
