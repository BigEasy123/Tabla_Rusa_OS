#include "window.h"
#include "console.h"

static struct window_info windows[WINDOW_MAX] = {
    {"shell", 1, 0, 0, "vga-text"},
    {"editor", 0, 0, 1, "vga-text"},
    {"network", 0, 40, 1, "planned"},
    {"inspector", 0, 40, 12, "planned"}
};

static int win_is(const char* a, const char* b){
    while(*a && *b){
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if(ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

void window_init(void){
    for(int i=0; i<WINDOW_MAX; i++)
        windows[i].focused = 0;
    windows[0].focused = 1;
}

struct window_info* window_find(const char* name){
    for(int i=0; i<WINDOW_MAX; i++)
        if(win_is(windows[i].name, name))
            return &windows[i];
    return 0;
}

void window_focus(const char* name){
    struct window_info* win = window_find(name);
    if(!win)
        return;
    for(int i=0; i<WINDOW_MAX; i++)
        windows[i].focused = 0;
    win->focused = 1;
}

void window_focus_next(void){
    int current = 0;
    for(int i=0; i<WINDOW_MAX; i++)
        if(windows[i].focused)
            current = i;
    for(int i=0; i<WINDOW_MAX; i++)
        windows[i].focused = 0;
    windows[(current + 1) % WINDOW_MAX].focused = 1;
}

void window_move(const char* name, int x, int y){
    struct window_info* win = window_find(name);
    if(!win)
        return;
    win->x = x;
    win->y = y;
}

void window_list(void){
    for(int i=0; i<WINDOW_MAX; i++){
        console_puts(windows[i].focused ? "[focus] " : "[     ] ");
        console_puts(windows[i].name);
        console_puts(" at ");
        console_write_dec((uint32_t)windows[i].x);
        console_putc(',');
        console_write_dec((uint32_t)windows[i].y);
        console_puts(" surface=");
        console_puts(windows[i].surface);
        console_putc('\n');
    }
}

const struct window_info* window_focused(void){
    for(int i=0; i<WINDOW_MAX; i++)
        if(windows[i].focused)
            return &windows[i];
    return &windows[0];
}
