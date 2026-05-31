#ifndef EDITOR_H
#define EDITOR_H

#include <stddef.h>

void editor_init(void);
int editor_is_active(void);
void editor_open(const char* path);
void editor_open_at(const char* path, uint32_t line, uint32_t col, const char* message);
void editor_close(void);
void editor_eval(char* line, size_t* len);
void editor_move(int delta, size_t* len);
void editor_move_horizontal(int delta, size_t* len);
const char* editor_path_current(void);

#endif
