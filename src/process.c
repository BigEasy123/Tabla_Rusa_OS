#include "process.h"
#include "console.h"
#include "fd.h"

#define IPC_MAX 12
#define PIPE_MAX 6
#define PROCESS_NAME_LEN 24
#define PROCESS_KIND_LEN 16
#define PROCESS_WORKLOAD_LEN 32
#define PROCESS_HANDLE_MAX 12

struct pipe_info {
    uint32_t id;
    uint32_t owner_pid;
    char buffer[96];
    int used;
    int has_data;
};

struct process_handle_slot {
    int used;
    int id;
    uint32_t owner_pid;
    uint32_t target_pid;
};

static struct process_info processes[PROCESS_MAX] = {
    {"kernel", 0, 0, 1, PROCESS_RUNNING, "kernel", 100, 0, 512, 0, 1, "system", 0, 0, 0},
    {"shell", 1, 0, 1, PROCESS_RUNNING, "user", 40, 0, 96, 0, 0, "interactive", 0, 0, 0},
    {"editor", 2, 1, 0, PROCESS_READY, "user", 35, 0, 128, 0, 0, "interactive", 0, 0, 0},
    {"logger", 3, 0, 1, PROCESS_RUNNING, "service", 20, 0, 64, 0, 1, "io", 0, 0, 0},
    {"network", 4, 0, 0, PROCESS_READY, "service", 55, 0, 160, 0, 1, "io", 0, 0, 0},
    {"compute", 5, 1, 0, PROCESS_READY, "service", 90, 0, 256, 0, 1, "scientific", 0, 0, 0}
};

static char process_names[PROCESS_MAX][PROCESS_NAME_LEN];
static char process_kinds[PROCESS_MAX][PROCESS_KIND_LEN];
static char process_workloads[PROCESS_MAX][PROCESS_WORKLOAD_LEN];
static struct ipc_message_info ipc_messages[IPC_MAX];
static struct pipe_info pipes[PIPE_MAX];
static struct process_handle_slot process_handles[PROCESS_HANDLE_MAX];
static uint32_t current_pid = 1;
static uint32_t next_pid = 6;
static uint32_t next_pipe_id = 1;
static int next_handle_id = 1;

