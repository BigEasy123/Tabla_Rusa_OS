#ifndef WINDOW_H
#define WINDOW_H

#define WINDOW_MAX 4

struct window_info {
    const char* name;
    int focused;
    int x;
    int y;
    int w;
    int h;
    const char* surface;
};

void window_init(void);
struct window_info* window_find(const char* name);
void window_focus(const char* name);
void window_focus_next(void);
void window_move(const char* name, int x, int y);
void window_resize(const char* name, int w, int h);
void window_list(void);
const struct window_info* window_focused(void);

#endif
