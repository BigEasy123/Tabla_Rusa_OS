#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "mathcore.h"
#include "process.h"
#include "proofcore.h"

#define PROOF_MAX 6
#define PROOF_STEP_MAX 32
#define PROOF_ASSUMPTION_MAX 8

struct proof_state {
    int used;
    struct proof_state_info info;
    char assumptions[PROOF_ASSUMPTION_MAX][96];
};

static struct proof_state proofs[PROOF_MAX];
static struct proof_step_info steps[PROOF_STEP_MAX];
static uint32_t next_proof_id = 1;
static uint32_t next_step_id = 1;

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c + 32);
    return c;
}

static int str_eq(const char* a, const char* b){
    uint32_t i = 0;
    if(!a || !b) return 0;
    while(a[i] && b[i]){
        if(lower_char(a[i]) != lower_char(b[i])) return 0;
        i++;
    }
    return a[i] == 0 && b[i] == 0;
}

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static const char* first_arg(const char* in, char* out, uint32_t max){
    uint32_t i = 0;
    while(in && is_space(*in)) in++;
    while(in && *in && !is_space(*in)){
        if(i + 1 < max) out[i++] = *in;
        in++;
    }
    out[i] = 0;
    while(in && is_space(*in)) in++;
    return in ? in : "";
}

