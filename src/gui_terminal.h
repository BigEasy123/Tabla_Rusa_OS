#ifndef GUI_TERMINAL_H
#define GUI_TERMINAL_H

#include <stdint.h>

void gui_terminal_seed(void);
void gui_terminal_set_focused(int focused);
int gui_terminal_focused(void);
const char* gui_terminal_view(void);
void gui_terminal_set_view(const char* text);
const char* gui_terminal_input(void);
uint32_t gui_terminal_cursor(void);
uint32_t gui_terminal_count(void);
uint32_t gui_terminal_top(void);
void gui_terminal_set_top(uint32_t top);
const char* gui_terminal_line(uint32_t index);
int gui_terminal_line_selected(uint32_t index);
int gui_terminal_clipboard_has_text(void);
void gui_terminal_input_text(char* out, uint32_t max);
void gui_terminal_clear_input(void);
void gui_terminal_select_range(uint32_t start, uint32_t end);
void gui_terminal_copy_selection(void);
void gui_terminal_paste_clipboard(void);
void gui_terminal_clear(void);
int gui_terminal_handle_key(int key);
void gui_terminal_scroll(int amount);

#endif
