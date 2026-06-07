#include "keyboard.h"
#include "console.h"
#include "fs.h"
#include "idt.h"
#include <stdint.h>

volatile int kb_buffer[128];
volatile int kb_len = 0;
static int shift_down = 0;
static int ctrl_down = 0;
static int alt_down = 0;
static int super_down = 0;
static int caps_on = 0;
static int num_on = 1;
static int scroll_on = 0;
static int extended = 0;
static int extended_seen = 0;
static uint32_t key_events = 0;
static uint8_t last_scancode = 0;
static char kb_type[48] = "PS/2 translated set-1 101/104-key";
static char kb_layout[16] = "us";
static char kb_repeat[16] = "normal";

/* Standard US keyboard map, ignoring shift/ctrl for now */
static const char keymap[128] = {
    0, 27,'1','2','3','4','5','6','7','8','9','0','-','=','\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',0,
    'a','s','d','f','g','h','j','k','l',';','\'','`',0,'\\',
    'z','x','c','v','b','n','m',',','.','/',0,'*',0,' ','\0'
    /* Fill remaining zeros if needed */
};

static const char shift_keymap[128] = {
    0, 27,'!','@','#','$','%','^','&','*','(',')','_','+','\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,
    'A','S','D','F','G','H','J','K','L',':','"','~',0,'|',
    'Z','X','C','V','B','N','M','<','>','?',0,'*',0,' ','\0'
};

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char lower_char(char c){
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int str_eq(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
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

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(max == 0) return;
    while(src && src[i] && i + 1 < max){
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

static void keyboard_write_descriptor(void){
    char text[384];
    char num[16];
    copy_text(text, "type=", sizeof(text));
    append_text(text, kb_type, sizeof(text));
    append_text(text, "\nlayout=", sizeof(text));
    append_text(text, kb_layout, sizeof(text));
    append_text(text, "\nlocks=caps:", sizeof(text));
    append_text(text, caps_on ? "on" : "off", sizeof(text));
    append_text(text, " num:", sizeof(text));
    append_text(text, num_on ? "on" : "off", sizeof(text));
    append_text(text, " scroll:", sizeof(text));
    append_text(text, scroll_on ? "on" : "off", sizeof(text));
    append_text(text, "\nmodifiers=shift ctrl alt super\nextended=", sizeof(text));
    append_text(text, extended_seen ? "seen" : "ready", sizeof(text));
    append_text(text, "\nrepeat=", sizeof(text));
    append_text(text, kb_repeat, sizeof(text));
    append_text(text, "\nevents=", sizeof(text));
    u32_text(key_events, num, sizeof(num));
    append_text(text, num, sizeof(text));
    append_text(text, "\n", sizeof(text));
    fs_mkdir("/system/input");
    fs_write("/system/input/keyboard.txt", text);
}

static void keyboard_sync_leds(void){
    /*
     * Keep lock state as the OS source of truth. Direct LED writes through
     * 0xED can leave ACK bytes in the QEMU/curses input stream during boot,
     * so the physical LED sync waits until we have a fuller keyboard driver.
     */
    keyboard_write_descriptor();
}

static void keyboard_set_lock(const char* name, const char* mode){
    int* target = 0;
    if(str_eq(name, "caps")) target = &caps_on;
    else if(str_eq(name, "num")) target = &num_on;
    else if(str_eq(name, "scroll")) target = &scroll_on;
    if(!target){
        console_puts("keyboard: lock must be caps|num|scroll\n");
        return;
    }
    if(str_eq(mode, "on")) *target = 1;
    else if(str_eq(mode, "off")) *target = 0;
    else *target = !*target;
    keyboard_sync_leds();
}

static void keyboard_print_status(void){
    console_puts("keyboard type=");
    console_puts(kb_type);
    console_puts(" layout=");
    console_puts(kb_layout);
    console_puts(" repeat=");
    console_puts(kb_repeat);
    console_puts(" locks caps=");
    console_puts(caps_on ? "on" : "off");
    console_puts(" num=");
    console_puts(num_on ? "on" : "off");
    console_puts(" scroll=");
    console_puts(scroll_on ? "on" : "off");
    console_puts(" mods shift=");
    console_puts(shift_down ? "down" : "up");
    console_puts(" ctrl=");
    console_puts(ctrl_down ? "down" : "up");
    console_puts(" alt=");
    console_puts(alt_down ? "down" : "up");
    console_puts(" super=");
    console_puts(super_down ? "down" : "up");
    console_puts(" extended=");
    console_puts(extended_seen ? "seen" : "ready");
    console_puts(" events=");
    console_write_dec(key_events);
    console_puts(" last_scancode=");
    console_write_hex(last_scancode);
    console_putc('\n');
}

static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ __volatile__("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ __volatile__("push %0; popf" : : "r"(flags) : "memory", "cc");
}

void keyboard_handler(void) {
    uint8_t sc = inb(KB_DATA);
    last_scancode = sc;
    key_events++;
    if(sc == 0xFA || sc == 0xFE)
        return;
    if(sc == 0xE0){
        extended = 1;
        extended_seen = 1;
        return;
    }
    if(extended){
        extended = 0;
        int release = (sc & 0x80) != 0;
        uint8_t code = sc & 0x7F;
        if(code == 0x1D){ ctrl_down = !release; return; }
        if(code == 0x38){ alt_down = !release; return; }
        if(code == 0x5B || code == 0x5C){
            super_down = !release;
            if(!release && kb_len < 127)
                kb_buffer[kb_len++] = code == 0x5B ? KB_KEY_SUPER_LEFT : KB_KEY_SUPER_RIGHT;
            return;
        }
        if(release)
            return;
        int key = 0;
        if(code == 0x48) key = KB_KEY_UP;
        else if(code == 0x50) key = KB_KEY_DOWN;
        else if(code == 0x4B) key = KB_KEY_LEFT;
        else if(code == 0x4D) key = KB_KEY_RIGHT;
        else if(code == 0x49) key = KB_KEY_PAGE_UP;
        else if(code == 0x51) key = KB_KEY_PAGE_DOWN;
        else if(code == 0x47) key = KB_KEY_HOME;
        else if(code == 0x4F) key = KB_KEY_END;
        else if(code == 0x52) key = KB_KEY_INSERT;
        else if(code == 0x53) key = KB_KEY_DELETE;
        else if(code == 0x1C) key = '\n';
        else if(code == 0x35) key = shift_down ? '?' : '/';
        if(key && kb_len < 127)
            kb_buffer[kb_len++] = key;
        keyboard_write_descriptor();
        return;
    }

    if(sc == 0x1D){ ctrl_down = 1; return; }
    if(sc == 0x9D){ ctrl_down = 0; return; }
    if(sc == 0x38){ alt_down = 1; return; }
    if(sc == 0xB8){ alt_down = 0; return; }
    if(sc == 0x2A || sc == 0x36){
        shift_down = 1;
        return;
    }
    if(sc == 0xAA || sc == 0xB6){
        shift_down = 0;
        return;
    }
    if(sc == 0x3A){
        caps_on = !caps_on;
        keyboard_sync_leds();
        return;
    }
    if(sc == 0x45){
        num_on = !num_on;
        keyboard_sync_leds();
        return;
    }
    if(sc == 0x46){
        scroll_on = !scroll_on;
        keyboard_sync_leds();
        return;
    }
    if (sc & 0x80) return;

    int special = 0;
    if(sc >= 0x3B && sc <= 0x44) special = KB_KEY_F1 + (sc - 0x3B);
    else if(sc == 0x57) special = KB_KEY_F11;
    else if(sc == 0x58) special = KB_KEY_F12;
    if(special){
        if(kb_len < 127) kb_buffer[kb_len++] = special;
        keyboard_write_descriptor();
        return;
    }

    if(sc >= 0x47 && sc <= 0x53){
        int key = 0;
        if(num_on){
            if(sc == 0x47) key = '7';
            else if(sc == 0x48) key = '8';
            else if(sc == 0x49) key = '9';
            else if(sc == 0x4A) key = '-';
            else if(sc == 0x4B) key = '4';
            else if(sc == 0x4C) key = '5';
            else if(sc == 0x4D) key = '6';
            else if(sc == 0x4E) key = '+';
            else if(sc == 0x4F) key = '1';
            else if(sc == 0x50) key = '2';
            else if(sc == 0x51) key = '3';
            else if(sc == 0x52) key = '0';
            else if(sc == 0x53) key = '.';
        } else {
            if(sc == 0x47) key = KB_KEY_HOME;
            else if(sc == 0x48) key = KB_KEY_UP;
            else if(sc == 0x49) key = KB_KEY_PAGE_UP;
            else if(sc == 0x4B) key = KB_KEY_LEFT;
            else if(sc == 0x4D) key = KB_KEY_RIGHT;
            else if(sc == 0x4F) key = KB_KEY_END;
            else if(sc == 0x50) key = KB_KEY_DOWN;
            else if(sc == 0x51) key = KB_KEY_PAGE_DOWN;
            else if(sc == 0x52) key = KB_KEY_INSERT;
            else if(sc == 0x53) key = KB_KEY_DELETE;
        }
        if(key && kb_len < 127) kb_buffer[kb_len++] = key;
        keyboard_write_descriptor();
        return;
    }

    char c = shift_down ? shift_keymap[sc] : keymap[sc];
    if(caps_on){
        if(!shift_down && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        else if(shift_down && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    }
    if (!c) return;
    if (kb_len < 127) kb_buffer[kb_len++] = (int)c;
    keyboard_write_descriptor();
}

int kb_read_key(void) {
    uint32_t flags = irq_save();
    if (kb_len == 0) {
        irq_restore(flags);
        return 0;
    }

    int c = kb_buffer[0];
    for (int i=0; i<kb_len-1; i++) kb_buffer[i] = kb_buffer[i+1];
    kb_len--;
    irq_restore(flags);
    return c;
}

char kb_read_char(void) {
    int key = kb_read_key();
    if(key > 0xFF)
        return 0;
    return (char)key;
}

int keyboard_ctrl_down(void){
    return ctrl_down;
}

int keyboard_alt_down(void){
    return alt_down || super_down;
}

int keyboard_caps_on(void){ return caps_on; }
int keyboard_num_on(void){ return num_on; }
int keyboard_scroll_on(void){ return scroll_on; }
const char* keyboard_type(void){ return kb_type; }
const char* keyboard_layout(void){ return kb_layout; }

void keyboard_install(void) {
    // Unmask keyboard IRQ1 in PIC
    uint8_t mask = inb(0x21);
    mask &= ~0x02;           // clear bit 1 to enable IRQ1
    outb(0x21, mask);
    keyboard_sync_leds();
    fs_append_line("/var/log/system.log", "keyboard: PS/2 translated set-1 controller online");
}

void keyboard_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "status")){
        keyboard_print_status();
    } else if(str_eq(action, "detect")){
        copy_text(kb_type, extended_seen ? "PS/2 translated set-1 extended 104-key" :
                                           "PS/2 translated set-1 101/104-key", sizeof(kb_type));
        keyboard_write_descriptor();
        keyboard_print_status();
    } else if(str_eq(action, "caps") || str_eq(action, "num") || str_eq(action, "scroll")){
        const char* mode = first_arg(rest, &rest);
        keyboard_set_lock(action, mode[0] ? mode : "toggle");
        keyboard_print_status();
    } else if(str_eq(action, "layout")){
        const char* layout = first_arg(rest, &rest);
        if(layout[0] == 0){
            console_puts("layout=");
            console_puts(kb_layout);
            console_puts(" available=us\n");
        } else if(str_eq(layout, "us")){
            copy_text(kb_layout, "us", sizeof(kb_layout));
            keyboard_write_descriptor();
            console_puts("keyboard layout=us\n");
        } else {
            console_puts("keyboard: only us layout is mapped right now\n");
        }
    } else if(str_eq(action, "repeat")){
        const char* mode = first_arg(rest, &rest);
        if(str_eq(mode, "slow") || str_eq(mode, "normal") || str_eq(mode, "fast")){
            copy_text(kb_repeat, mode, sizeof(kb_repeat));
            keyboard_write_descriptor();
        }
        console_puts("repeat=");
        console_puts(kb_repeat);
        console_putc('\n');
    } else if(str_eq(action, "keys")){
        console_puts("keys: letters numbers punctuation arrows home/end pageup/pagedown insert/delete f1-f12 keypad caps num scroll ctrl alt super\n");
    } else {
        console_puts("usage: keyboard status|detect|caps on|off|toggle|num on|off|toggle|scroll on|off|toggle|layout us|repeat slow|normal|fast|keys\n");
    }
}
