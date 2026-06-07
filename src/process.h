#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>

#define PROCESS_MAX 10

enum process_state {
    PROCESS_NEW,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_WAITING,
    PROCESS_BLOCKED,
    PROCESS_ZOMBIE,
    PROCESS_TERMINATED
};

struct process_info {
    const char* name;
    uint32_t pid;
    uint32_t parent_pid;
    int running;
    enum process_state state;
    const char* kind;
    uint32_t priority;
    uint32_t cpu_hint;
    uint32_t memory_kib;
    uint32_t handle_count;
    int background;
    const char* workload;
    uint32_t ticks;
    uint32_t switches;
    uint32_t last_run_tick;
};

struct ipc_message_info {
    uint32_t from_pid;
    uint32_t to_pid;
    char payload[96];
    int used;
};

struct process_handle_info {
    int id;
    uint32_t owner_pid;
    uint32_t target_pid;
    int valid;
};

void process_init(void);
struct process_info* process_find(const char* name);
const struct process_info* process_at(uint32_t index);
uint32_t process_count(void);
void process_set_running(const char* name, int running);
void process_set_compute(const char* name, const char* workload, uint32_t priority, uint32_t cpu_hint);
void process_tick(const char* name, uint32_t ticks);
void process_context_switch(const char* name, uint32_t tick);
void process_list(void);
void process_compute_report(void);
int process_stop(const char* name);
const char* process_state_name(enum process_state state);
int process_create(const char* name, const char* kind, const char* workload, uint32_t priority);
int process_spawn(const char* name);
int process_kill(const char* name);
int process_sleep(const char* name);
int process_wake(const char* name);
void process_yield(void);
const struct process_info* process_get_current(void);
uint32_t process_list_info(struct process_info* out, uint32_t max);
int process_set_priority(const char* name, uint32_t priority);
int process_set_memory(const char* name, uint32_t memory_kib);
int process_set_background(const char* name, int background);
uint32_t process_memory_total_kib(void);
uint32_t process_handles_for_pid(uint32_t pid);
int process_handle_open(uint32_t owner_pid, const char* target_name);
int process_handle_close(int handle_id);
const struct process_info* process_handle_get(int handle_id);
uint32_t process_handle_list(struct process_handle_info* out, uint32_t max);
int ipc_send(uint32_t from_pid, uint32_t to_pid, const char* payload);
int ipc_recv(uint32_t to_pid, struct ipc_message_info* out);
int pipe_create(uint32_t owner_pid);
int pipe_write(uint32_t pipe_id, const char* payload);
int pipe_read(uint32_t pipe_id, char* out, uint32_t out_max);
int signal_send(uint32_t to_pid, const char* signal);

#endif
