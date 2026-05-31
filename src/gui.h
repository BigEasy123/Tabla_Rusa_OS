#ifndef GUI_H
#define GUI_H

void gui_init(void);
int gui_is_running(void);
void gui_enter_desktop(void);
void gui_tick(void);
void gui_handle_key(int key);
void gui_handle_click(uint32_t x, uint32_t y);
void gui_handle_drag(uint32_t x, uint32_t y, uint32_t buttons);
int gui_handle_scroll(int amount);
int gui_key_captures(int key);
int gui_take_terminal_request(void);
void gui_active_terminal_command(char* out, uint32_t max);
void gui_cmd(char* arg);
int gui_is_desktop_visible(void);

#endif
