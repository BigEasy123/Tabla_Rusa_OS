#include <stdint.h>
#include "console.h"
#include "fb.h"
#include "fs.h"
#include "mouse.h"
#include "keyboard.h"
#include "net.h"
#include "privacy.h"
#include "process.h"
#include "service.h"
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
static char editor_current_path[96] = "/home/notes.txt";
static char file_dir[96] = "/home";
static char file_selected[96] = "/home/readme.txt";
static char terminal_view[160] = "Terminal ready. Open an app command or press Enter.";
static char rusa_view[160] = "Rusa Workbench ready. Choose Check or Run.";
static char editor_clipboard[72] = "";
static uint32_t gui_win_x = 174;
static uint32_t gui_win_y = 72;
static uint32_t gui_win_w = 820;
static uint32_t gui_win_h = 594;
static int gui_dragging = 0;

#define GUI_EDITOR_VISIBLE_LINES 9
#define GUI_EDITOR_MAX_LINES 64
#define GUI_EDITOR_COLS 72
#define GUI_FONT_ADVANCE 6
#define GUI_WALLPAPER_TICKS 90
#define GUI_INPUT_QUIET_TICKS 30

static char editor_buf[GUI_EDITOR_MAX_LINES][GUI_EDITOR_COLS];

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

void gui_enter_desktop(void);
static const char* editor_path(void);
static void editor_clamp_cursor(void);
static void gui_focus_app(const char* app);

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

static int app_is_open(const char* app){
    uint32_t mask = app_mask(app);
    return mask != 0 && (open_apps & mask) != 0;
}

static void app_open(const char* app){
    open_apps |= app_mask(app);
}

static void app_focus_next_or_desktop(void){
    if(open_apps & APP_FILES) copy_text(active_app, "files", sizeof(active_app));
    else if(open_apps & APP_MATH) copy_text(active_app, "math", sizeof(active_app));
    else if(open_apps & APP_RUSA) copy_text(active_app, "rusa", sizeof(active_app));
    else if(open_apps & APP_SETTINGS) copy_text(active_app, "privacy", sizeof(active_app));
    else if(open_apps & APP_TASKS) copy_text(active_app, "taskman", sizeof(active_app));
    else if(open_apps & APP_NET) copy_text(active_app, "network", sizeof(active_app));
    else if(open_apps & APP_EDITOR) copy_text(active_app, "editor", sizeof(active_app));
    else if(open_apps & APP_SAVER) copy_text(active_app, "saver", sizeof(active_app));
    else if(open_apps & APP_PROJECTS) copy_text(active_app, "projects", sizeof(active_app));
    else if(open_apps & APP_PACKAGES) copy_text(active_app, "packages", sizeof(active_app));
    else if(open_apps & APP_LOGS) copy_text(active_app, "logs", sizeof(active_app));
    else if(open_apps & APP_SECURITY) copy_text(active_app, "security", sizeof(active_app));
    else if(open_apps & APP_EVENTS) copy_text(active_app, "events", sizeof(active_app));
    else if(open_apps & APP_STORAGE) copy_text(active_app, "storage", sizeof(active_app));
    else if(open_apps & APP_INSPECTOR) copy_text(active_app, "inspector", sizeof(active_app));
    else if(open_apps & APP_TERM) copy_text(active_app, "terminal", sizeof(active_app));
    else copy_text(active_app, "desktop", sizeof(active_app));
}

