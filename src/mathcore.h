#ifndef MATHCORE_H
#define MATHCORE_H

#include <stdint.h>

enum theorem_status {
    THEOREM_STATED = 0,
    THEOREM_PROOF_SKETCH,
    THEOREM_FORMALLY_CHECKED,
    THEOREM_COMPUTATIONALLY_VERIFIED,
    THEOREM_ASSUMED_AXIOM,
    THEOREM_EXTERNAL_REFERENCE
};

struct math_plugin_info {
    uint32_t id;
    char name[32];
    char version[16];
    char capabilities[96];
    char commands[96];
    char ui_panel[32];
};

struct theorem_info {
    uint32_t id;
    char name[48];
    char field[32];
    char statement[128];
    enum theorem_status status;
    char source_plugin[32];
};

struct algorithm_info {
    uint32_t id;
    char name[48];
    char field[32];
    char input_types[64];
    char output_types[64];
    char complexity[32];
    char status[32];
    char source_plugin[32];
};

struct math_object_type_info {
    uint32_t id;
    char name[32];
    char display[64];
    char latex_hint[64];
    char source_plugin[32];
};

void mathcore_init(void);
int math_plugin_register(const char* name, const char* version, const char* capabilities,
                         const char* commands, const char* ui_panel);
int math_plugin_find(const char* name, struct math_plugin_info* out);
uint32_t math_plugin_list(struct math_plugin_info* out, uint32_t max);
int math_plugin_dispatch(const char* plugin, const char* command, char* out, uint32_t out_max);
int math_plugin_register_theorem(const char* plugin, const char* name, const char* field,
                                 const char* statement, enum theorem_status status);
int math_plugin_register_algorithm(const char* plugin, const char* name, const char* field,
                                   const char* input_types, const char* output_types,
                                   const char* complexity, const char* status);
int math_plugin_register_object_type(const char* plugin, const char* name,
                                     const char* display, const char* latex_hint);
uint32_t math_theorem_list(struct theorem_info* out, uint32_t max);
uint32_t math_algorithm_list(struct algorithm_info* out, uint32_t max);
uint32_t math_object_type_list(struct math_object_type_info* out, uint32_t max);
int math_plugin_run_tests(const char* plugin);
void mathcore_cmd(char* arg);

#endif
