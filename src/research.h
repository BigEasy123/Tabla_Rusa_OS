#ifndef RESEARCH_H
#define RESEARCH_H

#include <stdint.h>

enum research_cell_type {
    RESEARCH_CELL_MARKDOWN = 0,
    RESEARCH_CELL_MATH,
    RESEARCH_CELL_CODE,
    RESEARCH_CELL_RUSA,
    RESEARCH_CELL_PROOF,
    RESEARCH_CELL_DATASET,
    RESEARCH_CELL_RESULT,
    RESEARCH_CELL_CITATION
};

struct research_project_info {
    uint32_t id;
    char name[32];
    char description[96];
    char tags[64];
    uint32_t notebooks;
    uint32_t datasets;
    uint32_t experiments;
    uint32_t results;
    uint32_t modified_tick;
};

struct research_cell_info {
    uint32_t id;
    uint32_t project_id;
    enum research_cell_type type;
    char title[40];
    char body[128];
};

struct research_dataset_info {
    uint32_t id;
    uint32_t project_id;
    char name[40];
    char path[80];
    char schema[80];
};

struct research_experiment_info {
    uint32_t id;
    uint32_t project_id;
    char name[40];
    char status[24];
    char command[96];
};

void research_init(void);
int research_project_create(const char* name, const char* description, const char* tags);
int research_project_open(const char* name);
int research_project_save(uint32_t project_id);
int research_add_note(uint32_t project_id, enum research_cell_type type, const char* title, const char* body);
int research_register_dataset(uint32_t project_id, const char* name, const char* path, const char* schema);
int research_track_experiment(uint32_t project_id, const char* name, const char* command);
int research_record_result(uint32_t project_id, const char* experiment, const char* summary);
uint32_t research_project_list(struct research_project_info* out, uint32_t max);
uint32_t research_cell_list(uint32_t project_id, struct research_cell_info* out, uint32_t max);
uint32_t research_dataset_list(uint32_t project_id, struct research_dataset_info* out, uint32_t max);
uint32_t research_experiment_list(uint32_t project_id, struct research_experiment_info* out, uint32_t max);
void research_cmd(char* arg);

#endif
