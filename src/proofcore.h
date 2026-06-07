#ifndef PROOFCORE_H
#define PROOFCORE_H

#include <stdint.h>

enum proof_mode {
    PROOF_MODE_STRICT = 0,
    PROOF_MODE_GUIDED,
    PROOF_MODE_EDUCATIONAL,
    PROOF_MODE_INFORMAL
};

enum proof_step_status {
    PROOF_STEP_VALID = 0,
    PROOF_STEP_INVALID,
    PROOF_STEP_INCOMPLETE,
    PROOF_STEP_UNKNOWN
};

struct proof_state_info {
    uint32_t id;
    char name[32];
    char goal[96];
    enum proof_mode mode;
    uint32_t assumptions;
    uint32_t steps;
    enum proof_step_status status;
    char current[96];
    char explanation[128];
    char suggestion[96];
};

struct proof_step_info {
    uint32_t id;
    uint32_t proof_id;
    char rule[24];
    char expression[96];
    enum proof_step_status status;
    char explanation[128];
};

void proofcore_init(void);
int proof_create(const char* name, const char* goal, enum proof_mode mode);
int proof_add_assumption(uint32_t proof_id, const char* assumption);
int proof_add_step(uint32_t proof_id, const char* rule, const char* expression);
int proof_get_state(uint32_t proof_id, struct proof_state_info* out);
uint32_t proof_list_states(struct proof_state_info* out, uint32_t max);
uint32_t proof_list_steps(uint32_t proof_id, struct proof_step_info* out, uint32_t max);
int proof_replay(uint32_t proof_id);
int proof_export_text(uint32_t proof_id, char* out, uint32_t max);
const char* proof_status_text(enum proof_step_status status);
void proof_cmd(char* arg);

#endif
