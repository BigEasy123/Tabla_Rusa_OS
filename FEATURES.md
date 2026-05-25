# Tabla Rusa OS Feature Ledger

This file is the living feature ledger for Tabla Rusa OS. Update it after each development prompt.

Last updated: after `.rusa` source files, multi-line Rusa tools, project workspace, package hooks, socket FD I/O, and framebuffer dump pass.

## Current Kernel Shape

- i386 multiboot2 kernel with VGA text console, serial logging, IDT/PIC/timer/keyboard, heap, paging, memory map reporting, and RAM filesystem.
- Native shell/language surface is launched from `kmain.c`, while shell session state, editor state, dispatch bridge, and most OS behaviors live behind subsystem modules.
- Boot includes ASCII intro and `/home`, `/system`, `/proc`, `/pkg`, `/dev`, `/var/log`, `/share/docs`, and `/home/math`.

## Shell And Native Language

- Unified shell/programming syntax for commands and native object expressions.
- The native language is named **Rusa**.
- User-authored Rusa source files use the `.rusa` extension.
- Command history, cursor-aware input editing, and an editor prompt.
- Shell input/session state lives in `shell.c`.
- Native forms include `inspect memory`, `spawn editor`, and object expressions such as `file["/home/a"].read()`.
- New unified object dispatcher supports `file`, `process`, `service`, `window`, and `program` methods through `object eval`.
- Shell dispatch now goes through `shell_eval`; a compatibility handler still owns some legacy command bodies while the command table is being peeled out.
- Remaining cleanup: migrate each legacy command body out of `kmain.c` into subsystem modules.

Examples:

```text
help
structure
inspect memory
inspect compute
spawn editor /home/notes.txt
object types
object eval file["/home/readme.txt"].open()
object eval process["compute"].trace()
object eval service["network"].start()
object eval program["physics"].run()
lang about
lang keywords
lang examples
lang std
lang import std
lang object file
```

Rusa keywords currently documented in `/share/rusa/keywords`:

```text
inspect spawn on if else parallel run file process service window program math phys
```

## Editor

- Numbered line editor with Esc and `:q` exit.
- Editor state, rendering, save/load, movement, and evaluation live in `editor.c`.
- Up/Down moves between lines; Left/Right moves inside a line.
- Typing inserts at the cursor; Backspace deletes before the cursor.
- `:w`, `.w`, `.save`, `.show`, and `.clear` are supported.

Examples:

```text
edit /home/notes.txt
:w
:q
```

Inside the editor, use arrow keys to move between and within lines. Enter saves the current line.

## Filesystem And VFS

- RAM filesystem supports directories, files, read/write/append, copy, move, stat, tree, and current working directory.
- VFS layer tracks ramfs, procfs, sysfs, devfs, pkgfs, and mathfs namespaces.
- Protected namespace write checks exist for `/proc`, `/system`, and `/boot`.
- File descriptor layer added with `fd open/read/write/close/list`.
- File descriptors record owner PID and descriptor type.
- Socket creation allocates a descriptor, so files and sockets start sharing one descriptor namespace.
- Socket descriptors can be used through `fd read` and `fd write`.
- Block device layer added with an 8-sector RAM disk and save/load bridges.
- Remaining work: true per-process descriptor tables, seek offsets, file permissions, and persistent disk storage.

Examples:

```text
pwd
tree /
write /home/demo.txt hello
cat /home/demo.txt
fd open /home/demo.txt rw
fd write 0 updated-through-fd
fd read 0
fd close 0
block status
block write 1 sector-data
block read 1
block save 1 /mnt/disk/sector1.txt
write /system/nope.txt blocked
```

## Loader And Runtime

- `/bin/*.trx` executable manifests are generated at boot.
- `loader list/info/bytecode/run` and `run PROGRAM` commands exist.
- TRX1 bytecode interpreter supports `PRINT`, `READ`, `CALL`, `JOB`, `SERVICE`, `TICK`, and `HALT`.
- `/bin/*.trx` files now contain a `bytecode:` section, and the loader executes bytecode from file content before falling back to built-in defaults.
- User TRX helpers exist through `loader new NAME`, `loader write NAME LINE`, and `run NAME`.
- User programs are saved as `/home/projects/NAME.rusa`.
- Multi-line source editing helpers exist through `trx append/show/clear/edit`.
- `CALL` bridges bytecode into shell commands, so TRX programs can invoke existing OS services.
- Remaining work: richer TRX instruction set, bytecode files loaded from fs content, and eventually ELF loading.

Examples:

```text
loader list
loader info physics
loader bytecode physics
run physics
run mathbench
run paint
loader new demo
loader write demo CALL lang about
trx append demo CALL lang keywords
trx show demo
trx edit demo
run demo
```

Example TRX bytecode:

```text
PRINT physics-worker
CALL math phys fields 1 2 3 | 4 5 6
JOB physics-worker 24
TICK 24
```

## Scheduler, Processes, Jobs, Task Manager

