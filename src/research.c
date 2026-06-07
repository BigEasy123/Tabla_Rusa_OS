#include "console.h"
#include "fs.h"
#include "timer.h"
#include "research.h"

#define RESEARCH_PROJECT_MAX 6
#define RESEARCH_CELL_MAX 18
#define RESEARCH_DATASET_MAX 10
#define RESEARCH_EXPERIMENT_MAX 10
#define RESEARCH_NOTEBOOK_MAX 8
#define RESEARCH_CITATION_MAX 10
#define RESEARCH_TASK_MAX 10
#define RESEARCH_TIMELINE_MAX 10
#define RESEARCH_RELATION_MAX 10

static struct research_project_info projects[RESEARCH_PROJECT_MAX];
static struct research_cell_info cells[RESEARCH_CELL_MAX];
static struct research_dataset_info datasets[RESEARCH_DATASET_MAX];
static struct research_experiment_info experiments[RESEARCH_EXPERIMENT_MAX];
static struct research_notebook_info notebooks[RESEARCH_NOTEBOOK_MAX];
static struct research_citation_info citations[RESEARCH_CITATION_MAX];
static struct research_task_info tasks[RESEARCH_TASK_MAX];
static struct research_timeline_info timeline[RESEARCH_TIMELINE_MAX];
static struct research_relation_info relations[RESEARCH_RELATION_MAX];
static uint32_t next_project_id = 1;
static uint32_t next_cell_id = 1;
static uint32_t next_dataset_id = 1;
static uint32_t next_experiment_id = 1;
static uint32_t next_notebook_id = 1;
static uint32_t next_citation_id = 1;
static uint32_t next_task_id = 1;
static uint32_t next_timeline_id = 1;
static uint32_t next_relation_id = 1;
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

static void project_latex_path(uint32_t id, char* out, uint32_t max){
    copy_text(out, "/research/project-", max);
    append_dec(out, id, max);
    append_text(out, ".tex", max);
}

