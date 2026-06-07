#include "console.h"
#include "fs.h"
#include "timer.h"
#include "research.h"

#define RESEARCH_PROJECT_MAX 6
#define RESEARCH_CELL_MAX 18
#define RESEARCH_DATASET_MAX 10
#define RESEARCH_EXPERIMENT_MAX 10

static struct research_project_info projects[RESEARCH_PROJECT_MAX];
static struct research_cell_info cells[RESEARCH_CELL_MAX];
static struct research_dataset_info datasets[RESEARCH_DATASET_MAX];
static struct research_experiment_info experiments[RESEARCH_EXPERIMENT_MAX];
static uint32_t next_project_id = 1;
static uint32_t next_cell_id = 1;
static uint32_t next_dataset_id = 1;
static uint32_t next_experiment_id = 1;
static int seeded = 0;

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

static void append_dec(char* dst, uint32_t value, uint32_t max){
    char tmp[12];
    uint32_t n = 0;
    if(value == 0){
        append_text(dst, "0", max);
        return;
    }
    while(value && n < sizeof(tmp)){
        tmp[n++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while(n)
        append_text(dst, (char[]){tmp[--n], 0}, max);
}

static int find_project_slot(uint32_t id){
    uint32_t i;
    for(i = 0; i < RESEARCH_PROJECT_MAX; i++)
        if(projects[i].id == id) return (int)i;
    return -1;
}

static int find_project_by_name(const char* name){
    uint32_t i;
    for(i = 0; i < RESEARCH_PROJECT_MAX; i++)
        if(projects[i].id && str_eq(projects[i].name, name)) return (int)i;
    return -1;
}

static void project_doc_path(uint32_t id, char* out, uint32_t max){
    copy_text(out, "/research/project-", max);
    append_dec(out, id, max);
    append_text(out, ".md", max);
}

static void bump_project(uint32_t project_id){
    int slot = find_project_slot(project_id);
    if(slot >= 0)
        projects[slot].modified_tick = timer_ticks();
}

void research_init(void){
    fs_mkdir("/research");
    fs_mkdir("/research/datasets");
    fs_mkdir("/research/results");
    if(!seeded){
        int id = research_project_create("tabla-rusa", "Research OS and computational language design", "os,rusa,science");
        if(id > 0){
            research_add_note((uint32_t)id, RESEARCH_CELL_MARKDOWN, "Roadmap", "Modular OS-language integration workspace.");
            research_register_dataset((uint32_t)id, "selftest-log", "/var/log/system.log", "lines: text");
            research_track_experiment((uint32_t)id, "boot-selftest", "test");
        }
        seeded = 1;
    }
    fs_append_line("/var/log/system.log", "research: framework online");
}

int research_project_create(const char* name, const char* description, const char* tags){
    uint32_t i;
    if(!name || !name[0]) return -1;
    if(find_project_by_name(name) >= 0)
        return projects[find_project_by_name(name)].id;
    for(i = 0; i < RESEARCH_PROJECT_MAX; i++){
        if(projects[i].id == 0){
            projects[i].id = next_project_id++;
            copy_text(projects[i].name, name, sizeof(projects[i].name));
            copy_text(projects[i].description, description, sizeof(projects[i].description));
            copy_text(projects[i].tags, tags, sizeof(projects[i].tags));
            projects[i].modified_tick = timer_ticks();
            research_project_save(projects[i].id);
            return (int)projects[i].id;
        }
    }
    return -1;
}

int research_project_open(const char* name){
    int slot = find_project_by_name(name);
    return slot >= 0 ? (int)projects[slot].id : -1;
}

int research_project_save(uint32_t project_id){
    int slot = find_project_slot(project_id);
    char path[48];
    char text[512];
    if(slot < 0) return -1;
    project_doc_path(project_id, path, sizeof(path));
    copy_text(text, "# ", sizeof(text));
    append_text(text, projects[slot].name, sizeof(text));
    append_text(text, "\n\n", sizeof(text));
    append_text(text, projects[slot].description, sizeof(text));
    append_text(text, "\n\nTags: ", sizeof(text));
    append_text(text, projects[slot].tags, sizeof(text));
    append_text(text, "\nNotebooks: ", sizeof(text));
    append_dec(text, projects[slot].notebooks, sizeof(text));
    append_text(text, "\nDatasets: ", sizeof(text));
    append_dec(text, projects[slot].datasets, sizeof(text));
    append_text(text, "\nExperiments: ", sizeof(text));
    append_dec(text, projects[slot].experiments, sizeof(text));
    append_text(text, "\nResults: ", sizeof(text));
    append_dec(text, projects[slot].results, sizeof(text));
    append_text(text, "\n", sizeof(text));
    fs_write(path, text);
    return 0;
}

int research_add_note(uint32_t project_id, enum research_cell_type type, const char* title, const char* body){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0) return -1;
    for(i = 0; i < RESEARCH_CELL_MAX; i++){
        if(cells[i].id == 0){
            cells[i].id = next_cell_id++;
            cells[i].project_id = project_id;
            cells[i].type = type;
            copy_text(cells[i].title, title, sizeof(cells[i].title));
            copy_text(cells[i].body, body, sizeof(cells[i].body));
            projects[slot].notebooks++;
            bump_project(project_id);
            return (int)cells[i].id;
        }
    }
    return -1;
}

int research_register_dataset(uint32_t project_id, const char* name, const char* path, const char* schema){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0) return -1;
    for(i = 0; i < RESEARCH_DATASET_MAX; i++){
        if(datasets[i].id == 0){
            datasets[i].id = next_dataset_id++;
            datasets[i].project_id = project_id;
            copy_text(datasets[i].name, name, sizeof(datasets[i].name));
            copy_text(datasets[i].path, path, sizeof(datasets[i].path));
            copy_text(datasets[i].schema, schema, sizeof(datasets[i].schema));
            projects[slot].datasets++;
            bump_project(project_id);
            return (int)datasets[i].id;
        }
    }
    return -1;
}

