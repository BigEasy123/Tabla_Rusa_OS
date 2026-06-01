#ifndef LANG_H
#define LANG_H

void lang_init(void);
void lang_set_call_handler(void (*handler)(char* command));
int lang_run_source(const char* source, const char* origin, const char* args);
int lang_run_file(const char* path, const char* args);
int lang_last_diag(char* origin, unsigned int origin_max,
                   unsigned int* line, unsigned int* col,
                   char* title, unsigned int title_max,
                   char* detail, unsigned int detail_max);
void lang_cmd(char* arg);

#endif
