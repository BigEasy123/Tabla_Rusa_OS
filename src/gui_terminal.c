#include <stdint.h>
#include "console.h"
#include "gui_terminal.h"
#include "keyboard.h"
#include "shell.h"

static char terminal_view[160] = "Terminal ready. Open an app command or press Enter.";
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

static uint32_t text_len32(const char* s){
    uint32_t n = 0;
    while(s && s[n]) n++;
    return n;
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(max == 0) return;
    if(!src) src = "";
    while(src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
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

void gui_terminal_seed(void){
    if(terminal_count)
        return;
    terminal_add_line("Tabla Rusa GUI Terminal");
    terminal_add_line("Type commands here, Enter runs them.");
}

void gui_terminal_set_focused(int focused){
    terminal_focused = focused ? 1 : 0;
}

int gui_terminal_focused(void){
    return terminal_focused;
}

const char* gui_terminal_view(void){
    return terminal_view;
}

void gui_terminal_set_view(const char* text){
    copy_text(terminal_view, text, sizeof(terminal_view));
}

const char* gui_terminal_input(void){
    return terminal_input;
}

uint32_t gui_terminal_cursor(void){
    return terminal_cursor;
}

uint32_t gui_terminal_count(void){
    return terminal_count;
}

uint32_t gui_terminal_top(void){
    return terminal_top;
}

void gui_terminal_set_top(uint32_t top){
    terminal_top = top < terminal_count ? top : (terminal_count ? terminal_count - 1 : 0);
}

const char* gui_terminal_line(uint32_t index){
    if(index >= terminal_count)
        return "";
    return terminal_lines[index];
}

int gui_terminal_line_selected(uint32_t index){
    return terminal_selection && index >= terminal_sel_lo() && index <= terminal_sel_hi();
}

int gui_terminal_clipboard_has_text(void){
    return terminal_clipboard[0] != 0;
}

void gui_terminal_input_text(char* out, uint32_t max){
    copy_text(out, terminal_input, max);
}

void gui_terminal_clear_input(void){
    terminal_set_input("");
}

void gui_terminal_select_range(uint32_t start, uint32_t end){
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

void gui_terminal_copy_selection(void){
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

void gui_terminal_paste_clipboard(void){
    uint32_t len = text_len32(terminal_input);
    for(uint32_t i=0; terminal_clipboard[i] && len + 1 < sizeof(terminal_input); i++){
        char c = terminal_clipboard[i];
        if(c == '\n' || c == '\r')
            c = ' ';
        terminal_insert_char(c);
        len = text_len32(terminal_input);
    }
}

void gui_terminal_clear(void){
    terminal_count = 0;
    terminal_top = 0;
    terminal_selection = 0;
    terminal_clipboard[0] = 0;
    gui_terminal_seed();
}

int gui_terminal_handle_key(int key){
    uint32_t len = text_len32(terminal_input);
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
        gui_terminal_scroll(1);
    } else if(key == KB_KEY_PAGE_DOWN){
        gui_terminal_scroll(-1);
    } else if(key >= 32 && key <= 126){
        terminal_insert_char((char)key);
    } else {
        return 0;
    }
    return 1;
}

void gui_terminal_scroll(int amount){
    if(amount > 0){
        terminal_top = terminal_top > 0 ? terminal_top - 1 : 0;
    } else if(amount < 0){
        if(terminal_top + 8 < terminal_count)
            terminal_top++;
    }
}
