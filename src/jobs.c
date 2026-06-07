#include <stddef.h>
#include <stdint.h>
#include "console.h"
#include "fs.h"
#include "process.h"
#include "jobs.h"

#define JOB_MAX 6

static struct job_info jobs[JOB_MAX] = {
    {1, "shell-repl", "interactive", 40, 0, "running"},
    {1, "log-flush", "io", 20, 0, "ready"},
    {1, "math-worker", "scientific", 95, 0, "ready"},
    {0, "", "", 0, 0, ""},
    {0, "", "", 0, 0, ""},
    {0, "", "", 0, 0, ""}
};

static char lower_char(char c){
    if(c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
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

void jobs_init(void){
    fs_append_line("/var/log/system.log", "jobs: scheduler queues online");
}

void jobs_account(const char* name, uint32_t ticks){
    for(size_t i=0; i<JOB_MAX; i++){
        if(jobs[i].active && str_eq(jobs[i].name, name)){
            jobs[i].ticks += ticks;
            jobs[i].state = "running";
            return;
        }
    }
    for(size_t i=0; i<JOB_MAX; i++){
        if(!jobs[i].active){
            jobs[i].active = 1;
            jobs[i].name = name;
            jobs[i].class_name = "scientific";
            jobs[i].priority = 90;
            jobs[i].ticks = ticks;
            jobs[i].state = "running";
            return;
        }
    }
}

const struct job_info* jobs_at(uint32_t index){
    if(index >= JOB_MAX)
        return 0;
    return &jobs[index];
}

uint32_t jobs_count(void){
    return JOB_MAX;
}

void jobs_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "list")){
        for(size_t i=0; i<JOB_MAX; i++)
            if(jobs[i].active){
                console_puts(jobs[i].name);
                console_puts(" class=");
                console_puts(jobs[i].class_name);
                console_puts(" priority=");
                console_write_dec(jobs[i].priority);
                console_puts(" ticks=");
                console_write_dec(jobs[i].ticks);
                console_puts(" state=");
                console_puts(jobs[i].state);
                console_putc('\n');
            }
        return;
    }
    if(str_eq(action, "run")){
        const char* name = first_arg(rest, &rest);
        for(size_t i=0; i<JOB_MAX; i++){
            if(jobs[i].active && str_eq(jobs[i].name, name)){
                jobs[i].ticks += 8;
                jobs[i].state = "complete";
                process_set_running("compute", 1);
                process_tick("compute", 8);
                console_puts("job complete ");
                console_puts(jobs[i].name);
                console_putc('\n');
                return;
            }
        }
        console_puts("job: not found\n");
    } else if(str_eq(action, "priority")){
        const char* name = first_arg(rest, &rest);
        uint32_t prio = parse_u32(rest);
        for(size_t i=0; i<JOB_MAX; i++){
            if(jobs[i].active && str_eq(jobs[i].name, name)){
                jobs[i].priority = prio;
                console_puts("job priority updated\n");
                return;
            }
        }
        console_puts("job: not found\n");
    } else {
        console_puts("usage: job list | run NAME | priority NAME N\n");
    }
}