static int proc_is(const char* a, const char* b){
    if(!a || !b)
        return 0;
    while(*a && *b){
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if(ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static void copy_text(char* dst, const char* src, uint32_t max){
    uint32_t i = 0;
    if(max == 0)
        return;
    if(!src)
        src = "";
    while(src[i] && i + 1 < max){
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static struct process_info* process_find_pid(uint32_t pid){
    for(int i=0; i<PROCESS_MAX; i++)
        if(processes[i].name && processes[i].pid == pid)
            return &processes[i];
    return 0;
}

static struct process_info* free_process_slot(void){
    for(int i=0; i<PROCESS_MAX; i++)
        if(processes[i].name == 0 || processes[i].state == PROCESS_TERMINATED)
            return &processes[i];
    return 0;
}

static uint32_t process_index(const struct process_info* proc){
    return (uint32_t)(proc - processes);
}

void process_init(void){
    for(int i=0; i<PROCESS_MAX; i++){
        if(!processes[i].name)
            continue;
        if(proc_is(processes[i].name, "kernel") || proc_is(processes[i].name, "shell") ||
           proc_is(processes[i].name, "logger")){
            processes[i].running = 1;
            processes[i].state = PROCESS_RUNNING;
        } else {
            processes[i].running = 0;
            processes[i].state = PROCESS_READY;
        }
        processes[i].handle_count = 0;
    }
    for(int i=0; i<IPC_MAX; i++)
        ipc_messages[i].used = 0;
    for(int i=0; i<PIPE_MAX; i++)
        pipes[i].used = 0;
    for(int i=0; i<PROCESS_HANDLE_MAX; i++)
        process_handles[i].used = 0;
    current_pid = 1;
    next_pid = 6;
    next_pipe_id = 1;
    next_handle_id = 1;
}

struct process_info* process_find(const char* name){
    for(int i=0; i<PROCESS_MAX; i++)
        if(processes[i].name && processes[i].state != PROCESS_TERMINATED && proc_is(processes[i].name, name))
            return &processes[i];
    return 0;
}

const struct process_info* process_at(uint32_t index){
    if(index >= PROCESS_MAX)
        return 0;
    if(!processes[index].name)
        return 0;
    return &processes[index];
}

uint32_t process_count(void){
    uint32_t count = 0;
    for(int i=0; i<PROCESS_MAX; i++)
        if(processes[i].name && processes[i].state != PROCESS_TERMINATED)
            count++;
    return count;
}

void process_set_running(const char* name, int running){
    struct process_info* proc = process_find(name);
    if(proc){
        proc->running = running;
        if(running)
            proc->state = PROCESS_RUNNING;
        else if(proc->state == PROCESS_RUNNING)
            proc->state = PROCESS_READY;
    }
}

void process_set_compute(const char* name, const char* workload, uint32_t priority, uint32_t cpu_hint){
    struct process_info* proc = process_find(name);
    if(!proc)
        return;
    proc->workload = workload;
    proc->priority = priority;
    proc->cpu_hint = cpu_hint;
}

void process_tick(const char* name, uint32_t ticks){
    struct process_info* proc = process_find(name);
    if(proc)
        proc->ticks += ticks;
}

void process_context_switch(const char* name, uint32_t tick){
    struct process_info* proc = process_find(name);
    if(proc){
        proc->switches++;
        proc->last_run_tick = tick;
        proc->running = 1;
        proc->state = PROCESS_RUNNING;
        current_pid = proc->pid;
    }
}

void process_list(void){
    for(int i=0; i<PROCESS_MAX; i++){
        if(!processes[i].name || processes[i].state == PROCESS_TERMINATED)
            continue;
        processes[i].handle_count = process_handles_for_pid(processes[i].pid);
        console_write_dec(processes[i].pid);
        console_puts(" parent=");
        console_write_dec(processes[i].parent_pid);
        console_puts("  ");
        console_puts(processes[i].running ? "run " : "stop ");
        console_puts(process_state_name(processes[i].state));
        console_puts(" ");
        console_puts(processes[i].kind);
        console_puts("  ");
        console_puts(processes[i].name);
        console_puts(" prio=");
        console_write_dec(processes[i].priority);
        console_puts(" class=");
        console_puts(processes[i].workload);
        console_puts(" mem=");
        console_write_dec(processes[i].memory_kib);
        console_puts("KiB handles=");
        console_write_dec(processes[i].handle_count);
        console_puts(" switches=");
        console_write_dec(processes[i].switches);
        console_puts(" last=");
        console_write_dec(processes[i].last_run_tick);
        console_putc('\n');
    }
}

void process_compute_report(void){
    console_puts("scheduler profile: scientific-first cooperative table\n");
    console_puts("policy: prefer scientific/vector workloads, keep shell interactive\n");
    for(int i=0; i<PROCESS_MAX; i++){
        if(!processes[i].name || processes[i].state == PROCESS_TERMINATED)
            continue;
        if(proc_is(processes[i].workload, "scientific") || proc_is(processes[i].workload, "vector")){
            console_puts(processes[i].name);
            console_puts(" running=");
            console_puts(processes[i].running ? "yes" : "no");
            console_puts(" priority=");
            console_write_dec(processes[i].priority);
            console_puts(" cpu_hint=");
            console_write_dec(processes[i].cpu_hint);
            console_puts(" ticks=");
            console_write_dec(processes[i].ticks);
            console_puts(" switches=");
            console_write_dec(processes[i].switches);
            console_putc('\n');
        }
    }
}

int process_stop(const char* name){
    struct process_info* proc = process_find(name);
    if(proc == 0 || proc_is(proc->name, "kernel") || proc_is(proc->name, "shell"))
        return -1;
    fd_close_process(proc->pid);
    proc->running = 0;
    proc->state = PROCESS_READY;
    return 0;
}

const char* process_state_name(enum process_state state){
    if(state == PROCESS_NEW) return "new";
    if(state == PROCESS_READY) return "ready";
    if(state == PROCESS_RUNNING) return "running";
    if(state == PROCESS_SLEEPING) return "sleeping";
    if(state == PROCESS_WAITING) return "waiting";
    if(state == PROCESS_BLOCKED) return "blocked";
    if(state == PROCESS_ZOMBIE) return "zombie";
    if(state == PROCESS_TERMINATED) return "terminated";
    return "unknown";
}

int process_create(const char* name, const char* kind, const char* workload, uint32_t priority){
    if(!name || !name[0] || process_find(name))
        return -1;
    struct process_info* proc = free_process_slot();
    if(!proc)
        return -2;
    uint32_t index = process_index(proc);
    copy_text(process_names[index], name, PROCESS_NAME_LEN);
    copy_text(process_kinds[index], kind && kind[0] ? kind : "user", PROCESS_KIND_LEN);
    copy_text(process_workloads[index], workload && workload[0] ? workload : "interactive", PROCESS_WORKLOAD_LEN);
        proc->name = process_names[index];
        proc->pid = next_pid++;
        proc->parent_pid = current_pid;
        proc->running = 0;
        proc->state = PROCESS_NEW;
        proc->kind = process_kinds[index];
        proc->priority = priority ? priority : 40;
        proc->cpu_hint = 0;
        proc->memory_kib = 64;
        proc->handle_count = 0;
        proc->background = 0;
        proc->workload = process_workloads[index];
    proc->ticks = 0;
    proc->switches = 0;
    proc->last_run_tick = 0;
    return (int)proc->pid;
}

int process_spawn(const char* name){
    struct process_info* proc = process_find(name);
    if(!proc){
        int pid = process_create(name, "user", "interactive", 40);
        if(pid < 0)
            return pid;
        proc = process_find(name);
    }
    if(!proc)
        return -1;
    proc->running = 1;
    proc->state = PROCESS_READY;
    return 0;
}

int process_kill(const char* name){
    struct process_info* proc = process_find(name);
    if(proc == 0 || proc_is(proc->name, "kernel") || proc_is(proc->name, "shell"))
        return -1;
    fd_close_process(proc->pid);
    proc->running = 0;
    proc->state = PROCESS_TERMINATED;
    return 0;
}

int process_sleep(const char* name){
    struct process_info* proc = process_find(name);
    if(!proc || proc_is(proc->name, "kernel"))
        return -1;
    proc->running = 0;
    proc->state = PROCESS_SLEEPING;
    return 0;
}

int process_wake(const char* name){
    struct process_info* proc = process_find(name);
    if(!proc)
        return -1;
    proc->running = 1;
    proc->state = PROCESS_READY;
    return 0;
}

void process_yield(void){
    uint32_t start = current_pid;
    for(int step=0; step<PROCESS_MAX; step++){
        uint32_t next = (start + 1 + (uint32_t)step) % (uint32_t)PROCESS_MAX;
        for(int i=0; i<PROCESS_MAX; i++){
            if(processes[i].name && processes[i].pid == next &&
               processes[i].running && processes[i].state != PROCESS_SLEEPING &&
               processes[i].state != PROCESS_BLOCKED && processes[i].state != PROCESS_TERMINATED){
                current_pid = processes[i].pid;
                processes[i].switches++;
                return;
            }
        }
    }
}

const struct process_info* process_get_current(void){
    struct process_info* proc = process_find_pid(current_pid);
    if(proc)
        return proc;
    return process_find("shell");
}

uint32_t process_list_info(struct process_info* out, uint32_t max){
    uint32_t count = 0;
    for(int i=0; i<PROCESS_MAX; i++){
        if(!processes[i].name || processes[i].state == PROCESS_TERMINATED)
            continue;
        processes[i].handle_count = process_handles_for_pid(processes[i].pid);
        if(out && count < max)
            out[count] = processes[i];
        count++;
    }
    return count;
}

int process_set_priority(const char* name, uint32_t priority){
    struct process_info* proc = process_find(name);
    if(!proc)
        return -1;
    proc->priority = priority;
    return 0;
}

int process_set_memory(const char* name, uint32_t memory_kib){
    struct process_info* proc = process_find(name);
    if(!proc)
        return -1;
    proc->memory_kib = memory_kib;
    return 0;
}

int process_set_background(const char* name, int background){
    struct process_info* proc = process_find(name);
    if(!proc)
        return -1;
    proc->background = background ? 1 : 0;
    return 0;
}

uint32_t process_memory_total_kib(void){
    uint32_t total = 0;
    for(int i=0; i<PROCESS_MAX; i++)
        if(processes[i].name && processes[i].state != PROCESS_TERMINATED)
            total += processes[i].memory_kib;
    return total;
}

uint32_t process_handles_for_pid(uint32_t pid){
    uint32_t count = fd_count_for_pid(pid);
    for(int i=0; i<PROCESS_HANDLE_MAX; i++)
        if(process_handles[i].used && process_handles[i].owner_pid == pid)
            count++;
    return count;
}

int process_handle_open(uint32_t owner_pid, const char* target_name){
    struct process_info* owner = process_find_pid(owner_pid);
    struct process_info* target = process_find(target_name);
    if(!owner || !target)
        return -1;
    for(int i=0; i<PROCESS_HANDLE_MAX; i++){
        if(!process_handles[i].used){
            process_handles[i].used = 1;
            process_handles[i].id = next_handle_id++;
            process_handles[i].owner_pid = owner_pid;
            process_handles[i].target_pid = target->pid;
            owner->handle_count++;
            return process_handles[i].id;
        }
    }
    return -2;
}

int process_handle_close(int handle_id){
    for(int i=0; i<PROCESS_HANDLE_MAX; i++){
        if(process_handles[i].used && process_handles[i].id == handle_id){
            struct process_info* owner = process_find_pid(process_handles[i].owner_pid);
            if(owner && owner->handle_count)
                owner->handle_count--;
            process_handles[i].used = 0;
            return 0;
        }
    }
    return -1;
}

const struct process_info* process_handle_get(int handle_id){
    for(int i=0; i<PROCESS_HANDLE_MAX; i++)
        if(process_handles[i].used && process_handles[i].id == handle_id)
            return process_find_pid(process_handles[i].target_pid);
    return 0;
}

uint32_t process_handle_list(struct process_handle_info* out, uint32_t max){
    uint32_t count = 0;
    for(int i=0; i<PROCESS_HANDLE_MAX; i++){
        if(!process_handles[i].used)
            continue;
        if(out && count < max){
            out[count].id = process_handles[i].id;
            out[count].owner_pid = process_handles[i].owner_pid;
            out[count].target_pid = process_handles[i].target_pid;
            out[count].valid = process_find_pid(process_handles[i].target_pid) != 0;
        }
        count++;
    }
    return count;
}

int ipc_send(uint32_t from_pid, uint32_t to_pid, const char* payload){
    if(!process_find_pid(from_pid) || !process_find_pid(to_pid))
        return -1;
    for(int i=0; i<IPC_MAX; i++){
        if(!ipc_messages[i].used){
            ipc_messages[i].from_pid = from_pid;
            ipc_messages[i].to_pid = to_pid;
            copy_text(ipc_messages[i].payload, payload, sizeof(ipc_messages[i].payload));
            ipc_messages[i].used = 1;
            return 0;
        }
    }
    return -2;
}

int ipc_recv(uint32_t to_pid, struct ipc_message_info* out){
    if(!process_find_pid(to_pid) || !out)
        return -1;
    for(int i=0; i<IPC_MAX; i++){
        if(ipc_messages[i].used && ipc_messages[i].to_pid == to_pid){
            *out = ipc_messages[i];
            ipc_messages[i].used = 0;
            return 0;
        }
    }
    return -2;
}

int pipe_create(uint32_t owner_pid){
    if(!process_find_pid(owner_pid))
        return -1;
    for(int i=0; i<PIPE_MAX; i++){
        if(!pipes[i].used){
            pipes[i].id = next_pipe_id++;
            pipes[i].owner_pid = owner_pid;
            pipes[i].buffer[0] = 0;
            pipes[i].used = 1;
            pipes[i].has_data = 0;
            return (int)pipes[i].id;
        }
    }
    return -2;
}

static struct pipe_info* pipe_find(uint32_t pipe_id){
    for(int i=0; i<PIPE_MAX; i++)
        if(pipes[i].used && pipes[i].id == pipe_id)
            return &pipes[i];
    return 0;
}

int pipe_write(uint32_t pipe_id, const char* payload){
    struct pipe_info* pipe = pipe_find(pipe_id);
    if(!pipe)
        return -1;
    copy_text(pipe->buffer, payload, sizeof(pipe->buffer));
    pipe->has_data = 1;
    return 0;
}

int pipe_read(uint32_t pipe_id, char* out, uint32_t out_max){
    struct pipe_info* pipe = pipe_find(pipe_id);
    if(!pipe || !out || out_max == 0)
        return -1;
    if(!pipe->has_data)
        return -2;
    copy_text(out, pipe->buffer, out_max);
    pipe->buffer[0] = 0;
    pipe->has_data = 0;
    return 0;
}

int signal_send(uint32_t to_pid, const char* signal){
    struct process_info* proc = process_find_pid(to_pid);
    if(!proc || !signal)
        return -1;
    if(proc_is(signal, "kill"))
        return process_kill(proc->name);
    if(proc_is(signal, "sleep"))
        return process_sleep(proc->name);
    if(proc_is(signal, "wake"))
        return process_wake(proc->name);
    return ipc_send(0, to_pid, signal);
}