static uint32_t parse_u32(const char* s){
    uint32_t v = 0;
    while(s && is_space(*s)) s++;
    while(s && *s >= '0' && *s <= '9'){
        v = v * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(!dst || max == 0) return;
    if(!src) src = "";
    while(src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void append_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    uint32_t j = 0;
    if(!dst || !src || max == 0) return;
    while(dst[i] && i < max) i++;
    while(src[j] && i + 1 < max)
        dst[i++] = src[j++];
    dst[i] = 0;
}

static int text_has(const char* haystack, const char* needle){
    uint32_t i = 0;
    if(!haystack || !needle) return 0;
    if(!needle[0]) return 1;
    while(haystack[i]){
        uint32_t j = 0;
        while(haystack[i + j] && needle[j] && lower_char(haystack[i + j]) == lower_char(needle[j])) j++;
        if(!needle[j]) return 1;
        i++;
    }
    return 0;
}

static int proof_index(uint32_t id){
    uint32_t i;
    for(i = 0; i < PROOF_MAX; i++)
        if(proofs[i].used && proofs[i].info.id == id) return (int)i;
    return -1;
}

static int proof_has_fact(struct proof_state* proof, const char* fact){
    uint32_t i;
    if(!proof || !fact) return 0;
    for(i = 0; i < proof->info.assumptions && i < PROOF_ASSUMPTION_MAX; i++)
        if(str_eq(proof->assumptions[i], fact)) return 1;
    for(i = 0; i < PROOF_STEP_MAX; i++)
        if(steps[i].proof_id == proof->info.id && steps[i].status == PROOF_STEP_VALID &&
           str_eq(steps[i].expression, fact)) return 1;
    return 0;
}

static void status_message(enum proof_step_status status, char* explanation, uint32_t explanation_max,
                           char* suggestion, uint32_t suggestion_max){
    if(status == PROOF_STEP_VALID){
        copy_text(explanation, "The step follows from the current proof context.", explanation_max);
        copy_text(suggestion, "Continue toward the goal or finish with qed.", suggestion_max);
    } else if(status == PROOF_STEP_INCOMPLETE){
        copy_text(explanation, "The step is reasonable but leaves an unfinished proof obligation.", explanation_max);
        copy_text(suggestion, "Add the missing premise, subgoal, or exact goal statement.", suggestion_max);
    } else if(status == PROOF_STEP_UNKNOWN){
        copy_text(explanation, "The proof checker does not know this rule yet.", explanation_max);
        copy_text(suggestion, "Try assume, exact, modus, split, intro, or qed.", suggestion_max);
    } else {
        copy_text(explanation, "The step does not follow from known assumptions or previous valid steps.", explanation_max);
        copy_text(suggestion, "Check the expression or add the needed assumption first.", suggestion_max);
    }
}

static enum proof_step_status check_step(struct proof_state* proof, const char* rule, const char* expr){
    char left[48];
    char right[48];
    const char* rest;
    if(!proof || !rule || !expr) return PROOF_STEP_INVALID;
    if(str_eq(rule, "assume"))
        return PROOF_STEP_VALID;
    if(str_eq(rule, "exact"))
        return (proof_has_fact(proof, expr) || str_eq(proof->info.goal, expr)) ? PROOF_STEP_VALID : PROOF_STEP_INVALID;
    if(str_eq(rule, "intro"))
        return text_has(proof->info.goal, "->") ? PROOF_STEP_INCOMPLETE : PROOF_STEP_INVALID;
    if(str_eq(rule, "split"))
        return text_has(proof->info.goal, "and") ? PROOF_STEP_INCOMPLETE : PROOF_STEP_INVALID;
    if(str_eq(rule, "modus")){
        rest = first_arg(expr, left, sizeof(left));
        first_arg(rest, right, sizeof(right));
        return (proof_has_fact(proof, left) && text_has(right, "->")) ? PROOF_STEP_VALID : PROOF_STEP_INVALID;
    }
    if(str_eq(rule, "rewrite") || str_eq(rule, "simplify") || str_eq(rule, "apply") || str_eq(rule, "cases"))
        return PROOF_STEP_INCOMPLETE;
    if(str_eq(rule, "qed"))
        return (proof_has_fact(proof, proof->info.goal) || str_eq(proof->info.current, proof->info.goal)) ?
               PROOF_STEP_VALID : PROOF_STEP_INVALID;
    return proof->info.mode == PROOF_MODE_INFORMAL ? PROOF_STEP_UNKNOWN : PROOF_STEP_INVALID;
}

const char* proof_status_text(enum proof_step_status status){
    if(status == PROOF_STEP_VALID) return "valid";
    if(status == PROOF_STEP_INVALID) return "invalid";
    if(status == PROOF_STEP_INCOMPLETE) return "incomplete";
    return "unknown";
}

void proofcore_init(void){
    fs_mkdir("/share/proof");
    fs_write("/share/proof/README",
        "ProofCore tracks proof goals, assumptions, steps, status, explanations, suggestions, replay, and export.\n"
        "Modes: strict guided educational informal. Only valid checked steps count as formal proof progress.\n");
    fs_write("/share/proof/tactics",
        "assume\nintro\nexact\napply\nrewrite\nsimplify\nsplit\ncases\nmodus\nqed\n");
    math_plugin_register_algorithm("logic-proof", "ProofCore step checker", "logic",
        "proof state and proof step", "valid invalid incomplete unknown", "linear in current proof steps", "implemented");
    fs_append_line("/var/log/system.log", "proofcore: proof state engine online");
}

int proof_create(const char* name, const char* goal, enum proof_mode mode){
    uint32_t i;
    if(!goal || !goal[0]) return -1;
    for(i = 0; i < PROOF_MAX; i++){
        if(!proofs[i].used){
            proofs[i].used = 1;
            proofs[i].info.id = next_proof_id++;
            copy_text(proofs[i].info.name, name && name[0] ? name : "proof", sizeof(proofs[i].info.name));
            copy_text(proofs[i].info.goal, goal, sizeof(proofs[i].info.goal));
            proofs[i].info.mode = mode;
            proofs[i].info.assumptions = 0;
            proofs[i].info.steps = 0;
            proofs[i].info.status = PROOF_STEP_INCOMPLETE;
            copy_text(proofs[i].info.current, goal, sizeof(proofs[i].info.current));
            copy_text(proofs[i].info.explanation, "Proof goal created.", sizeof(proofs[i].info.explanation));
            copy_text(proofs[i].info.suggestion, "Add assumptions or proof steps.", sizeof(proofs[i].info.suggestion));
            return (int)proofs[i].info.id;
        }
    }
    return -1;
}

int proof_add_assumption(uint32_t proof_id, const char* assumption){
    int idx = proof_index(proof_id);
    uint32_t n;
    if(idx < 0 || !assumption || !assumption[0]) return -1;
    n = proofs[idx].info.assumptions;
    if(n >= PROOF_ASSUMPTION_MAX) return -1;
    copy_text(proofs[idx].assumptions[n], assumption, sizeof(proofs[idx].assumptions[n]));
    proofs[idx].info.assumptions++;
    copy_text(proofs[idx].info.explanation, "Assumption added to the context.", sizeof(proofs[idx].info.explanation));
    copy_text(proofs[idx].info.suggestion, "Use exact or modus when the goal follows.", sizeof(proofs[idx].info.suggestion));
    return 0;
}

int proof_add_step(uint32_t proof_id, const char* rule, const char* expression){
    uint32_t i;
    int idx = proof_index(proof_id);
    enum proof_step_status status;
    if(idx < 0 || !rule || !rule[0]) return -1;
    for(i = 0; i < PROOF_STEP_MAX; i++){
        if(steps[i].id == 0){
            steps[i].id = next_step_id++;
            steps[i].proof_id = proof_id;
            copy_text(steps[i].rule, rule, sizeof(steps[i].rule));
            copy_text(steps[i].expression, expression, sizeof(steps[i].expression));
            steps[i].status = PROOF_STEP_UNKNOWN;
            status = check_step(&proofs[idx], rule, expression);
            steps[i].status = status;
            status_message(status, steps[i].explanation, sizeof(steps[i].explanation),
                           proofs[idx].info.suggestion, sizeof(proofs[idx].info.suggestion));
            copy_text(proofs[idx].info.explanation, steps[i].explanation, sizeof(proofs[idx].info.explanation));
            if(status == PROOF_STEP_VALID)
                copy_text(proofs[idx].info.current, expression, sizeof(proofs[idx].info.current));
            proofs[idx].info.status = status;
            proofs[idx].info.steps++;
            jobs_account("proof-worker", 2);
            process_set_compute("compute", "proofcore", 86, 50);
            return (int)steps[i].id;
        }
    }
    return -1;
}

int proof_get_state(uint32_t proof_id, struct proof_state_info* out){
    int idx = proof_index(proof_id);
    if(idx < 0) return -1;
    if(out) *out = proofs[idx].info;
    return 0;
}

uint32_t proof_list_states(struct proof_state_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < PROOF_MAX; i++){
        if(proofs[i].used){
            if(out && n < max) out[n] = proofs[i].info;
            n++;
        }
    }
    return n;
}

uint32_t proof_list_steps(uint32_t proof_id, struct proof_step_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < PROOF_STEP_MAX; i++){
        if(steps[i].id && steps[i].proof_id == proof_id){
            if(out && n < max) out[n] = steps[i];
            n++;
        }
    }
    return n;
}