static void notebook_doc_path(uint32_t id, char* out, uint32_t max){
    copy_text(out, "/research/notebook-", max);
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
    fs_mkdir("/research/notebooks");
    fs_mkdir("/research/citations");
    fs_mkdir("/research/tasks");
    fs_mkdir("/research/timeline");
    fs_mkdir("/research/graph");
    if(!seeded){
        int id = research_project_create("tabla-rusa", "Research OS and computational language design", "os,rusa,science");
        if(id > 0){
            int nb = notebook_create((uint32_t)id, "system-notebook");
            research_add_note((uint32_t)id, RESEARCH_CELL_MARKDOWN, "Roadmap", "Modular OS-language integration workspace.");
            if(nb > 0)
                notebook_add_cell((uint32_t)nb, RESEARCH_CELL_MARKDOWN, "Roadmap", "Modular OS-language integration workspace.");
            research_register_dataset((uint32_t)id, "selftest-log", "/var/log/system.log", "lines: text");
            research_add_citation((uint32_t)id, "templeos", "TempleOS inspiration note", "local design memory");
            research_track_experiment((uint32_t)id, "boot-selftest", "test");
            research_add_task((uint32_t)id, "Wire research GUI panels", "system");
            research_add_timeline((uint32_t)id, "seed", "Research OS framework initialized.");
            research_add_relation((uint32_t)id, "Rusa", "drives", "Research notebooks");
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

int research_add_dataset(uint32_t project_id, const char* name, const char* path, const char* schema){
    return research_register_dataset(project_id, name, path, schema);
}

int research_add_citation(uint32_t project_id, const char* key, const char* title, const char* source){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0) return -1;
    for(i = 0; i < RESEARCH_CITATION_MAX; i++){
        if(citations[i].id == 0){
            citations[i].id = next_citation_id++;
            citations[i].project_id = project_id;
            copy_text(citations[i].key, key, sizeof(citations[i].key));
            copy_text(citations[i].title, title, sizeof(citations[i].title));
            copy_text(citations[i].source, source, sizeof(citations[i].source));
            research_add_note(project_id, RESEARCH_CELL_CITATION, key, title);
            bump_project(project_id);
            return (int)citations[i].id;
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

int research_add_result(uint32_t project_id, const char* experiment, const char* summary){
    return research_record_result(project_id, experiment, summary);
}

int research_add_task(uint32_t project_id, const char* title, const char* owner){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0 || !title || !title[0]) return -1;
    for(i = 0; i < RESEARCH_TASK_MAX; i++){
        if(tasks[i].id == 0){
            tasks[i].id = next_task_id++;
            tasks[i].project_id = project_id;
            copy_text(tasks[i].title, title, sizeof(tasks[i].title));
            copy_text(tasks[i].status, "todo", sizeof(tasks[i].status));
            copy_text(tasks[i].owner, owner && owner[0] ? owner : "unassigned", sizeof(tasks[i].owner));
            bump_project(project_id);
            return (int)tasks[i].id;
        }
    }
    return -1;
}

int research_update_task(uint32_t task_id, const char* status){
    uint32_t i;
    for(i = 0; i < RESEARCH_TASK_MAX; i++){
        if(tasks[i].id == task_id){
            copy_text(tasks[i].status, status && status[0] ? status : "todo", sizeof(tasks[i].status));
            bump_project(tasks[i].project_id);
            return 0;
        }
    }
    return -1;
}

int research_add_timeline(uint32_t project_id, const char* label, const char* note){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0 || !label || !label[0]) return -1;
    for(i = 0; i < RESEARCH_TIMELINE_MAX; i++){
        if(timeline[i].id == 0){
            timeline[i].id = next_timeline_id++;
            timeline[i].project_id = project_id;
            copy_text(timeline[i].label, label, sizeof(timeline[i].label));
            copy_text(timeline[i].note, note, sizeof(timeline[i].note));
            timeline[i].tick = timer_ticks();
            bump_project(project_id);
            return (int)timeline[i].id;
        }
    }
    return -1;
}

int research_add_relation(uint32_t project_id, const char* from, const char* relation, const char* to){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0 || !from || !from[0] || !relation || !relation[0] || !to || !to[0]) return -1;
    for(i = 0; i < RESEARCH_RELATION_MAX; i++){
        if(relations[i].id == 0){
            relations[i].id = next_relation_id++;
            relations[i].project_id = project_id;
            copy_text(relations[i].from, from, sizeof(relations[i].from));
            copy_text(relations[i].relation, relation, sizeof(relations[i].relation));
            copy_text(relations[i].to, to, sizeof(relations[i].to));
            bump_project(project_id);
            return (int)relations[i].id;
        }
    }
    return -1;
}

int research_export_latex(uint32_t project_id, char* out_path, uint32_t out_max){
    int slot = find_project_slot(project_id);
    char path[48];
    char text[768];
    uint32_t i;
    if(slot < 0) return -1;
    project_latex_path(project_id, path, sizeof(path));
    copy_text(text, "\\section{", sizeof(text));
    append_text(text, projects[slot].name, sizeof(text));
    append_text(text, "}\n", sizeof(text));
    append_text(text, projects[slot].description, sizeof(text));
    append_text(text, "\n\\subsection{Notes}\n", sizeof(text));
    for(i = 0; i < RESEARCH_CELL_MAX; i++){
        if(cells[i].id && cells[i].project_id == project_id){
            append_text(text, "\\paragraph{", sizeof(text));
            append_text(text, cells[i].title, sizeof(text));
            append_text(text, "} ", sizeof(text));
            append_text(text, cells[i].body, sizeof(text));
            append_text(text, "\n", sizeof(text));
        }
    }
    append_text(text, "\\subsection{Citations}\n", sizeof(text));
    for(i = 0; i < RESEARCH_CITATION_MAX; i++){
        if(citations[i].id && citations[i].project_id == project_id){
            append_text(text, "\\cite{", sizeof(text));
            append_text(text, citations[i].key, sizeof(text));
            append_text(text, "} ", sizeof(text));
            append_text(text, citations[i].title, sizeof(text));
            append_text(text, "\n", sizeof(text));
        }
    }
    fs_write(path, text);
    copy_text(out_path, path, out_max);
    return 0;
}

int notebook_create(uint32_t project_id, const char* name){
    uint32_t i;
    int slot = find_project_slot(project_id);
    if(slot < 0) return -1;
    for(i = 0; i < RESEARCH_NOTEBOOK_MAX; i++){
        if(notebooks[i].id == 0){
            notebooks[i].id = next_notebook_id++;
            notebooks[i].project_id = project_id;
            copy_text(notebooks[i].name, name, sizeof(notebooks[i].name));
            notebooks[i].cells = 0;
            notebook_doc_path(notebooks[i].id, notebooks[i].export_path, sizeof(notebooks[i].export_path));
            projects[slot].notebooks++;
            bump_project(project_id);
            return (int)notebooks[i].id;
        }
    }
    return -1;
}

static int notebook_project(uint32_t notebook_id){
    uint32_t i;
    for(i = 0; i < RESEARCH_NOTEBOOK_MAX; i++)
        if(notebooks[i].id == notebook_id)
            return (int)notebooks[i].project_id;
    return -1;
}

int notebook_add_cell(uint32_t notebook_id, enum research_cell_type type, const char* title, const char* body){
    uint32_t i;
    int project_id = notebook_project(notebook_id);
    int cell;
    if(project_id < 0) return -1;
    cell = research_add_note((uint32_t)project_id, type, title, body);
    if(cell < 0) return -1;
    for(i = 0; i < RESEARCH_NOTEBOOK_MAX; i++)
        if(notebooks[i].id == notebook_id)
            notebooks[i].cells++;
    return cell;
}

int notebook_run_cell(uint32_t notebook_id, uint32_t cell_id, char* out, uint32_t out_max){
    int project_id = notebook_project(notebook_id);
    uint32_t i;
    if(project_id < 0) return -1;
    for(i = 0; i < RESEARCH_CELL_MAX; i++){
        if(cells[i].id == cell_id && cells[i].project_id == (uint32_t)project_id){
            copy_text(out, "cell result: ", out_max);
            append_text(out, cells[i].title, out_max);
            append_text(out, " ready", out_max);
            research_record_result((uint32_t)project_id, cells[i].title, out ? out : "cell result");
            return 0;
        }
    }
    return -1;
}

int notebook_export(uint32_t notebook_id, char* out_path, uint32_t out_max){
    uint32_t i, j;
    char text[768];
    for(i = 0; i < RESEARCH_NOTEBOOK_MAX; i++){
        if(notebooks[i].id == notebook_id){
            copy_text(text, "# ", sizeof(text));
            append_text(text, notebooks[i].name, sizeof(text));
            append_text(text, "\n\n", sizeof(text));
            for(j = 0; j < RESEARCH_CELL_MAX; j++){
                if(cells[j].id && cells[j].project_id == notebooks[i].project_id){
                    append_text(text, "## ", sizeof(text));
                    append_text(text, cells[j].title, sizeof(text));
                    append_text(text, "\n", sizeof(text));
                    append_text(text, cells[j].body, sizeof(text));
                    append_text(text, "\n\n", sizeof(text));
                }
            }
            fs_write(notebooks[i].export_path, text);
            copy_text(out_path, notebooks[i].export_path, out_max);
            return 0;
        }
    }
    return -1;
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

uint32_t research_citation_list(uint32_t project_id, struct research_citation_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_CITATION_MAX; i++){
        if(citations[i].id && citations[i].project_id == project_id){
            if(out && n < max) out[n] = citations[i];
            n++;
        }
    }
    return n;
}

uint32_t research_task_list(uint32_t project_id, struct research_task_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_TASK_MAX; i++){
        if(tasks[i].id && tasks[i].project_id == project_id){
            if(out && n < max) out[n] = tasks[i];
            n++;
        }
    }
    return n;
}

uint32_t research_timeline_list(uint32_t project_id, struct research_timeline_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_TIMELINE_MAX; i++){
        if(timeline[i].id && timeline[i].project_id == project_id){
            if(out && n < max) out[n] = timeline[i];
            n++;
        }
    }
    return n;
}

