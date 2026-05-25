#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include "sched.h"

#define SCHED_TASK_MAX 6

struct sched_task {
    const char* name;
    const char* state;
    uint32_t quantum;
    uint32_t runs;
};

static struct sched_task tasks[SCHED_TASK_MAX] = {
    {"shell", "ready", 4, 0},
    {"logger", "ready", 2, 0},
    {"network", "sleep", 2, 0},
    {"gui", "sleep", 3, 0},
    {"compute", "ready", 8, 0},
    {"idle", "ready", 1, 0}
};

static size_t current_task = 0;

static char lower_char(char c){
    return c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c;
}

static int str_eq(const char* a, const char* b){
    while(*a && *b){
        if(lower_char(*a) != lower_char(*b)) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
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

void sched_init(void){
    fs_append_line("/var/log/system.log", "sched: cooperative run queue online");
}

void sched_yield(void){
    for(size_t tries=0; tries<SCHED_TASK_MAX; tries++){
        current_task = (current_task + 1) % SCHED_TASK_MAX;
        if(str_eq(tasks[current_task].state, "ready")){
            tasks[current_task].runs++;
            process_tick(tasks[current_task].name, tasks[current_task].quantum);
            jobs_account("scheduler", tasks[current_task].quantum);
            return;
        }
    }
}

void sched_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<SCHED_TASK_MAX; i++){
            console_puts(i == current_task ? "* " : "  ");
            console_puts(tasks[i].name);
            console_puts(" state=");
            console_puts(tasks[i].state);
            console_puts(" quantum=");
            console_write_dec(tasks[i].quantum);
            console_puts(" runs=");
            console_write_dec(tasks[i].runs);
            console_putc('\n');
        }
    } else if(str_eq(action, "yield") || str_eq(action, "tick")){
        sched_yield();
        console_puts("scheduler: ran ");
        console_puts(tasks[current_task].name);
        console_putc('\n');
    } else if(str_eq(action, "wake")){
        const char* name = first_arg(rest, &rest);
        for(size_t i=0; i<SCHED_TASK_MAX; i++)
            if(str_eq(tasks[i].name, name)){
                tasks[i].state = "ready";
                process_set_running(name, 1);
                console_puts("scheduler: woke task\n");
                return;
            }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "sleep")){
        const char* name = first_arg(rest, &rest);
        for(size_t i=0; i<SCHED_TASK_MAX; i++)
            if(str_eq(tasks[i].name, name)){
                tasks[i].state = "sleep";
                process_set_running(name, 0);
                console_puts("scheduler: slept task\n");
                return;
            }
        console_puts("scheduler: task not found\n");
    } else if(str_eq(action, "quantum")){
        const char* name = first_arg(rest, &rest);
        uint32_t q = parse_u32(rest);
        for(size_t i=0; i<SCHED_TASK_MAX; i++)
            if(str_eq(tasks[i].name, name)){
                tasks[i].quantum = q;
                console_puts("scheduler: quantum set\n");
                return;
            }
        console_puts("scheduler: task not found\n");
    } else {
        console_puts("usage: sched list | yield | wake NAME | sleep NAME | quantum NAME N\n");
    }
}
