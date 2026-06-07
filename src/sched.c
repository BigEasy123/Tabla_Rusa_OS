#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include "sched.h"
#include "timer.h"

#define SCHED_TASK_MAX 10
#define SCHED_STACK_WORDS 64
#define SCHED_NAME_LEN 24

enum sched_state {
    SCHED_READY,
    SCHED_RUNNING,
    SCHED_SLEEPING,
    SCHED_BLOCKED
};

struct sched_context {
    uint32_t pc;
    uint32_t sp;
    uint32_t stack_base;
    uint32_t stack_limit;
    uint32_t budget;
};

struct sched_task {
    const char* name;
    enum sched_state state;
    uint32_t quantum;
    uint32_t runs;
    uint32_t ticks;
    uint32_t wake_tick;
    struct sched_context ctx;
    void (*entry)(struct sched_task* task);
    sched_task_entry_fn external_entry;
};

static uint32_t stacks[SCHED_TASK_MAX][SCHED_STACK_WORDS];
static struct sched_task tasks[SCHED_TASK_MAX];
static char task_names[SCHED_TASK_MAX][SCHED_NAME_LEN];
static size_t current_task = 0;
static uint32_t total_switches = 0;

static char lower_char(char c){
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int str_eq(const char* a, const char* b){
    if(!a || !b)
        return 0;
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
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

static int is_space(char c){
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char* first_arg(char* arg, char** rest){
    while(is_space(*arg)) arg++;
    char* start = arg;
    while(*arg && !is_space(*arg)) arg++;
    if(*arg){
        *arg = 0;
        arg++;
    }
    while(is_space(*arg)) arg++;
    *rest = arg;
    return start;
}

static uint32_t parse_u32(const char* s){
    uint32_t value = 0;
    while(is_space(*s)) s++;
    while(*s >= '0' && *s <= '9'){
        value = value * 10 + (uint32_t)(*s - '0');
        s++;
    }
    return value;
}

static const char* state_name(enum sched_state state){
    if(state == SCHED_READY) return "ready";
    if(state == SCHED_RUNNING) return "run";
    if(state == SCHED_SLEEPING) return "sleep";
    return "block";
}

static void task_shell(struct sched_task* task){
    task->ctx.pc++;
    jobs_account("interactive", 1);
}

static void task_logger(struct sched_task* task){
    task->ctx.pc++;
    jobs_account("io", 1);
}

static void task_network(struct sched_task* task){
    task->ctx.pc++;
    jobs_account("network", 1);
}

static void task_gui(struct sched_task* task){
    task->ctx.pc++;
    jobs_account("gui", 1);
}

static void task_compute(struct sched_task* task){
    task->ctx.pc += 2;
    jobs_account("scientific", task->quantum);
}

static void task_idle(struct sched_task* task){
    task->ctx.pc++;
    jobs_account("idle", 1);
}

static void task_setup(size_t id, const char* name, enum sched_state state,
                       uint32_t quantum, void (*entry)(struct sched_task* task)){
    tasks[id].name = name;
    tasks[id].state = state;
    tasks[id].quantum = quantum;
    tasks[id].runs = 0;
    tasks[id].ticks = 0;
    tasks[id].wake_tick = 0;
    tasks[id].ctx.pc = 0;
    tasks[id].ctx.stack_base = (uint32_t)&stacks[id][SCHED_STACK_WORDS - 1];
    tasks[id].ctx.stack_limit = (uint32_t)&stacks[id][0];
    tasks[id].ctx.sp = tasks[id].ctx.stack_base;
    tasks[id].ctx.budget = quantum;
    tasks[id].entry = entry;
    tasks[id].external_entry = 0;
}

static struct sched_task* find_task(const char* name){
    for(size_t i=0; i<SCHED_TASK_MAX; i++)
        if(tasks[i].name && str_eq(tasks[i].name, name))
            return &tasks[i];
    return 0;
}

static size_t free_task_slot(void){
    for(size_t i=0; i<SCHED_TASK_MAX; i++)
        if(!tasks[i].name)
            return i;
    return SCHED_TASK_MAX;
}

void sched_init(void){
    task_setup(0, "shell", SCHED_READY, 4, task_shell);
    task_setup(1, "logger", SCHED_READY, 2, task_logger);
    task_setup(2, "network", SCHED_SLEEPING, 2, task_network);
    task_setup(3, "gui", SCHED_SLEEPING, 3, task_gui);
    task_setup(4, "compute", SCHED_READY, 8, task_compute);
    task_setup(5, "idle", SCHED_READY, 1, task_idle);
    for(size_t i=6; i<SCHED_TASK_MAX; i++){
        tasks[i].name = 0;
        tasks[i].state = SCHED_BLOCKED;
        tasks[i].entry = 0;
        tasks[i].external_entry = 0;
        task_names[i][0] = 0;
    }
    current_task = 0;
    total_switches = 0;
    fs_append_line("/var/log/system.log", "sched: timer-driven task contexts online");
}

static void wake_due_tasks(void){
    uint32_t now = timer_ticks();
    for(size_t i=0; i<SCHED_TASK_MAX; i++){
        if(tasks[i].state == SCHED_SLEEPING && tasks[i].wake_tick && now >= tasks[i].wake_tick){
            tasks[i].wake_tick = 0;
            tasks[i].state = SCHED_READY;
            process_set_running(tasks[i].name, 1);
        }
    }
}

static void run_task(size_t id){
    struct sched_task* task = &tasks[id];
    task->state = SCHED_RUNNING;
    task->runs++;
    task->ticks += task->quantum;
    task->ctx.budget = task->quantum;
    total_switches++;
    process_context_switch(task->name, timer_ticks());
    process_tick(task->name, task->quantum);
    jobs_account("scheduler", task->quantum);
    if(task->entry)
        task->entry(task);
    if(task->external_entry)
        task->external_entry(task->name, task->quantum);
    if(task->state == SCHED_RUNNING)
        task->state = SCHED_READY;
}

void sched_yield(void){
    wake_due_tasks();
    for(size_t tries=0; tries<SCHED_TASK_MAX; tries++){
        current_task = (current_task + 1) % SCHED_TASK_MAX;
        if(tasks[current_task].state == SCHED_READY){
            run_task(current_task);
            return;
        }
    }
}

void sched_on_timer(void){
    static uint32_t divisor = 0;
    divisor++;
    wake_due_tasks();
    if(divisor < 10)
        return;
    divisor = 0;
    sched_yield();
}

const char* sched_current_name(void){
    return tasks[current_task].name;
}

uint32_t sched_total_switches(void){
    return total_switches;
}

int sched_register_task(const char* name, uint32_t quantum, sched_task_entry_fn entry){
    struct sched_task* task;
    size_t id;
    if(!name || !name[0] || !entry)
        return -1;
    task = find_task(name);
    if(task){
        task->quantum = quantum ? quantum : 1;
        task->external_entry = entry;
        task->state = SCHED_READY;
        process_spawn(name);
        return 0;
    }
    id = free_task_slot();
    if(id >= SCHED_TASK_MAX)
        return -2;
    copy_text(task_names[id], name, sizeof(task_names[id]));
    task_setup(id, task_names[id], SCHED_READY, quantum ? quantum : 1, 0);
    tasks[id].external_entry = entry;
    process_spawn(name);
    process_set_background(name, 1);
    return 0;
}

int sched_task_ready(const char* name){
    struct sched_task* task = find_task(name);
    if(!task)
        return -1;
    task->state = SCHED_READY;
    process_set_running(name, 1);
    return 0;
}

uint32_t sched_task_runs(const char* name){
    struct sched_task* task = find_task(name);
    return task ? task->runs : 0;
}

static void print_task(const struct sched_task* task, size_t id){
    if(!task->name)
        return;
    console_puts(id == current_task ? "* " : "  ");
    console_puts(task->name);
    console_puts(" state=");
    console_puts(state_name(task->state));
    console_puts(" quantum=");
    console_write_dec(task->quantum);
    console_puts(" runs=");
    console_write_dec(task->runs);
    console_puts(" pc=");
    console_write_dec(task->ctx.pc);
    console_puts(" sp=");
    console_write_hex(task->ctx.sp);
    console_putc('\n');
}

void sched_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        console_puts("switches=");
        console_write_dec(total_switches);
        console_puts(" current=");
        console_puts(tasks[current_task].name);
        console_putc('\n');
        for(size_t i=0; i<SCHED_TASK_MAX; i++)
            print_task(&tasks[i], i);
    } else if(str_eq(action, "yield") || str_eq(action, "tick") || str_eq(action, "step")){
        sched_yield();
        console_puts("scheduler: ran ");
        console_puts(tasks[current_task].name);
        console_putc('\n');
    } else if(str_eq(action, "run")){
        uint32_t count = parse_u32(rest);
        if(count == 0) count = 1;
        while(count--)
            sched_yield();
        console_puts("scheduler: switches=");
        console_write_dec(total_switches);
        console_putc('\n');
    } else if(str_eq(action, "wake")){
        const char* name = first_arg(rest, &rest);
        struct sched_task* task = find_task(name);
        if(task){
            task->state = SCHED_READY;
            task->wake_tick = 0;
            process_set_running(name, 1);
            console_puts("scheduler: woke task\n");
            return;
        }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "sleep")){
        const char* name = first_arg(rest, &rest);
        struct sched_task* task = find_task(name);
        uint32_t ticks = parse_u32(rest);
        if(task){
            task->state = SCHED_SLEEPING;
            task->wake_tick = ticks ? timer_ticks() + ticks : 0;
            process_set_running(name, 0);
            console_puts("scheduler: slept task\n");
            return;
        }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "block")){
        const char* name = first_arg(rest, &rest);
        struct sched_task* task = find_task(name);
        if(task){
            task->state = SCHED_BLOCKED;
            process_set_running(name, 0);
            console_puts("scheduler: blocked task\n");
            return;
        }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "quantum")){
        const char* name = first_arg(rest, &rest);
        uint32_t q = parse_u32(rest);
        struct sched_task* task = find_task(name);
        if(task){
            task->quantum = q ? q : 1;
            console_puts("scheduler: quantum set\n");
            return;
        }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "trace")){
        const char* name = first_arg(rest, &rest);
        struct sched_task* task = find_task(name);
        if(task){
            console_puts(task->name);
            console_puts(" pc=");
            console_write_dec(task->ctx.pc);
            console_puts(" sp=");
            console_write_hex(task->ctx.sp);
            console_puts(" stack=");
            console_write_hex(task->ctx.stack_limit);
            console_puts("..");
            console_write_hex(task->ctx.stack_base);
            console_puts(" wake=");
            console_write_dec(task->wake_tick);
            console_putc('\n');
            return;
        }
        console_puts("scheduler: task not found\n");
    } else {
        console_puts("usage: sched list | yield | run N | wake NAME | sleep NAME [TICKS] | block NAME | quantum NAME N | trace NAME\n");
    }
}

void scheduler_init(void){
    sched_init();
}

void scheduler_tick(void){
    sched_on_timer();
}

const char* scheduler_pick_next(void){
    sched_yield();
    return sched_current_name();
}

int scheduler_set_priority(const char* name, uint32_t priority){
    struct sched_task* task = find_task(name);
    if(!task)
        return -1;
    task->quantum = priority ? priority : 1;
    process_set_priority(name, priority);
    return 0;
}
