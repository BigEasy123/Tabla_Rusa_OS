#include "console.h"
#include "fs.h"
#include "jobs.h"
#include "process.h"
#include "service.h"
#include "taskman.h"

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

void taskman_init(void){
    fs_append_line("/var/log/system.log", "taskman: process/job/service view online");
}

void taskman_cmd(char* arg){
    char* rest;
    const char* action = first_arg(arg, &rest);
    if(action[0] == 0 || str_eq(action, "top")){
        console_puts("TASK MANAGER\n");
        console_puts("processes:\n");
        process_list();
        console_puts("jobs:\n");
        char list[] = "list";
        jobs_cmd(list);
    } else if(str_eq(action, "ps")){
        process_list();
    } else if(str_eq(action, "jobs")){
        char list[] = "list";
        jobs_cmd(list);
    } else if(str_eq(action, "services")){
        char list[] = "list";
        service_cmd(list);
    } else if(str_eq(action, "kill")){
        const char* name = first_arg(rest, &rest);
        if(process_stop(name) != 0)
            console_puts("taskman: protected or unknown process\n");
        else {
            console_puts("taskman: stopped ");
            console_puts(name);
            console_putc('\n');
        }
    } else if(str_eq(action, "boost")){
        process_set_running("compute", 1);
        process_set_compute("compute", "boosted-taskman-compute", 99, 0);
        jobs_account("math-worker", 4);
        console_puts("taskman: compute process boosted priority=99\n");
    } else {
        console_puts("usage: taskman top | ps | jobs | services | kill NAME | boost\n");
    }
}