uint32_t research_relation_list(uint32_t project_id, struct research_relation_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_RELATION_MAX; i++){
        if(relations[i].id && relations[i].project_id == project_id){
            if(out && n < max) out[n] = relations[i];
            n++;
        }
    }
    return n;
}

uint32_t notebook_list(uint32_t project_id, struct research_notebook_info* out, uint32_t max){
    uint32_t i, n = 0;
    for(i = 0; i < RESEARCH_NOTEBOOK_MAX; i++){
        if(notebooks[i].id && notebooks[i].project_id == project_id){
            if(out && n < max) out[n] = notebooks[i];
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
    } else if(str_eq(action, "citation")){
        char name[32], key[32], title[80];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, key, sizeof(key));
        arg = (char*)first_arg(arg, title, sizeof(title));
        id = research_project_open(name);
        if(id < 0 || !key[0] || !title[0]){
            console_puts("usage: research citation PROJECT KEY TITLE [source]\n");
            return;
        }
        console_puts(research_add_citation((uint32_t)id, key, title, arg) > 0 ? "research: citation added\n" : "research: citation table full\n");
    } else if(str_eq(action, "result")){
        char name[32], exp[40];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, exp, sizeof(exp));
        id = research_project_open(name);
        if(id < 0 || !exp[0]){
            console_puts("usage: research result PROJECT EXPERIMENT SUMMARY\n");
            return;
        }
        console_puts(research_add_result((uint32_t)id, exp, arg) > 0 ? "research: result archived\n" : "research: result table full\n");
    } else if(str_eq(action, "task")){
        char name[32], title[48], owner[32];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, title, sizeof(title));
        arg = (char*)first_arg(arg, owner, sizeof(owner));
        id = research_project_open(name);
        if(id < 0 || !title[0]){
            console_puts("usage: research task PROJECT TITLE [owner]\n");
            return;
        }
        console_puts(research_add_task((uint32_t)id, title, owner) > 0 ? "research: task added\n" : "research: task table full\n");
    } else if(str_eq(action, "timeline")){
        char name[32], label[48];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, label, sizeof(label));
        id = research_project_open(name);
        if(id < 0 || !label[0]){
            console_puts("usage: research timeline PROJECT LABEL NOTE\n");
            return;
        }
        console_puts(research_add_timeline((uint32_t)id, label, arg) > 0 ? "research: timeline added\n" : "research: timeline table full\n");
    } else if(str_eq(action, "graph")){
        char name[32], from[40], relation[24], to[40];
        int id;
        arg = (char*)first_arg(arg, name, sizeof(name));
        arg = (char*)first_arg(arg, from, sizeof(from));
        arg = (char*)first_arg(arg, relation, sizeof(relation));
        arg = (char*)first_arg(arg, to, sizeof(to));
        id = research_project_open(name);
        if(id < 0 || !from[0] || !relation[0] || !to[0]){
            console_puts("usage: research graph PROJECT FROM RELATION TO\n");
            return;
        }
        console_puts(research_add_relation((uint32_t)id, from, relation, to) > 0 ? "research: relation added\n" : "research: relation table full\n");
    } else if(str_eq(action, "notebook")){
        char sub[16], name[32], nb_name[40];
        int id;
        arg = (char*)first_arg(arg, sub, sizeof(sub));
        if(str_eq(sub, "new")){
            arg = (char*)first_arg(arg, name, sizeof(name));
            first_arg(arg, nb_name, sizeof(nb_name));
            id = research_project_open(name);
            if(id < 0 || !nb_name[0]){
                console_puts("usage: research notebook new PROJECT NAME\n");
                return;
            }
            console_puts(notebook_create((uint32_t)id, nb_name) > 0 ? "research: notebook created\n" : "research: notebook table full\n");
        } else if(str_eq(sub, "export")){
            char id_text[12], path[80];
            first_arg(arg, id_text, sizeof(id_text));
            if(notebook_export(parse_u32(id_text), path, sizeof(path)) == 0){
                console_puts(path);
                console_putc('\n');
            } else console_puts("research: notebook not found\n");
        } else {
            console_puts("usage: research notebook new PROJECT NAME | notebook export ID\n");
        }
    } else if(str_eq(action, "latex")){
        char name[32], path[80];
        int id;
        first_arg(arg, name, sizeof(name));
        id = research_project_open(name);
        if(id > 0 && research_export_latex((uint32_t)id, path, sizeof(path)) == 0){
            console_puts(path);
            console_putc('\n');
        } else console_puts("research: project not found\n");
    } else if(str_eq(action, "save")){
        char name[32];
        int id;
        first_arg(arg, name, sizeof(name));
        id = research_project_open(name);
        console_puts(id > 0 && research_project_save((uint32_t)id) == 0 ? "research: project saved\n" : "research: project not found\n");
    } else {
        console_puts("usage: research list | new NAME DESC | note PROJECT TITLE BODY | dataset PROJECT NAME PATH SCHEMA | experiment PROJECT NAME CMD | citation PROJECT KEY TITLE SRC | result PROJECT EXP SUMMARY | task PROJECT TITLE OWNER | timeline PROJECT LABEL NOTE | graph PROJECT FROM REL TO | notebook new/export ... | latex PROJECT | save PROJECT\n");
    }
}