- Process table tracks pid, running state, priority, workload, CPU hints, and ticks.
- Job table tracks named workload classes and tick accounting.
- Cooperative scheduler queue added with `sched list/yield/wake/sleep/quantum`.
- Task manager aggregates processes, jobs, and services with `taskman top`.
- Remaining work: actual task contexts, stacks, context switching, blocking waits, and per-process resources.

Examples:

```text
ps
jobs
sched list
sched yield
sched quantum compute 12
sched sleep network
sched wake network
taskman top
taskman boost
```

## GUI, Framebuffer, Vector Graphics

- Text-mode GUI desktop, tabs, focus, movement, and window list exist.
- Vector graphics command surface supports line, rect, circle, and a sample scene.
- Framebuffer descriptor layer tracks mode, surfaces, mouse coordinates, font/blit hooks.
- Framebuffer tracks pixel and rectangle draw calls with `fb pixel` and `fb rect`.
- Draw calls update a tiny stored pixel buffer; `fb blit` reports a checksum.
- Remaining work: real multiboot framebuffer discovery, actual pixel plotting, bitmap font renderer, mouse driver, and window surface compositing.

Examples:

```text
gui start
gui tab
gui focus editor
gui move editor 4 8
window list
gfx line 0 0 12 8
gfx rect 2 2 20 6
gfx circle 12 8 5
gfx scene
fb status
fb mode 800 600 32
fb surface
fb mouse 100 120
fb pixel 4 4 255
fb rect 10 10 40 20
fb dump
fb blit
```

## Networking

- Loopback and eth0 stub state exist.
- IPv4, ARP, route, UDP/TCP surfaces are exposed.
- Socket table supports `net socket`, `net connect`, `net send`, `net recv`, and `net sockets` using loopback buffers.
- `net socket` allocates a file descriptor for the socket.
- `fd write SOCKET_FD MSG` and `fd read SOCKET_FD` operate on socket descriptors.
- Remaining work: real packet buffers, checksums, protocol state machines, and NIC driver.

Examples:

```text
net status
net up
net arp
net route
net tcp 8080
net open tcp 8080
net fd 0
fd list
fd write 0 hello-through-fd
fd read 0
net connect 0 8081
net send 0 hello-loopback
net recv 0
net sockets
```

## Security

- Users, capabilities, secure mode, audit log, and namespace write checks exist.
- Kernel-facing packages are blocked in secure mode.
- Remaining work: enforce security through all object paths, per-process credentials, signed packages, and syscall boundary.

Examples:

```text
security status
user list
cap list
write /system/nope.txt blocked
security audit
security unlock
pkg install gui-core
security lock
```

## Package Manager

- Package manifests in `/pkg`.
- Commands support list/info/install/remove/compat.
- Compatibility fields track target ABI and architecture.
- Rusa language packages are registered: `rusa-core`, `rusa-stdlib`, and `rusa-docs`.
- Installing `rusa-core`, `rusa-stdlib`, or `rusa-docs` repopulates Rusa docs and stdlib files.
- Installing `gui-core` and `net-tcpip` refreshes their foundation files.

Examples:

```text
pkg list
pkg info editor
pkg compat
pkg info rusa-core
pkg info rusa-stdlib
pkg info rusa-docs
package["editor"].install()
pkg remove editor
```

## Math And Science

- Math commands cover vectors, matrices, rationals, modular matrices, number theory, group theory, stats, polynomials, symbolic polynomial tools, LaTeX conversion, math objects, math jobs, and benchmarks.
- `/home/math` acts as workspace storage for math objects.
- Physics section supports scaled first-principles gravity, electric, magnetic, kinetic energy, orbital velocity, and field energy commands.
- Math and physics update compute process/job accounting.

Examples:

```text
math vec dot 1 2 3 | 4 5 6
math mat det2 1 2 3 4
math num gcd 252 105
math group cyclic 5
math object vector a 1 2 3
math object save a /home/math/a.obj
math latex vec 1 2 3
math phys grav 10 20 5
math phys electric 3 -4 2
math phys magnetic 2 7 5
math phys fields 1 2 3 | 4 5 6
taskman top
```

## Project Workspace

- `/home/projects` is the Rusa source workspace.
- `project new NAME` creates `/home/projects/NAME.rusa` and `/home/projects/NAME.md`.
- `project run NAME` executes the `.rusa` source through the loader.
- `project docs NAME` prints the project notes.

Examples:

```text
project new orbit
project list
project docs orbit
trx append orbit CALL math phys orbit 10 5
project run orbit
project edit orbit
```

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

The `test` command now checks the major non-interactive surfaces:

```text
console
timer/memory/paging
filesystem and file descriptors
program manifests and TRX hello execution
object dispatcher
scheduler yield
framebuffer/vector/network descriptors
block device descriptor
Rusa docs and stdlib
Rusa object docs
security mode
```

Some graphical commands such as `gui start` and `gfx scene` intentionally redraw the text desktop, so the selftest checks their manifests/descriptors instead of running every screen-clearing path.

Latest QEMU selftest result:

```text
selftest pass=28 fail=0
```
