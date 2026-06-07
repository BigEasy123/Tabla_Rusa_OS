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

struct research_notebook_info {
    uint32_t id;
    uint32_t project_id;
    char name[40];
    uint32_t cells;
    char export_path[80];
};

struct research_citation_info {
    uint32_t id;
    uint32_t project_id;
    char key[32];
    char title[80];
    char source[80];
};

struct research_task_info {
    uint32_t id;
    uint32_t project_id;
    char title[48];
    char status[20];
    char owner[32];
};

struct research_timeline_info {
    uint32_t id;
    uint32_t project_id;
    char label[48];
    char note[96];
    uint32_t tick;
};

struct research_relation_info {
    uint32_t id;
    uint32_t project_id;
    char from[40];
    char relation[24];
    char to[40];
};

void research_init(void);
int research_project_create(const char* name, const char* description, const char* tags);
int research_project_open(const char* name);
int research_project_save(uint32_t project_id);
int research_add_note(uint32_t project_id, enum research_cell_type type, const char* title, const char* body);
int research_add_dataset(uint32_t project_id, const char* name, const char* path, const char* schema);
int research_register_dataset(uint32_t project_id, const char* name, const char* path, const char* schema);
int research_add_citation(uint32_t project_id, const char* key, const char* title, const char* source);
int research_track_experiment(uint32_t project_id, const char* name, const char* command);
int research_add_result(uint32_t project_id, const char* experiment, const char* summary);
int research_record_result(uint32_t project_id, const char* experiment, const char* summary);
int research_add_task(uint32_t project_id, const char* title, const char* owner);
int research_update_task(uint32_t task_id, const char* status);
int research_add_timeline(uint32_t project_id, const char* label, const char* note);
int research_add_relation(uint32_t project_id, const char* from, const char* relation, const char* to);
int research_export_latex(uint32_t project_id, char* out_path, uint32_t out_max);
int notebook_create(uint32_t project_id, const char* name);
int notebook_add_cell(uint32_t notebook_id, enum research_cell_type type, const char* title, const char* body);
int notebook_run_cell(uint32_t notebook_id, uint32_t cell_id, char* out, uint32_t out_max);
int notebook_export(uint32_t notebook_id, char* out_path, uint32_t out_max);
uint32_t research_project_list(struct research_project_info* out, uint32_t max);
uint32_t research_cell_list(uint32_t project_id, struct research_cell_info* out, uint32_t max);
uint32_t research_dataset_list(uint32_t project_id, struct research_dataset_info* out, uint32_t max);
uint32_t research_experiment_list(uint32_t project_id, struct research_experiment_info* out, uint32_t max);
uint32_t research_citation_list(uint32_t project_id, struct research_citation_info* out, uint32_t max);
uint32_t research_task_list(uint32_t project_id, struct research_task_info* out, uint32_t max);
uint32_t research_timeline_list(uint32_t project_id, struct research_timeline_info* out, uint32_t max);
uint32_t research_relation_list(uint32_t project_id, struct research_relation_info* out, uint32_t max);
uint32_t notebook_list(uint32_t project_id, struct research_notebook_info* out, uint32_t max);
void research_cmd(char* arg);

#endif
