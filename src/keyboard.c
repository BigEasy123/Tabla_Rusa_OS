#include "keyboard.h"
#include "idt.h"
#include <stdint.h>

volatile int kb_buffer[128];
volatile int kb_len = 0;
static int shift_down = 0;
static int caps_on = 0;
static int extended = 0;

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
    if(sc == 0xE0){
        extended = 1;
        return;
    }
    if(extended){
        extended = 0;
        if(sc & 0x80)
            return;
        int key = 0;
        if(sc == 0x48) key = KB_KEY_UP;
        else if(sc == 0x50) key = KB_KEY_DOWN;
        else if(sc == 0x4B) key = KB_KEY_LEFT;
        else if(sc == 0x4D) key = KB_KEY_RIGHT;
        else if(sc == 0x49) key = KB_KEY_PAGE_UP;
        else if(sc == 0x51) key = KB_KEY_PAGE_DOWN;
        if(key && kb_len < 127)
            kb_buffer[kb_len++] = key;
        return;
    }

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
        return;
    }
    if (sc & 0x80) return;

    char c = shift_down ? shift_keymap[sc] : keymap[sc];
    if(caps_on){
        if(!shift_down && c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        else if(shift_down && c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
    }
    if (!c) return;
    if (kb_len < 127) kb_buffer[kb_len++] = (int)c;
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

void keyboard_install(void) {
    // Unmask keyboard IRQ1 in PIC
    uint8_t mask = inb(0x21);
    mask &= ~0x02;           // clear bit 1 to enable IRQ1
    outb(0x21, mask);
}