static void app_close(const char* app){
    uint32_t mask = app_mask(app);
    if(mask == 0){
        copy_text(active_app, "desktop", sizeof(active_app));
        editor_focused = 0;
        return;
    }
    open_apps &= ~mask;
    if(mask == APP_EDITOR)
        editor_focused = 0;
    if(mask == APP_TERM && app_mask(active_app) == mask)
        copy_text(active_app, "desktop", sizeof(active_app));
    else if(app_mask(active_app) == mask)
        app_focus_next_or_desktop();
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

static void gui_note_activity(void){
    gui_busy_until = timer_ticks() + GUI_INPUT_QUIET_TICKS;
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

static void files_select_index(uint32_t index){
    char name[40];
    int type = 0;
    if(fs_child_name(file_dir, (int)index, name, sizeof(name), &type) != 0)
        return;
    fs_join_path(file_dir, name, file_selected, sizeof(file_selected));
    if(type == 1)
        copy_text(file_dir, file_selected, sizeof(file_dir));
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

static const char* app_title(void){
    if(active_is("desktop")) return "Desktop";
    if(active_is("files")) return "Files";
    if(active_is("taskman")) return "Task Manager";
    if(active_is("math")) return "Math Lab";
    if(active_is("rusa")) return "Rusa Workbench";
    if(active_is("privacy")) return "Settings";
    if(active_is("network")) return "Network";
    if(active_is("saver")) return "Screensavers";
    if(active_is("inspector")) return "Inspector";
    if(active_is("editor")) return "Tabla Editor";
    if(active_is("projects")) return "Projects";
    if(active_is("packages")) return "Packages";
    if(active_is("logs")) return "Logs";
    if(active_is("security")) return "Security";
    if(active_is("events")) return "Events";
    if(active_is("storage")) return "Storage";
    return "Terminal";
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
    else if(active_is("math")) copy_text(out, "math help", max);
    else if(active_is("rusa")){
        if(rusa_tab == 1) copy_text(out, "rusa keywords", max);
        else if(rusa_tab == 2) copy_text(out, "rusa docs", max);
        else if(rusa_tab == 3) copy_text(out, "rusa check /home/projects/demo.rusa", max);
        else if(rusa_tab == 4) copy_text(out, "rusa run /home/projects/demo.rusa", max);
        else if(rusa_tab == 5) copy_text(out, "rusa last-error", max);
        else copy_text(out, "rusa examples", max);
    }
    else if(active_is("privacy")) copy_text(out, "privacy status", max);
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
    }
}

static void request_terminal_command(const char* command, const char* notice){
    copy_text(launch_override, command, sizeof(launch_override));
    copy_text(launch_notice, notice, sizeof(launch_notice));
    copy_text(terminal_view, command, sizeof(terminal_view));
    terminal_requested = 1;
}

static void gui_focus_app(const char* app){
    app = canonical_app(app);
    app_open(app);
    copy_text(active_app, app, sizeof(active_app));
    if(str_eq(app, "terminal")) window_focus("shell");
    else if(str_eq(app, "inspector") || str_eq(app, "taskman")) window_focus("inspector");
    else if(str_eq(app, "editor") || str_eq(app, "rusa") || str_eq(app, "files") ||
            str_eq(app, "projects") || str_eq(app, "packages")) window_focus("editor");
    else if(str_eq(app, "network") || str_eq(app, "privacy") || str_eq(app, "saver") ||
            str_eq(app, "logs") || str_eq(app, "security") || str_eq(app, "events") ||
            str_eq(app, "storage")) window_focus("network");
}

static void draw_dock_item(uint32_t x, const char* app, const char* label, uint32_t color){
    uint32_t edge = active_is(app) ? 0xFFFFFF : 0x202830;
    fb_fill_rect(x - 2, 728, 82, 34, edge);
    fb_fill_rect(x, 730, 78, 30, color);
    fb_draw_text(x + 8, 742, label, 0xFFFFFF);
}

static void draw_app_line(uint32_t row, const char* label, const char* value){
    uint32_t y = 166 + row * 34;
    fb_draw_text(226, y, label, 0xCFE8FF);
    fb_draw_text(378, y, value, 0xFFFFFF);
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
    fb_fill_rect(x, y, w, 28, 0x0C1118);
    fb_fill_rect(x + 2, y + 2, w - 4, 24, color);
    fb_draw_text(x + 12, y + 18, label, 0xFFFFFF);
}

static void draw_mode_button(uint32_t x, uint32_t y, uint32_t w, const char* label, int selected, uint32_t color){
    fb_fill_rect(x, y, w, 30, selected ? 0xFFFFFF : 0x0C1118);
    fb_fill_rect(x + 2, y + 2, w - 4, 26, selected ? color : 0x2C3946);
    fb_draw_text(x + 12, y + 19, label, 0xFFFFFF);
}

static void draw_editor_surface(void){
    char prefix[8];
    char line_info[36];
    fb_fill_rect(226, 328, 700, 260, 0xFFFFFF);
    fb_fill_rect(226, 328, 150, 260, 0x26313C);
    fb_draw_text(242, 352, "Explorer", 0xCFE8FF);
    fb_draw_text(242, 386, editor_mode ? "demo.rusa" : "notes.txt", 0xFFFFFF);
    fb_draw_text(242, 420, editor_dirty ? "unsaved" : "saved", editor_dirty ? 0xFFD28A : 0xCFE8FF);
    fb_draw_text(242, 454, editor_focused ? "typing on" : "click page", 0xCFE8FF);
    fb_fill_rect(388, 346, 518, 220, 0xF8FAFC);
    for(uint32_t row=0; row<GUI_EDITOR_VISIBLE_LINES; row++){
        uint32_t doc_row = editor_top + row;
        uint32_t y = 372 + row * 20;
        prefix[0] = (doc_row == editor_line && editor_focused) ? '>' : ' ';
        editor_line_label(doc_row, prefix + 1, sizeof(prefix) - 1);
        fb_draw_text(404, y, prefix, 0x6A7580);
        fb_draw_text(438, y, editor_buf[doc_row], 0x223040);
        if(doc_row == editor_line && editor_focused){
            uint32_t cx = 438 + editor_col * GUI_FONT_ADVANCE;
            if(cx > 894) cx = 894;
            fb_fill_rect(cx, y, 2, 8, 0x111820);
        }
    }
    fb_fill_rect(912, 346, 6, 220, 0xD0D8E0);
    fb_fill_rect(912, 346 + (editor_top * 180) / (GUI_EDITOR_MAX_LINES - GUI_EDITOR_VISIBLE_LINES),
                 6, 40, 0x536070);
    copy_text(line_info, "Line ", sizeof(line_info));
    char num[8];
    u32_text(editor_line + 1, num, sizeof(num));
    append_text(line_info, num, sizeof(line_info));
    append_text(line_info, "/", sizeof(line_info));
    u32_text(GUI_EDITOR_MAX_LINES, num, sizeof(num));
    append_text(line_info, num, sizeof(line_info));
    fb_draw_text(410, 562, line_info, 0x2E6B4C);
    fb_draw_text(538, 562, editor_mode ? "Code: arrows/wheel/Page edit, Save writes demo.rusa" :
                                      "Paper: arrows/wheel/Page edit, Save writes notes.txt", 0x2E6B4C);
}

static void draw_rusa_surface(void){
    fb_fill_rect(226, 328, 700, 230, 0xFFFFFF);
    fb_fill_rect(226, 328, 160, 230, 0x2C243C);
    fb_draw_text(244, 354, "Rusa", 0xFFFFFF);
    fb_draw_text(244, 388, ".rusa files", 0xCFE8FF);
    fb_draw_text(244, 422, "packages", 0xCFE8FF);
    fb_draw_text(244, 456, "diagnostics", 0xCFE8FF);
    fb_fill_rect(404, 348, 500, 186, 0xF8FAFC);
    fb_draw_text(426, 374, "Tab", 0x223040);
    fb_draw_text(510, 374, rusa_tab_name(), 0x5B3C9A);
    if(rusa_tab == 1){
        fb_draw_text(426, 414, "Keywords: import let set fn return if else while repeat", 0x223040);
        fb_draw_text(426, 448, "Objects: file process service window program math phys", 0x223040);
    } else if(rusa_tab == 2){
        fb_draw_text(426, 414, "Docs: readable syntax, curly blocks, typed values", 0x223040);
        fb_draw_text(426, 448, "Use Open Terminal for the matching docs command.", 0x223040);
    } else if(rusa_tab == 3){
        fb_draw_text(426, 414, "Check: validates /home/projects/demo.rusa", 0x223040);
        fb_draw_text(426, 448, rusa_view, 0x223040);
    } else if(rusa_tab == 4){
        fb_draw_text(426, 414, "Run: executes the source parser and scanner gate.", 0x223040);
        fb_draw_text(426, 448, rusa_view, 0x223040);
    } else if(rusa_tab == 5){
        fb_draw_text(426, 414, "Diagnostics: last-error and open-error workflow.", 0x223040);
        fb_draw_text(426, 448, "Goal: plain English, clickable jump surface next.", 0x223040);
    } else {
        fb_draw_text(426, 414, "Examples: variables, functions, loops, imports, events.", 0x223040);
        fb_draw_text(426, 448, "Open Terminal runs: rusa examples", 0x223040);
    }
}

static void draw_active_app_detail(void){
    char num[16];
    char line[72];
    if(active_is("desktop") || !app_is_open(active_app)){
        fb_draw_text(204, 74, "Dynamic desktop wallpaper", 0xFFFFFF);
        fb_draw_text(204, 102, "Click an icon or taskbar app to open a window.", 0xFFFFFF);
        return;
    }
    fb_fill_rect(gui_win_x, gui_win_y, gui_win_w, gui_win_h, 0x0C1118);
    fb_fill_rect(gui_win_x + 4, gui_win_y + 4, gui_win_w - 8, gui_win_h - 8, 0xEEF2F6);
    fb_fill_rect(gui_win_x + 4, gui_win_y + 4, gui_win_w - 8, 36, 0x1E2A36);
    fb_draw_text(198, 92, app_title(), 0xFFFFFF);
    fb_fill_rect(946, 84, 28, 22, 0xA84A4A);
    fb_fill_rect(914, 84, 28, 22, 0xC8A848);
    fb_fill_rect(882, 84, 28, 22, 0x4A9A68);
    fb_draw_text(956, 100, "x", 0xFFFFFF);
    fb_draw_text(226, 132, "GUI window. Use buttons below, or Open Terminal for command mode.", 0x223040);
    draw_button(812, 124, 130, "Open Terminal", 0x345A7A);
    draw_button(652, 124, 130, "Close Window", 0xA84A4A);
    if(active_is("files")){
        char name[40];
        char row[80];
        int type = 0;
        draw_app_line(1, "Folder", file_dir);
        draw_app_line(2, "Selected", file_selected);
        draw_button(226, 268, 96, "Open", 0x3C704C);
        draw_button(346, 268, 96, "Edit", 0x345A7A);
        draw_button(466, 268, 116, "Terminal", 0x725C9A);
        fb_fill_rect(226, 328, 700, 220, 0xF8FAFC);
        for(uint32_t i=0; i<7; i++){
            if(fs_child_name(file_dir, (int)i, name, sizeof(name), &type) != 0)
                break;
            copy_text(row, type == 1 ? "[dir]  " : "[file] ", sizeof(row));
            append_text(row, name, sizeof(row));
            fb_draw_text(250, 358 + i * 24, row, 0x223040);
        }
        fb_draw_text(250, 536, "Click a row, Open enters folders, Edit opens files in Editor.", 0x2E6B4C);
    } else if(active_is("taskman")){
        struct process_info* compute = process_find("compute");
        u32_text(compute ? compute->ticks : 0, num, sizeof(num));
        copy_text(line, "compute ticks=", sizeof(line));
        append_text(line, num, sizeof(line));
        draw_app_line(1, "Processes", "scheduler jobs services process table");
        draw_app_line(2, "Compute", line);
        draw_app_line(3, "Action", "Open Terminal runs: taskman top");
    } else if(active_is("math")){
        draw_app_line(1, "Catalog", "linear algebra groups physics latex units jobs");
        draw_app_line(2, "Physics", "gravity electric magnetic fields first principles");
        draw_app_line(3, "Action", "Open Terminal runs: math help");
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
        copy_text(line, privacy_allows_network() ? "network on cookies " : "network off cookies ", sizeof(line));
        append_text(line, privacy_cookie_policy(), sizeof(line));
        draw_app_line(1, "Master", line);
        copy_text(line, "cursor ", sizeof(line));
        append_text(line, fb_cursor_style(), sizeof(line));
        append_text(line, "  use: gui cursor dot", sizeof(line));
        draw_app_line(2, "Pointer", line);
        draw_button(226, 268, 96, "Security", 0x884C4C);
        draw_button(346, 268, 96, "Events", 0x725C9A);
        draw_button(466, 268, 96, "Storage", 0x345A7A);
        draw_app_line(4, "Action", "Open Terminal runs: privacy status");
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
        fb_fill_rect(226, 328, 700, 220, 0x101820);
        fb_draw_text(250, 358, "Wallpaper is calm by default to avoid blinking.", 0xCFE8FF);
        fb_draw_text(250, 398, "Click Lava, Rain, Stars, or Waves for still wallpaper.", 0xFFFFFF);
        fb_draw_text(250, 438, "Use gui wallpaper live MODE for slow animation.", 0xFFFFFF);
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
        draw_editor_surface();
    } else if(active_is("inspector")){
        draw_app_line(1, "System", "memory scheduler trace replay logs");
        draw_app_line(2, "Terminal", "Enter opens: inspect memory");
    } else {
        draw_app_line(1, "Shell", "GUI terminal surface");
        draw_app_line(2, "Last", terminal_view);
        fb_fill_rect(226, 228, 700, 330, 0x101820);
        fb_draw_text(250, 260, "Tabla Rusa Terminal", 0x8EE8A0);
        fb_draw_text(250, 304, terminal_view, 0xFFFFFF);
        fb_draw_text(250, 348, "Press Enter or Open Terminal for full command input.", 0xCFE8FF);
    }
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
    draw_active_app_detail();
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
    return active_is("desktop") || !app_is_open(active_app);
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
            editor_open_path(path[0] ? path : editor_current_path);
            copy_text(launch_notice, "editor file opened", sizeof(launch_notice));
        } else if(str_eq(mode, "save")){
            const char* path = first_arg(rest, &rest);
            if(path[0])
                copy_text(editor_current_path, path, sizeof(editor_current_path));
            editor_save_file();
            gui_save_settings();
            copy_text(launch_notice, "editor file saved", sizeof(launch_notice));
        } else if(str_eq(mode, "new")){
            const char* path = first_arg(rest, &rest);
            if(path[0])
                copy_text(editor_current_path, path, sizeof(editor_current_path));
            editor_seed();
            editor_dirty = 1;
            copy_text(launch_notice, "new editor file", sizeof(launch_notice));
        } else if(str_eq(mode, "copy")){
            copy_text(editor_clipboard, editor_buf[editor_line], sizeof(editor_clipboard));
            copy_text(launch_notice, "line copied", sizeof(launch_notice));
        } else if(str_eq(mode, "paste")){
            copy_text(editor_buf[editor_line], editor_clipboard, sizeof(editor_buf[editor_line]));
            editor_dirty = 1;
            copy_text(launch_notice, "line pasted", sizeof(launch_notice));
        } else if(mode[0]){
            console_puts("usage: gui editor paper|code|new [PATH]|open [PATH]|save [PATH]|copy|paste\n");
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
        else if(str_eq(tab, "check")) rusa_tab = 3;
        else if(str_eq(tab, "run")) rusa_tab = 4;
        else if(str_eq(tab, "diagnostics") || str_eq(tab, "errors")) rusa_tab = 5;
        else if(tab[0]){
            console_puts("usage: gui rusa examples|keywords|docs|check|run|diagnostics\n");
            return;
        }
        copy_text(launch_notice, "rusa tab changed", sizeof(launch_notice));
        gui_focus_app("rusa");
        gui_draw_desktop();
    } else if(str_eq(action, "close")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0) name = active_app;
        name = canonical_app(name);
        app_close(name);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
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
    } else if(str_eq(action, "tab") || str_eq(action, "next")){
        window_focus_next();
        gui_draw_desktop();
    } else if(str_eq(action, "focus")){
        const char* name = first_arg(rest, &rest);
        if(name[0] == 0){
            console_puts("usage: gui focus WINDOW\n");
            return;
        }
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
        console_puts("usage: gui status | start | stop | desktop | draw | app NAME | editor MODE | rusa TAB | wallpaper MODE|off | saver [NAME] [backdrop] | windows | tab | focus NAME | move NAME X Y | resize active W H | click X Y\n");
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
        copy_text(editor_clipboard, editor_buf[editor_line], sizeof(editor_clipboard));
        copy_text(launch_notice, "line copied", sizeof(launch_notice));
        gui_draw_desktop_core(0);
        return 1;
    }
    if(keyboard_ctrl_down() && (key == 'v' || key == 'V')){
        copy_text(editor_buf[editor_line], editor_clipboard, sizeof(editor_buf[editor_line]));
        editor_dirty = 1;
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
        editor_insert_char((char)key);
    } else {
        return 0;
    }
    editor_clamp_cursor();
    gui_draw_desktop_core(0);
    console_input_write("Tabla Editor - typing in GUI document");
    return 1;
}

