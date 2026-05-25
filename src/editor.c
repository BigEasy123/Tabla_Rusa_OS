#include <stdint.h>
#include <stddef.h>
#include "console.h"
#include "editor.h"
#include "events.h"
#include "fs.h"
#include "process.h"
#include "shell.h"
#include "window.h"

#define EDITOR_MAX_LINES 20
#define EDITOR_LINE_MAX 72

static int active = 0;
static char path_current[64];
static char lines[EDITOR_MAX_LINES][EDITOR_LINE_MAX];
static size_t line_count = 0;
static size_t current_line = 0;

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z')
        return (char)(c - 'A' + 'a');
    return c;
}

static int str_is(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != *b)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void str_copy(char* dst, const char* src, size_t max){
    size_t i = 0;
    if(max == 0) return;
    while(i + 1 < max && src[i]){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

void editor_init(void){
    active = 0;
    path_current[0] = 0;
    line_count = 0;
    current_line = 0;
}

int editor_is_active(void){
    return active;
}

const char* editor_path_current(void){
    return path_current;
}

void editor_close(void){
    active = 0;
    shell_set_editor_mode(0);
    process_set_running("editor", 0);
    path_current[0] = 0;
    line_count = 0;
    current_line = 0;
    console_clear_output();
    console_puts("editor closed\n");
}

static void editor_line_no(size_t line){
    uint32_t n = (uint32_t)(line + 1);
    if(n < 10)
        console_putc('0');
    console_write_dec(n);
}

static void editor_render(void){
    console_clear_output();
    console_puts("Editing ");
    console_puts(path_current);
    console_puts("   Arrows move  Enter saves line  :w save  :q quit  Esc exit\n");
    console_puts("---------------------------------------------------------------\n");
    for(size_t i=0; i<line_count; i++){
        console_putc(i == current_line ? '>' : ' ');
        console_putc(' ');
        editor_line_no(i);
        console_puts(" | ");
        console_puts(lines[i]);
        console_putc('\n');
    }
    if(line_count == 0 || current_line == line_count){
        console_puts("> ");
        editor_line_no(current_line);
        console_puts(" | \n");
    }
}

static void editor_save(void){
    char text[1024];
    size_t pos = 0;
    for(size_t line=0; line<line_count; line++){
        for(size_t i=0; lines[line][i] && pos + 2 < sizeof(text); i++)
            text[pos++] = lines[line][i];
        if(pos + 1 < sizeof(text))
            text[pos++] = '\n';
    }
    text[pos] = 0;
    fs_write(path_current, text);
}

static void editor_load(const char* text){
    size_t line = 0;
    size_t col = 0;
    for(size_t i=0; i<EDITOR_MAX_LINES; i++)
        lines[i][0] = 0;
    if(text == 0 || text[0] == 0){
        line_count = 0;
        current_line = 0;
        return;
    }
    for(size_t i=0; text[i] && line < EDITOR_MAX_LINES; i++){
        if(text[i] == '\r')
            continue;
        if(text[i] == '\n'){
            lines[line][col] = 0;
            line++;
            col = 0;
            continue;
        }
        if(col + 1 < EDITOR_LINE_MAX)
            lines[line][col++] = text[i];
    }
    if(line < EDITOR_MAX_LINES && (col > 0 || text[0] == '\n')){
        lines[line][col] = 0;
        line++;
    }
    line_count = line;
    current_line = line_count;
}

void editor_open(const char* path){
    const char* text = "";
    str_copy(path_current, path, sizeof(path_current));
    fs_touch(path_current);
    fs_read(path_current, &text);
    editor_load(text);
    active = 1;
    shell_set_editor_mode(1);
    process_set_running("editor", 1);
    window_focus("editor");
    events_emit("process.start:editor");
    editor_render();
}

static void editor_commit_line(const char* line){
    if(current_line < line_count){
        str_copy(lines[current_line], line, EDITOR_LINE_MAX);
        if(current_line + 1 < line_count)
            current_line++;
        else
            current_line = line_count;
    } else if(line_count < EDITOR_MAX_LINES){
        str_copy(lines[line_count], line, EDITOR_LINE_MAX);
        line_count++;
        current_line = line_count;
    } else {
        console_puts("editor: line limit reached\n");
    }
    editor_save();
    editor_render();
}

void editor_move(int delta, size_t* len){
    size_t desired_col = shell_cursor();
    if(delta < 0){
        if(current_line > 0)
            current_line--;
    } else {
        if(current_line < line_count)
            current_line++;
    }
    editor_render();
    if(current_line < line_count)
        shell_set_input_text_cursor(lines[current_line], desired_col, len);
    else
        shell_set_input_text("", len);
}

void editor_move_horizontal(int delta, size_t* len){
    size_t cursor = shell_cursor();
    if(delta < 0){
        if(cursor > 0){
            shell_set_cursor(cursor - 1);
        } else if(current_line > 0){
            current_line--;
            editor_render();
            shell_set_input_text(lines[current_line], len);
            return;
        }
    } else {
        if(cursor < *len){
            shell_set_cursor(cursor + 1);
        } else if(current_line < line_count){
            current_line++;
            editor_render();
            if(current_line < line_count)
                shell_set_input_text_cursor(lines[current_line], 0, len);
            else
                shell_set_input_text("", len);
            return;
        }
    }
    shell_redraw(len);
}

void editor_eval(char* line, size_t* len){
    if(str_is(line, ".quit") || str_is(line, ".q") || str_is(line, ".exit") ||
       str_is(line, ":q") || str_is(line, "exit")){
        editor_close();
        return;
    }
    if(str_is(line, ".save") || str_is(line, ".w") || str_is(line, ":w")){
        editor_save();
        console_puts("saved ");
        console_puts(path_current);
        console_putc('\n');
        editor_render();
        shell_redraw(len);
        return;
    }
    if(str_is(line, ".show")){
        editor_render();
        shell_redraw(len);
        return;
    }
    if(str_is(line, ".clear")){
        line_count = 0;
        current_line = 0;
        editor_save();
        editor_render();
        shell_set_input_text("", len);
        console_puts("buffer cleared\n");
        return;
    }
    editor_commit_line(line);
    if(current_line < line_count)
        shell_set_input_text_cursor(lines[current_line], 0, len);
    else
        shell_set_input_text("", len);
}