int research_track_experiment(uint32_t project_id, const char* name, const char* command){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0) return -1;
    for(i = 0; i < RESEARCH_EXPERIMENT_MAX; i++){
        if(experiments[i].id == 0){
            experiments[i].id = next_experiment_id++;
            experiments[i].project_id = project_id;
            copy_text(experiments[i].name, name, sizeof(experiments[i].name));
            copy_text(experiments[i].status, "planned", sizeof(experiments[i].status));
            copy_text(experiments[i].command, command, sizeof(experiments[i].command));
            projects[slot].experiments++;
            bump_project(project_id);
            return (int)experiments[i].id;
        }
    }
    return -1;
}

int research_record_result(uint32_t project_id, const char* experiment, const char* summary){
    int cell = research_add_note(project_id, RESEARCH_CELL_RESULT, experiment, summary);
    int slot = find_project_slot(project_id);
    if(cell < 0 || slot < 0) return -1;
    projects[slot].results++;
    return cell;
}

uint32_t research_project_list(struct research_project_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_PROJECT_MAX; i++){
        if(projects[i].id){
            if(out && n < max) out[n] = projects[i];
            n++;
        }
    }
    return n;
}

uint32_t research_cell_list(uint32_t project_id, struct research_cell_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_CELL_MAX; i++){
        if(cells[i].id && cells[i].project_id == project_id){
            if(out && n < max) out[n] = cells[i];
            n++;
        }
    }
    return n;
}

uint32_t research_dataset_list(uint32_t project_id, struct research_dataset_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_DATASET_MAX; i++){
        if(datasets[i].id && datasets[i].project_id == project_id){
            if(out && n < max) out[n] = datasets[i];
            n++;
        }
    }
    return n;
}

uint32_t research_experiment_list(uint32_t project_id, struct research_experiment_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_EXPERIMENT_MAX; i++){
        if(experiments[i].id && experiments[i].project_id == project_id){
            if(out && n < max) out[n] = experiments[i];
            n++;
        }
    }
    return n;
}

void research_cmd(char* arg){
    char action[16];
    arg = (char*)first_arg(arg, action, sizeof(action));
    if(action[0] == 0 || str_eq(action, "list")){
        struct research_project_info list[RESEARCH_PROJECT_MAX];
        uint32_t i, n = research_project_list(list, RESEARCH_PROJECT_MAX);
        console_puts("research projects\n");
        for(i = 0; i < n; i++){
            console_puts("  #");
            console_write_dec(list[i].id);
            console_puts(" ");
            console_puts(list[i].name);
            console_puts(" - ");
            console_puts(list[i].description);
            console_putc('\n');
        }
        if(n == 0) console_puts("  none\n");
    } else if(str_eq(action, "new")){
        char name[32];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        if(!name[0]){
            console_puts("usage: research new NAME [description]\n");
            return;
        }
        id = research_project_create(name, arg, "research");
        console_puts(id > 0 ? "research: project created\n" : "research: project table full\n");
    } else if(str_eq(action, "note")){
        char name[32], title[40];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, title, sizeof(title));
        id = research_project_open(name);
        if(id < 0 || !title[0]){
            console_puts("usage: research note PROJECT TITLE BODY\n");
            return;
        }
        console_puts(research_add_note((uint32_t)id, RESEARCH_CELL_MARKDOWN, title, arg) > 0 ? "research: note added\n" : "research: note table full\n");
    } else if(str_eq(action, "dataset")){
        char name[32], ds[40], path[80];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, ds, sizeof(ds));
        arg = (char*)first_arg(arg, path, sizeof(path));
        id = research_project_open(name);
        if(id < 0 || !ds[0] || !path[0]){
            console_puts("usage: research dataset PROJECT NAME PATH [schema]\n");
            return;
        }
        console_puts(research_register_dataset((uint32_t)id, ds, path, arg) > 0 ? "research: dataset registered\n" : "research: dataset table full\n");
    } else if(str_eq(action, "experiment")){
        char name[32], exp[40];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, exp, sizeof(exp));
        id = research_project_open(name);
        if(id < 0 || !exp[0]){
            console_puts("usage: research experiment PROJECT NAME COMMAND\n");
            return;
        }
        console_puts(research_track_experiment((uint32_t)id, exp, arg) > 0 ? "research: experiment tracked\n" : "research: experiment table full\n");
    } else if(str_eq(action, "save")){
        char name[32];
        int id;
        first_arg(arg, name, sizeof(name));
        id = research_project_open(name);
        console_puts(id > 0 && research_project_save((uint32_t)id) == 0 ? "research: project saved\n" : "research: project not found\n");
    } else {
        console_puts("usage: research list | new NAME DESC | note PROJECT TITLE BODY | dataset PROJECT NAME PATH SCHEMA | experiment PROJECT NAME CMD | save PROJECT\n");
    }
}
