#include "console.h"
#include "keyboard.h"
#include "string_utils.h" // minimal strcmp/strncmp/strlen

// Simple command processor
void process_command(const char* line) {
    if (strcmp(line, "help") == 0) {
        console_write("Available commands: help, echo\n");
    } 
    else if (strncmp(line, "echo ", 5) == 0) {
        console_write(line + 5);  // print everything after "echo "
        console_putc('\n');
    } 
    else if (strlen(line) == 0) {
        // do nothing for empty line
    }
    else {
        console_write("Unknown command\n");
    }
}

void kernel_main(void) {
    console_init();
    keyboard_init();

    console_write("Tabla Rusa OS terminal! Type 'help'\n");

    while (1) {
        keyboard_handler();       // poll/process keyboard input

        // If Enter was pressed, process the command line
        if (keyboard_line_ready()) {
            process_command(keyboard_get_line());
            keyboard_clear_line_ready();
        }
    }
}
