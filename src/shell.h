#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>

void shell_intro(void);
void shell_structure_cmd(void);
void shell_set_eval_handler(void (*handler)(char* line));
void shell_eval(char* line);
void shell_session_init(void);
void shell_set_editor_mode(int active);
void shell_prompt(void);
void shell_redraw(size_t* len);
void shell_set_input_text(const char* text, size_t* len);
void shell_set_input_text_cursor(const char* text, size_t cursor, size_t* len);
void shell_insert_char(char c, size_t* len);
void shell_backspace(size_t* len);
void shell_echo_command(const char* line);
void shell_history_add(const char* line);
void shell_history_cmd(void);
int shell_history_prev(size_t* len);
int shell_history_next(size_t* len);
char* shell_input_buffer(void);
size_t shell_cursor(void);
void shell_set_cursor(size_t cursor);
void shell_reset_input(size_t* len);

#endif
