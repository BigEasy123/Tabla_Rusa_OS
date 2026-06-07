# Process and Scheduler Subsystem

Tabla Rusa OS now exposes a small structured process layer instead of only a printed task table.

## Process Table

`process.c` tracks built-in kernel, shell, editor, logger, network, compute, and short-lived user processes. Each entry records:

- pid
- name
- kind
- state
- running flag
- priority
- workload class
- tick and switch counters
- last run tick
- parent pid
- estimated memory usage
- foreground/background metadata
- descriptor and process-handle counts

States are:

- `new`
- `ready`
- `running`
- `sleeping`
- `waiting`
- `blocked`
- `zombie`
- `terminated`

## Process APIs

```c
process_init();
process_create("worker", "user", "simulation", 44);
process_spawn("worker");
process_sleep("worker");
process_wake("worker");
process_kill("worker");
process_get_current();
process_list_info(snapshot, PROCESS_MAX);
process_set_priority("compute", 90);
process_set_memory("worker", 123);
process_set_background("worker", 1);
```

`process_stop()` remains the GUI/task-manager stop action. It pauses a process without deleting its table row, so stopped apps can be restarted. `process_kill()` is the destructive termination path.

## IPC and Pipes

The first IPC slice supports local in-kernel messages and tiny named pipe buffers:

```c
ipc_send(1, worker_pid, "hello");
ipc_recv(worker_pid, &message);

int pipe_id = pipe_create(worker_pid);
pipe_write(pipe_id, "result");
pipe_read(pipe_id, buffer, sizeof(buffer));
```

This is intentionally local and cooperative. It is enough for GUI apps, task manager actions, and Rusa runtime experiments to exchange small control messages before a full independent process executor exists.

## Signals

Signals are simple named control messages:

```c
signal_send(worker_pid, "sleep");
signal_send(worker_pid, "wake");
signal_send(worker_pid, "kill");
```

Unknown signal names are delivered through IPC as normal messages.

## Process Handles and Resource Accounting

Process handles give GUI apps, the task manager, and security controls stable references to target processes:

```c
int handle = process_handle_open(1, "compute");
const struct process_info* target = process_handle_get(handle);
process_handle_close(handle);
```

Resource counters are deliberately simple:

- `memory_kib` is an estimate suitable for UI and scheduling policy.
- `handle_count` includes process handles plus file/socket descriptors.
- `parent_pid` records the creator process for spawned user processes.
- `background` lets Task Manager and scheduler policy distinguish service/background work from foreground apps.

## Scheduler Wrappers

The existing cooperative scheduler remains the execution driver. Compatibility APIs now expose the roadmap names:

```c
scheduler_init();
scheduler_tick();
scheduler_pick_next();
scheduler_set_priority("compute", 9);
```

`sched_yield()` and timer dispatch still perform the actual cooperative task run. This keeps current boot behavior stable while providing a cleaner API surface for the GUI, tests, and future runtime work.

## Current Limits

- Processes are still cooperative accounting records, not isolated CPU contexts.
- IPC payloads and pipe buffers are intentionally small.
- Scheduler priority currently maps onto the cooperative quantum.
- No memory protection or per-process address spaces exist yet.
- Memory usage is currently estimated metadata, not measured page ownership.