int gui_key_captures(int key){
    if(!(active_is("editor") && app_is_open("editor") && editor_focused)) return 0;
    return key == '\n' || key == 8 || key == 127 ||
           key == KB_KEY_LEFT || key == KB_KEY_RIGHT || key == KB_KEY_UP || key == KB_KEY_DOWN ||
           key == KB_KEY_PAGE_UP || key == KB_KEY_PAGE_DOWN ||
           key == KB_KEY_HOME || key == KB_KEY_END || key == KB_KEY_DELETE ||
           (key >= 32 && key <= 126);
}

void gui_handle_key(int key){
    gui_note_activity();
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
    if(!(desktop_mode && active_is("editor") && app_is_open("editor")))
        return 0;
    if(mouse_x() < 388 || mouse_x() >= 926 || mouse_y() < 346 || mouse_y() >= 566)
        return 0;
    gui_note_activity();
    editor_focused = 1;
    editor_scroll(amount > 0 ? 3 : -3);
    gui_draw_desktop_core(0);
    console_input_write("Tabla Editor - scrolled document");
    return 1;
}

void gui_handle_drag(uint32_t x, uint32_t y, uint32_t buttons){
    if(!desktop_mode || !app_is_open(active_app))
        return;
    if(!(buttons & 1)){
        gui_dragging = 0;
        return;
    }
    if(gui_dragging){
        gui_win_x = x > 260 ? x - 260 : 20;
        gui_win_y = y > 24 ? y - 24 : 40;
        if(gui_win_x + gui_win_w > 1004) gui_win_x = 1004 - gui_win_w;
        if(gui_win_y + gui_win_h > 710) gui_win_y = 710 - gui_win_h;
        gui_draw_desktop_core(0);
    }
}

