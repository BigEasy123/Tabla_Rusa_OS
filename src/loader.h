#ifndef LOADER_H
#define LOADER_H

void loader_init(void);
void loader_cmd(char* arg);
int loader_run(const char* path, const char* args);

#endif
