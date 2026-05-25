# Tabla Rusa OS Feature Ledger

This file is the living feature ledger for Tabla Rusa OS. Update it after each development prompt.

## Current Kernel Shape

- i386 multiboot2 kernel with VGA text console, serial logging, IDT/PIC/timer/keyboard, heap, paging, memory map reporting, and RAM filesystem.
- Native shell/language surface is still launched from `kmain.c`, but most OS behaviors now live behind subsystem modules.
- Boot includes ASCII intro and `/home`, `/system`, `/proc`, `/pkg`, `/dev`, `/var/log`, `/share/docs`, and `/home/math`.

## Shell And Native Language

- Unified shell/programming syntax for commands and native object expressions.
- Command history, cursor-aware input editing, and an editor prompt.
- Native forms include `inspect memory`, `spawn editor`, and object expressions such as `file["/home/a"].read()`.
- New unified object dispatcher supports `file`, `process`, `service`, `window`, and `program` methods through `object eval`.
- Remaining cleanup: move shell dispatch and editor state fully out of `kmain.c`.

## Editor

- Numbered line editor with Esc and `:q` exit.
- Up/Down moves between lines; Left/Right moves inside a line.
- Typing inserts at the cursor; Backspace deletes before the cursor.
- `:w`, `.w`, `.save`, `.show`, and `.clear` are supported.

## Filesystem And VFS

- RAM filesystem supports directories, files, read/write/append, copy, move, stat, tree, and current working directory.
- VFS layer tracks ramfs, procfs, sysfs, devfs, pkgfs, and mathfs namespaces.
- Protected namespace write checks exist for `/proc`, `/system`, and `/boot`.
- File descriptor layer added with `fd open/read/write/close/list`.
- Remaining work: per-process descriptor tables, seek offsets, file permissions, and persistent block storage.

## Loader And Runtime

- `/bin/*.trx` executable manifests are generated at boot.
- `loader list/info/bytecode/run` and `run PROGRAM` commands exist.
- TRX1 bytecode interpreter supports `PRINT`, `READ`, `JOB`, `SERVICE`, `TICK`, and `HALT`.
- Remaining work: richer TRX instruction set, shell-call bridge, bytecode files loaded from fs content, and eventually ELF loading.

## Scheduler, Processes, Jobs, Task Manager

- Process table tracks pid, running state, priority, workload, CPU hints, and ticks.
- Job table tracks named workload classes and tick accounting.
- Cooperative scheduler queue added with `sched list/yield/wake/sleep/quantum`.
- Task manager aggregates processes, jobs, and services with `taskman top`.
- Remaining work: actual task contexts, stacks, context switching, blocking waits, and per-process resources.

## GUI, Framebuffer, Vector Graphics

- Text-mode GUI desktop, tabs, focus, movement, and window list exist.
- Vector graphics command surface supports line, rect, circle, and a sample scene.
- Framebuffer descriptor layer tracks mode, surfaces, mouse coordinates, font/blit hooks.
- Remaining work: real multiboot framebuffer discovery, pixel plotting, bitmap font renderer, mouse driver, and window surface compositing.

## Networking

- Loopback and eth0 stub state exist.
- IPv4, ARP, route, UDP/TCP surfaces are exposed.
- Socket table supports `net socket`, `net connect`, `net send`, `net recv`, and `net sockets` using loopback buffers.
- Remaining work: packet buffers, checksums, protocol state machines, NIC driver, and real sockets as file descriptors.

## Security

- Users, capabilities, secure mode, audit log, and namespace write checks exist.
- Kernel-facing packages are blocked in secure mode.
- Remaining work: enforce security through all object paths, per-process credentials, signed packages, and syscall boundary.

## Package Manager

- Package manifests in `/pkg`.
- Commands support list/info/install/remove/compat.
- Compatibility fields track target ABI and architecture.

## Math And Science

- Math commands cover vectors, matrices, rationals, modular matrices, number theory, group theory, stats, polynomials, symbolic polynomial tools, LaTeX conversion, math objects, math jobs, and benchmarks.
- `/home/math` acts as workspace storage for math objects.
- Physics section supports scaled first-principles gravity, electric, magnetic, kinetic energy, orbital velocity, and field energy commands.
- Math and physics update compute process/job accounting.

## Useful Smoke Test Commands

```text
test
structure
fd open /home/readme.txt r
loader list
loader bytecode physics
run physics
object eval file["/home/readme.txt"].open()
sched list
sched yield
fb status
fb surface
net up
net socket tcp 8080
net send 0 hello
net recv 0
taskman top
```