static int icon_hit(uint32_t x, uint32_t y, uint32_t ix, uint32_t iy){
    return x >= ix - 8 && x < ix + 96 && y >= iy - 8 && y < iy + 84;
}

static int task_hit(uint32_t x, uint32_t start){
    return x >= start - 2 && x < start + 82;
}

static void gui_open_app(const char* app){
    gui_focus_app(app);
    if(str_eq(app, "terminal")){
        copy_text(launch_notice, "opening terminal", sizeof(launch_notice));
        terminal_requested = 1;
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
    if(app_is_open(active_app) && x >= gui_win_x + 18 && x < gui_win_x + gui_win_w - 92 &&
       y >= gui_win_y + 4 && y < gui_win_y + 40){
        gui_dragging = 1;
        copy_text(launch_notice, "drag window", sizeof(launch_notice));
    } else if(app_is_open(active_app) && x >= 936 && x < 982 && y >= 78 && y < 114){
        app_close(active_app);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
    } else if(app_is_open(active_app) && x >= 652 && x < 782 && y >= 124 && y < 154){
        app_close(active_app);
        copy_text(launch_notice, "window closed", sizeof(launch_notice));
    } else if(app_is_open(active_app) && x >= 812 && x < 942 && y >= 124 && y < 154){
        copy_text(launch_notice, "opening terminal", sizeof(launch_notice));
        terminal_requested = 1;
    } else if(active_is("rusa") && app_is_open("rusa") && y >= 268 && y < 298 && x >= 226 && x < 788){
        if(x < 312) rusa_tab = 0;
        else if(x < 412) rusa_tab = 1;
        else if(x < 498) rusa_tab = 2;
        else if(x < 584){
            rusa_tab = 3;
            copy_text(rusa_view, "Check passed or terminal shows exact diagnostic.", sizeof(rusa_view));
            request_terminal_command("rusa check /home/projects/demo.rusa", "rusa check");
        }
        else if(x < 662){
            rusa_tab = 4;
            copy_text(rusa_view, "Run queued: /home/projects/demo.rusa", sizeof(rusa_view));
            request_terminal_command("rusa run /home/projects/demo.rusa", "rusa run");
        }
        else rusa_tab = 5;
        copy_text(launch_notice, "rusa tab changed", sizeof(launch_notice));
    } else if(active_is("files") && app_is_open("files") && y >= 268 && y < 296 && x >= 226 && x < 582){
        if(x < 322){
            int type = 0;
            size_t size = 0;
            if(fs_stat(file_selected, &type, &size) == 0 && type == 1)
                copy_text(file_dir, file_selected, sizeof(file_dir));
            else
                files_open_selected_editor();
        } else if(x < 442){
            files_open_selected_editor();
        } else {
            request_terminal_command("tree /home", "tree /home");
        }
    } else if(active_is("files") && app_is_open("files") && y >= 358 && y < 526 && x >= 226 && x < 926){
        files_select_index((y - 358) / 24);
        copy_text(launch_notice, "file selected", sizeof(launch_notice));
    } else if(active_is("saver") && app_is_open("saver") && y >= 268 && y < 296 && x >= 226 && x < 870){
        if(x < 298){ copy_text(saver_hint, "lava", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(x < 390){ copy_text(saver_hint, "rain", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(x < 482){ copy_text(saver_hint, "stars", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(x < 574){ copy_text(saver_hint, "waves", sizeof(saver_hint)); saver_backdrop = 1; saver_live = 0; }
        else if(x < 666){ saver_backdrop = 1; saver_live = 1; gui_save_settings(); }
        else if(x < 778){
            copy_text(launch_notice, "screensaver preview", sizeof(launch_notice));
            terminal_requested = 1;
        } else { saver_backdrop = 0; saver_live = 0; gui_save_settings(); }
        if(x < 666 || x >= 778)
            copy_text(launch_notice, saver_backdrop ? (saver_live ? "slow live wallpaper on" : "calm wallpaper on") : "backdrop off", sizeof(launch_notice));
    } else if(active_is("privacy") && app_is_open("privacy") && y >= 268 && y < 296 && x >= 226 && x < 562){
        if(x < 322) gui_open_app("security");
        else if(x < 442) gui_open_app("events");
        else gui_open_app("storage");
    } else if(active_is("editor") && app_is_open("editor") && x >= 388 && x < 906 && y >= 346 && y < 566){
        editor_focused = 1;
        editor_line = editor_top + ((y > 372) ? (y - 372) / 20 : 0);
        if(editor_line >= GUI_EDITOR_MAX_LINES) editor_line = GUI_EDITOR_MAX_LINES - 1;
        editor_col = x > 438 ? (x - 438) / GUI_FONT_ADVANCE : 0;
        editor_clamp_cursor();
        copy_text(launch_notice, "editor ready for typing", sizeof(launch_notice));
    } else if(active_is("editor") && app_is_open("editor") && y >= 236 && y < 266 && x >= 226 && x < 450){
        editor_set_mode(x < 330 ? 0 : 1);
        editor_focused = 1;
        copy_text(launch_notice, editor_mode ? "editor code mode" : "editor paper mode", sizeof(launch_notice));
    } else if(active_is("editor") && app_is_open("editor") && y >= 278 && y < 306 && x >= 226 && x < 704){
        editor_focused = 1;
        if(x < 322){
            editor_seed();
            editor_dirty = 1;
            copy_text(launch_notice, "new document ready", sizeof(launch_notice));
        }
        else if(x < 442){
            editor_load_file();
            copy_text(launch_notice, "document opened", sizeof(launch_notice));
        }
        else if(x < 562){
            editor_save_file();
            gui_save_settings();
            copy_text(launch_notice, "document saved", sizeof(launch_notice));
        }
        else {
            copy_text(launch_notice, "opening editor", sizeof(launch_notice));
            terminal_requested = 1;
        }
    } else if(active_is("projects") && app_is_open("projects") && y >= 268 && y < 296 && x >= 226 && x < 582){
        if(x < 322) request_terminal_command("project list", "project list");
        else if(x < 462) request_terminal_command("project new demo", "new demo project");
        else request_terminal_command("project run demo", "run demo project");
    } else if(active_is("packages") && app_is_open("packages") && y >= 268 && y < 296 && x >= 226 && x < 582){
        if(x < 322) request_terminal_command("pkg list", "package list");
        else if(x < 442) request_terminal_command("pkg compat", "package compat");
        else request_terminal_command("pkg info rusa-docs", "rusa docs package");
    } else if(active_is("logs") && app_is_open("logs") && y >= 268 && y < 296 && x >= 226 && x < 562){
        if(x < 322) request_terminal_command("log show system", "system log");
        else if(x < 442) request_terminal_command("log show security", "security log");
        else request_terminal_command("log show network", "network log");
    } else if(active_is("security") && app_is_open("security") && y >= 268 && y < 296 && x >= 226 && x < 562){
        if(x < 322) request_terminal_command("security status", "security status");
        else if(x < 442) request_terminal_command("security audit", "security audit");
        else request_terminal_command("user list", "user list");
    } else if(active_is("events") && app_is_open("events") && y >= 268 && y < 296 && x >= 226 && x < 582){
        if(x < 322) request_terminal_command("event list", "event list");
        else if(x < 442) request_terminal_command("event emit fs.write", "emit fs event");
        else request_terminal_command("event list", "rusa events");
    } else if(active_is("storage") && app_is_open("storage") && y >= 268 && y < 296 && x >= 226 && x < 562){
        if(x < 322) request_terminal_command("block status", "block status");
        else if(x < 442) request_terminal_command("mounts", "mount table");
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