int proof_replay(uint32_t proof_id){
    struct proof_step_info list[PROOF_STEP_MAX];
    uint32_t i, n = proof_list_steps(proof_id, list, PROOF_STEP_MAX);
    if(proof_index(proof_id) < 0) return -1;
    for(i = 0; i < n; i++)
        if(list[i].status == PROOF_STEP_INVALID)
            return -1;
    jobs_account("proof-replay", n ? n : 1);
    return 0;
}

int proof_export_text(uint32_t proof_id, char* out, uint32_t max){
    int idx = proof_index(proof_id);
    struct proof_step_info list[PROOF_STEP_MAX];
    uint32_t i, n;
    if(idx < 0 || !out || max == 0) return -1;
    copy_text(out, "Proof ", max);
    append_text(out, proofs[idx].info.name, max);
    append_text(out, "\nGoal: ", max);
    append_text(out, proofs[idx].info.goal, max);
    append_text(out, "\nSteps:\n", max);
    n = proof_list_steps(proof_id, list, PROOF_STEP_MAX);
    for(i = 0; i < n; i++){
        append_text(out, "- ", max);
        append_text(out, list[i].rule, max);
        append_text(out, " ", max);
        append_text(out, list[i].expression, max);
        append_text(out, " [", max);
        append_text(out, proof_status_text(list[i].status), max);
        append_text(out, "]\n", max);
    }
    return 0;
}

void proof_cmd(char* arg){
    char action[16];
    arg = (char*)first_arg(arg, action, sizeof(action));
    if(action[0] == 0 || str_eq(action, "list")){
        struct proof_state_info list[PROOF_MAX];
        uint32_t i, n = proof_list_states(list, PROOF_MAX);
        for(i = 0; i < n; i++){
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" goal=");
            console_puts(list[i].goal);
            console_puts(" status=");
            console_puts(proof_status_text(list[i].status));
            console_putc('\n');
        }
        if(n == 0) console_puts("proof: no active proofs\n");
    } else if(str_eq(action, "new")){
        char name[32];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        id = proof_create(name, arg, PROOF_MODE_GUIDED);
        console_puts("proof ");
        console_write_dec((uint32_t)id);
        console_putc('\n');
    } else if(str_eq(action, "assume")){
        char id_text[12];
        arg = (char*)first_arg(arg, id_text, sizeof(id_text));
        console_puts(proof_add_assumption(parse_u32(id_text), arg) == 0 ? "assumption added\n" : "assumption failed\n");
    } else if(str_eq(action, "step")){
        char id_text[12], rule[24];
        int step;
        arg = (char*)first_arg(arg, id_text, sizeof(id_text));
        arg = (char*)first_arg(arg, rule, sizeof(rule));
        step = proof_add_step(parse_u32(id_text), rule, arg);
        console_puts("step ");
        console_write_dec((uint32_t)step);
        console_putc('\n');
    } else if(str_eq(action, "show")){
        char id_text[12];
        struct proof_state_info state;
        first_arg(arg, id_text, sizeof(id_text));
        if(proof_get_state(parse_u32(id_text), &state) != 0){
            console_puts("proof: not found\n");
        } else {
            console_puts(state.name);
            console_puts(" goal=");
            console_puts(state.goal);
            console_puts(" status=");
            console_puts(proof_status_text(state.status));
            console_puts("\nplain english: ");
            console_puts(state.explanation);
            console_puts("\nsuggestion: ");
            console_puts(state.suggestion);
            console_putc('\n');
        }
    } else if(str_eq(action, "export")){
        char id_text[12], out[512];
        first_arg(arg, id_text, sizeof(id_text));
        if(proof_export_text(parse_u32(id_text), out, sizeof(out)) == 0) console_puts(out);
        else console_puts("proof: not found\n");
    } else if(str_eq(action, "replay")){
        char id_text[12];
        first_arg(arg, id_text, sizeof(id_text));
        console_puts(proof_replay(parse_u32(id_text)) == 0 ? "proof replay ok\n" : "proof replay failed\n");
    } else {
        console_puts("usage: proof list|new NAME GOAL|assume ID FACT|step ID RULE EXPR|show ID|export ID|replay ID\n");
    }
}
