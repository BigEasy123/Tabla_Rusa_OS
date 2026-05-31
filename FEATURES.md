# Tabla Rusa OS Feature Ledger

This file is the living feature ledger for Tabla Rusa OS. Update it after each development prompt.

Last updated: after adding path-aware GUI editor open/save, file-browser-to-editor flow, GUI-visible run surfaces, window geometry controls, and persisted GUI settings.

## Current Kernel Shape

- i386 multiboot2 kernel with VGA text console, terminal scrollback, serial logging, IDT/PIC/timer/keyboard, heap, paging, memory map reporting, and RAM filesystem.
- Native shell/language surface is launched from `kmain.c`, while shell session state, editor state, dispatch bridge, and most OS behaviors live behind subsystem modules.
- Boot includes ASCII intro, a full-stack autostart path, and `/home`, `/system`, `/proc`, `/pkg`, `/dev`, `/var/log`, `/share/docs`, and `/home/math`.
- The boot path now starts networking, GUI compositor state, scheduler state, and the crosshair pointer by default.
- GUI boot writes `/system/gui/state.txt` so autostart state is inspectable after startup.
- `make run` launches the graphical QEMU SDL display so keyboard and pointer focus are more predictable; `make run-gtk` is a graphical fallback; `make run-text` keeps the old curses backend for text-only testing.
- Boot now requests a 1024x768x32 linear framebuffer from GRUB and lands directly in GUI desktop mode.
- GUI boot now starts at the icon desktop with no forced app window, so the wallpaper is visible immediately.
- The shell prompt is hidden until Enter opens the terminal.
- PS/2 mouse setup explicitly enables the AUX device and AUX IRQs through the controller config byte.
- The early boot stack is now 32 KiB, giving the growing GUI and Rusa parser enough room for nested parser/editor work.

## Shell And Native Language

- Unified shell/programming syntax for commands and native object expressions.
- The native language is named **Rusa**.
- User-authored Rusa source files use the `.rusa` extension.
- Rusa source now has its own parser/evaluator separate from TRX bytecode.
- Variables support `let` and `set`, with typed value annotations for `int`, `bool`, and `string`.
- Expressions support literals, variables, arithmetic, comparisons, parentheses, booleans, strings, and function calls.
- Block syntax uses braces, keeping the language readable without indentation dependency.
- Functions support parameters, optional parameter type annotations, `return`, and `call`.
- Loops support `while` and `repeat`; `if`/`else` and `parallel { ... }` blocks are parsed.
- N-dimensional switch blocks support `nswitch`, `case`, `default`, comma-separated dimensions, and `*` wildcards.
- `import std` loads reusable source from `/lib/rusa/std.rusa`.
- `on "trigger" { ... }` registers persistent Rusa source event handlers with the OS event bus.
- Rusa diagnostics now report file, line, column, plain-English explanation, source snippet, caret highlight, and a suggested fix.
- `lang check PATH` validates a `.rusa` file and saves the latest diagnostic.
- `lang scan PATH` reads every source line and reports suspicious code in plain English before it runs.
- `lang run PATH` stops before execution when the scanner finds risky lines such as privacy shutoff, security unlocks, endless loops, dangerous system writes, or network sends.
- `lang last-error` reprints the saved diagnostic.
- `lang open-error` opens the failing file in the editor and highlights the problem line/column.
- Diagnostics currently catch common syntax and runtime mistakes such as missing braces, extra braces, missing parentheses, unclosed strings, missing `=`, missing variable/function names, unknown names, unknown statements, missing imports, and full event/variable tables.
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
lang run /home/projects/demo.rusa
lang check /home/projects/demo.rusa
lang scan /home/projects/demo.rusa
lang last-error
lang open-error
lang eval fn inc(x: int) { return x + 1 } print inc(4)
lang object file
```

Rusa keywords currently documented in `/share/rusa/keywords`:

```text
import let set fn return if else while repeat parallel on run call print true false
file process service window program math phys
```

Example Rusa source:

```text
import std

let count: int = 0

fn hello(name: string) {
  print "hello " + name
}

while count < 2 {
  call hello("rusa")
  set count = count + 1
}

let x: int = 1
let y: int = 2
nswitch x, y {
  case 1, 2 { print "matched point" }
  case 1, * { print "matched row" }
  default { print "no match" }
}

on "fs.write" {
  print "filesystem changed"
}

file["/home/readme.txt"].read()
```

Example diagnostic:

```text
Rusa found a problem
where: /home/projects/bad.rusa:1:12
what: missing equals sign
plain english: Rusa expected '=' before the value you want to store.
source: let broken 3
                   ^
try: Use: let name: int = 1 or set name = name + 1
open: lang open-error
```

Example security scan:

```text
write /home/projects/risky.rusa while true { print "loop" }
lang scan /home/projects/risky.rusa
lang run /home/projects/risky.rusa
lang open-error
```

The scanner reports the file, line, column, readable issue title, and plain-English reason, then keeps the first issue available for editor jumping.

## Console And Boot

- VGA console output now keeps a 256-line terminal scrollback buffer.
- `console_clear_output` starts a clean visible page while keeping older boot text reachable.
- PageUp/PageDown scroll through terminal history.
- Mouse wheel and touchpad scroll gestures now drive the same terminal scrollback.
- The OS crosshair is drawn over the pixel desktop and mirrored into the soft framebuffer debug plane.
- Mouse pointer sync is careful to update both the framebuffer debug plane and the VGA overlay without recursing through the input path.
- The framebuffer cursor now restores the exact pixels under the crosshair, so mouse movement does not leave duplicate cursor trails.
- `scroll up`, `scroll down`, `scroll top`, `scroll bottom`, and `scroll status` are shell commands.
- GUI startup no longer destroys the ability to read boot messages; the boot log remains in scrollback and serial output.
- Full-stack boot prints the ASCII intro, initializes core drivers, brings `net up`, starts the GUI desktop, and hides the terminal prompt.
- Opening Terminal now enables a framebuffer-backed terminal mirror so shell output, Rusa commands, and editor prompts are visible in the graphical QEMU window.
- The Terminal app window now also shows a GUI-side status/result pane for the last launched command before entering the full terminal.
- Closing the last GUI window now returns to the wallpaper desktop instead of leaving an unclosable Terminal panel.

Examples:

```text
scroll status
scroll up
scroll down
scroll top
scroll bottom
mouse scroll up
mouse scroll down
gui status
net status
service list
```

GUI boot controls:

```text
Enter   opens the terminal from the GUI desktop
Esc     returns from the terminal to the GUI desktop
Tab     cycles GUI focus
Arrows  cycle GUI focus
Home/End/Delete move and edit text in GUI editor and shell input
1..4    focus shell, inspector, editor, network panels
5..8    focus files, math, privacy, task manager
S       previews the lava screensaver
Click   focuses GUI app panels; Terminal click opens the shell
Mouse wheel scrolls the GUI editor when the pointer is over the document pane
PageUp/PageDown scroll the GUI editor while it is focused
```

The GUI treats the desktop itself as a first-class no-window state. Calm colored lava wallpaper is on by default; `gui wallpaper off` disables it, `gui wallpaper live MODE` enables slow opt-in animation, and closing the active window reveals the icon desktop again.

GUI app launcher:

```text
gui app files      shows filesystem workspace and opens: tree /home
gui app editor     opens the GUI-friendly hybrid Paper/Code editor
gui app projects   shows Rusa project workspace actions
gui app packages   shows package registry and compatibility actions
gui app logs       shows system/security/network log actions
gui app security   shows secure mode, audit, and user controls
gui app events     shows event rules and emit actions
gui app storage    shows block device, mounts, and file descriptors
gui app tasks      shows process/job/service surface and opens: taskman top
gui app math       shows math/physics catalog and opens: math help
gui app rusa       opens the standalone Rusa Workbench language app
gui app settings   shows high-level privacy/cookie/network controls
gui app privacy    shows master privacy/cookie state and opens: privacy status
gui app net        shows network state and opens: net status
gui app saver      shows screensaver launcher state
gui move active X Y moves the active GUI window frame
gui resize active W H resizes the active GUI window frame
gui cursor dot     high-contrast crosshair with black center dot
gui cursor cross   plain white crosshair
gui cursor target  alias for the dot/target style
gui wallpaper lava  uses lava as calm still desktop wallpaper
gui wallpaper rain  uses rain as calm still desktop wallpaper
gui wallpaper stars uses stars as calm still desktop wallpaper
gui wallpaper waves uses waves as calm still desktop wallpaper
gui wallpaper live lava  enables slow opt-in animated lava wallpaper
gui wallpaper live rain  enables slow opt-in animated rain wallpaper
gui wallpaper MODE live  also enables slow opt-in animation
gui wallpaper off   returns to the plain desktop wallpaper
gui backdrop MODE   alias for gui wallpaper MODE
gui editor paper   switches the Editor app into paper drafting mode
gui editor code    switches the Editor app into code workspace mode
gui editor new [PATH] creates a fresh GUI-side document buffer
gui editor open [PATH] loads a file into the GUI editor
gui editor save [PATH] saves the GUI editor buffer to a file
gui editor copy    copies the current editor line
gui editor paste   pastes over the current editor line
gui rusa examples  switches Rusa Workbench to examples
gui rusa keywords  switches Rusa Workbench to keyword docs
gui rusa docs      switches Rusa Workbench to language docs
gui rusa check     switches Rusa Workbench to source checking
gui rusa run       switches Rusa Workbench to source running
gui rusa diagnostics switches Rusa Workbench to friendly errors
gui close APP      closes a GUI app window; aliases like net/pkg/log work
gui close terminal returns from the Terminal window to the desktop
gui saver lava     previews a pixel lava screensaver
gui saver rain     previews falling rain
gui saver stars    previews drifting stars
gui saver waves    previews soft wave fields
```

Host-side launch commands:

```text
make run
make run-gtk
make run-text
```

For mouse/touchpad testing, click inside the QEMU graphics window first so QEMU owns input focus. Scrolling over the host terminal will scroll the host terminal history, not Tabla Rusa OS.

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
- File descriptors now live in per-process descriptor tables with local fd numbers.
- `fd list` shows the shell process table; `fd list PROC` and `fd all` inspect other process tables.
- `fd openfor PROC PATH MODE` opens a descriptor in another process table for debugging.
- File descriptors record owner PID, descriptor type, mode, path/label, and simple offset accounting.
- Socket creation allocates a descriptor in the owning process table, so files and sockets share the same per-process namespace.
- Socket descriptors can be used through `fd read` and `fd write`.
- Stopping a process releases its descriptor table.
- Block device layer added with an 8-sector RAM disk and save/load bridges.
- Remaining work: real seek offsets, descriptor inheritance, file permissions, and persistent disk storage.

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
fd openfor compute /tmp/compute.txt rw
fd list compute
fd all
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
- `.rusa` files without a `bytecode:` section execute through the Rusa source parser.
- Multi-line source editing helpers exist through `trx append/show/clear/edit`.
- `CALL` bridges bytecode into shell commands, so TRX programs can invoke existing OS services.
- TRX `HALT` now stops bytecode execution instead of only printing a halt line.
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
- Process entries now track context-switch counts and last-run timer ticks.
- Process stop releases descriptors owned by that process.
- Job table tracks named workload classes and tick accounting.
- Scheduler has fixed task contexts for shell, logger, network, gui, compute, and idle.
- Each scheduler task tracks state, quantum, run count, saved program counter, synthetic stack pointer, stack range, wake tick, and task step function.
- Timer interrupts periodically dispatch scheduler tasks through `sched_on_timer`.
- `sched yield`, `sched step`, and `sched run N` manually drive task execution for debugging.
- `sched trace NAME` prints a task context snapshot.
- Task manager aggregates processes, jobs, and services with `taskman top`.
- Remaining work: real CPU register save/restore, separate kernel stacks, blocking waits, and per-process resources.

Examples:

```text
ps
jobs
sched list
sched yield
sched run 4
sched trace compute
sched quantum compute 12
sched sleep network
sched wake network
taskman top
taskman fds
taskman boost
```

## GUI, Framebuffer, Vector Graphics

- Text-mode GUI desktop, tabs, focus, movement, and window list exist.
- The GUI now requests and maps a real Multiboot2 linear framebuffer, then draws a pixel desktop at boot.
- The desktop now follows a more traditional Windows/Ubuntu-style layout: top system bar, wallpaper area, left-side app icons, a centered app window, and a bottom taskbar.
- Basic pictogram icons exist for Files, Terminal, Math, Rusa, Settings, Editor, Network, Saver, Tasks, Projects, Packages, and Logs.
- Framebuffer text now uses a real 5x7 ASCII font for readable desktop labels instead of placeholder patterned glyphs.
- The default GUI pointer is now a white crosshair with a black center dot for better visibility.
- Cursor style can be changed from settings with `gui cursor dot`, `gui cursor cross`, or `gui cursor target`; the framebuffer command `fb cursor ...` exposes the same setting.
- Terminal launch from the GUI switches on the framebuffer terminal renderer, so command output no longer disappears into the old VGA-only text console.
- Non-terminal desktop icons now open GUI app windows instead of immediately dropping into Terminal.
- Each GUI app window has an Open Terminal button for the matching command-line tool.
- App windows can be closed with a larger titlebar close button or `gui close APP`.
- App windows also include an obvious in-window Close Window button.
- App windows can be moved with `gui move active X Y`, resized with `gui resize active W H`, and dragged by the titlebar as an early desktop interaction model.
- A dedicated hybrid Editor desktop app can switch between paper drafting mode and code workspace mode.
- Editor Paper mode opens `/home/notes.txt`; Editor Code mode opens `/home/projects/demo.rusa`.
- The Editor app includes GUI controls for Paper, Code, New, Open, Save, and Open File into the terminal editor.
- The Editor app can now open, create, and save explicit paths such as `gui editor open /home/readme.txt` or `gui editor save /home/projects/scratch.rusa`.
- Clicking inside the Editor document/code pane focuses a GUI-side text buffer.
- While focused, the GUI Editor accepts typed characters, Backspace, Delete, Enter for new lines, arrow/Home/End cursor movement, and PageUp/PageDown scrolling.
- The GUI Editor now uses a 64-line backing buffer with a visible line indicator and scrollbar.
- Mouse wheel/touchpad scroll gestures over the document pane scroll the GUI Editor instead of the terminal history.
- The GUI Editor has simple line copy/paste commands as the first text-selection/editing bridge.
- Save writes the GUI-side buffer back through the RAM filesystem.
- The Files app now lists the current folder, tracks a selected item, enters folders, and opens selected files in the GUI Editor.
- Rusa Workbench is a standalone GUI app for `.rusa` language work, with tabs for examples, keywords, docs, check, run, and diagnostics.
- Rusa Workbench displays GUI-side Check/Run status text and still offers Open Terminal for the full command output.
- Projects, Packages, Logs, Security, Events, and Storage now have GUI app windows with Open Terminal bridges to their subsystem commands.
- Settings includes quick GUI jumps into Security, Events, and Storage.
- Project/package/log/security/event/storage action buttons can launch their matching shell commands directly.
- `gui close` now canonicalizes aliases, so `gui close net`, `gui close pkg`, and similar names close the visible app instead of leaving stale focus.
- Screensaver-inspired wallpapers now render as coherent colored desktop backgrounds instead of scaled grayscale debug rasters.
- `gui wallpaper lava|rain|stars|waves|off` selects a still wallpaper by default; `gui wallpaper live MODE` or `gui wallpaper MODE live` turns on slow opt-in animation.
- `gui backdrop lava|rain|stars|waves|off` remains as a compatibility alias for wallpaper selection.
- The Screensaver app now has a visible Live button that turns on slow animated wallpaper from the current wallpaper mode.
- GUI wallpaper, cursor style, editor mode, and last editor path are persisted in `/config/gui.conf`.
- The GUI idle loop only advances wallpaper frames when live wallpaper is explicitly enabled, heavily throttled to reduce flicker and input lag.
- Live wallpaper pauses briefly after clicks and keystrokes so typing stays smoother.
- GUI editor redraws avoid console status repaint spam while typing, reducing blink.
- The GUI editor caret is aligned to the soft-font glyph baseline.
- Extended keyboard handling now recognizes Home, End, Insert, Delete, right Ctrl, right Alt, and Super keys from PS/2 extended scancodes.
- The Screensaver app exposes visible Lava, Rain, Stars, Waves, Preview, and Off controls.
- `fb status` reports `hardware=on` with the framebuffer address and pitch when GRUB provides the pixel buffer.
- GUI desktop icons are wired to real app focus/open behavior rather than decorative panels.
- Clicking Terminal or the taskbar Start/Term button requests the shell; clicking other desktop icons opens their GUI app window and launches the matching terminal command.
- VGA text output still exists for debugging and fallback, but the first boot surface is now the pixel desktop.
- Vector graphics command surface supports line, rect, circle, and a sample scene.
- Framebuffer layer now has an in-memory soft raster plane backing pixel, rectangle, text, GUI, and screensaver output.
- The logical framebuffer mode remains configurable while the debug raster is stored as a compact 96x54 pixel plane.
- Soft text rendering exists through `fb text` and `fb_draw_text`.
- `gui start` and `gui desktop` rasterize the desktop layout into the hardware framebuffer when available.
- `gui desktop` enters the GUI-first desktop mode and hides the terminal prompt.
- `fb demo` draws a GUI desktop preview into the raster.
- `fb dump [W H]` prints an ASCII luminance preview of the current raster.
- `fb saver lava|rain|stars|waves [N]` advances soothing screensaver frame generators.
- `fb blit` reports the current raster checksum, standing in for compositing to a physical primary buffer.
- Mouse input subsystem tracks crosshair position, buttons, event count, and a light IRQ12 packet path.
- Mouse input now attempts PS/2 wheel mode with ACK-draining setup and maps wheel/touchpad scrolls into terminal scrollback.
- PS/2 setup enables AUX IRQs through the controller config byte before unmasking IRQ12.
- `mouse set/move/click/down/up/status/scroll/wheel` drives the GUI pointer and terminal scroll in testable form.
- The pixel desktop, VGA fallback, and framebuffer dump overlay the pointer as a crosshair instead of an arrow cursor.
- Cursor movement saves the exact hardware pixels under the crosshair and restores them before the next draw, preventing repeated cursor images/trails.
- `gui click X Y` routes a click through the mouse layer and focuses the matching desktop app/window.
- GUI click routing now uses real icon/taskbar hitboxes for Files, Terminal, Math, Rusa, Settings, Tasks, Network, and Saver.
- GUI and network services are now started by the full-stack boot path so the desktop is present immediately.
- Remaining work: dirty rectangles, real window surface compositing, graphical terminal widget, richer app launch actions, and USB tablet support.

Examples:

```text
gui start
gui tab
gui focus editor
gui move editor 4 8
gui click 500 120
window list
mouse status
mouse scroll up 2
mouse scroll down 2
mouse set 160 120
mouse click
gfx line 0 0 12 8
gfx rect 2 2 20 6
gfx circle 12 8 5
gfx scene
fb status
fb mode 800 600 32
fb surface
fb mouse 100 120
fb pixel 4 4 255
fb rect 10 10 40 20 180
fb text 20 20 240 hello-gui
fb demo
fb dump 48 24
fb saver lava 3
fb saver rain 4
fb saver stars 2
fb saver waves 2
fb blit
```

## Networking

- Loopback and eth0 stub state exist, with `net iface` showing interface status.
- IPv4, ARP, route, UDP/TCP surfaces are exposed.
- Network layer now stores real packet records with direction, interface, protocol, src/dst IP, ports, length, checksum, and payload.
- TX/RX loopback queues are represented by packet records; `net packets` and `net trace` dump them.
- Packet checksums are computed for each queued packet and tracked in stats.
- Socket table supports `net socket`, `net connect`, `net send`, `net recv`, and `net sockets` using loopback packet delivery.
- TCP sockets track simple state transitions such as `LISTEN`, `SYN-SENT`, and `ESTABLISHED`.
- UDP sockets use an `OPEN` datagram path through the same packet queues.
- `net stats` reports tx/rx/drops/checksum errors/state changes.
- `net socket` allocates a file descriptor for the socket.
- `fd write SOCKET_FD MSG` and `fd read SOCKET_FD` operate on socket descriptors in the owning process table.
- Network privacy gates block socket/open/send/connect actions when the master privacy or network switch is off.
- DDoS hardening surface includes `net shield`, `net mask`, flood scores per socket, automatic port close after repeated connection/send patterns, and masked IP display by default.
- Remaining work: real NIC driver, ARP cache mutation, ICMP packets, TCP retransmit/window handling, packet ring buffers, and external network I/O.

Examples:

```text
net status
net iface
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
net packets
net trace
net stats
net flush
net sockets
privacy status
net shield status
net shield mask off
```

## Security

- Users, capabilities, secure mode, audit log, and namespace write checks exist.
- Kernel-facing packages are blocked in secure mode.
- Privacy center provides layperson-facing connection controls: master privacy, network access, device access, telemetry, cookie policy, port review, connection review, and program accounting.
- `privacy off` disconnects network sockets, stops the network service, and leaves connection state visible for review.
- `privacy cookies block|ask|allow` records simple high-level cookie policy.
- Rusa source security scanning catches risky control/network/filesystem patterns before source execution.
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
privacy status
privacy off
privacy on
privacy network off
privacy network on
privacy cookies ask
privacy ports
privacy connections
lang scan /home/projects/app.rusa
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
taskman fds
```

## Project Workspace

- `/home/projects` is the Rusa source workspace.
- `project new NAME` creates `/home/projects/NAME.rusa` and `/home/projects/NAME.md`.
- `project new NAME` now writes a real Rusa source template using `import`, `let`, `fn`, `while`, and `call`.
- `project run NAME` executes the `.rusa` source through the loader and Rusa parser.
- `project docs NAME` prints the project notes.

Examples:

```text
project new orbit
project list
project docs orbit
project edit orbit
project run orbit
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
mouse status
mouse set 160 120
mouse click
gui click 500 120
fb demo
fb dump 48 24
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
console scrollback
mousepad terminal scroll
timer/memory/paging
filesystem and file descriptors
per-process descriptor count
program manifests and TRX hello execution
object dispatcher
scheduler yield
scheduler context switch
framebuffer descriptor and raster
hardware framebuffer
mouse crosshair input
GUI icon launch request
GUI projects app
GUI packages app
GUI logs app
GUI security app
GUI close alias
GUI calm wallpaper stable
GUI live wallpaper opt-in
GUI live wallpaper button
GUI editor scroll/write
GUI editor path open/save
GUI file browser edit bridge
GUI window geometry commands
vector/network descriptors
network packet queue
block device descriptor
Rusa docs and stdlib
Rusa source stdlib
Rusa source runtime
Rusa diagnostics
Rusa nswitch
Rusa security scan
Rusa persistent events
Rusa object docs
security mode
```

Some graphical commands such as `gui start` and `gfx scene` intentionally redraw the text desktop. The selftest now checks the framebuffer descriptor and raster checksum directly, while screen-clearing paths remain smoke-tested manually.

Latest GUI smoke checks:

```text
gui app files
gui app editor
gui app math
gui app privacy
gui app net
gui app projects
gui app packages
gui app logs
gui app security
gui app tasks
gui app settings
gui click 60 95
gui click 60 295
gui click 60 495
gui editor code
gui editor open /home/readme.txt
gui editor new /home/projects/scratch.rusa
gui editor save
gui editor copy
gui editor paste
gui rusa docs
gui wallpaper lava
gui wallpaper live rain
gui wallpaper rain live
gui backdrop rain
gui close net
gui click 960 94
gui status
fb status
gui cursor cross
gui cursor dot
gui backdrop waves
gui close editor
gui move active 190 90
gui resize active 700 500
gui saver rain
GUI desktop key 5 + Enter -> tree /home
GUI icon click -> launch request for tree /home
GUI icon click -> app window, Open Terminal button -> command launch request
Enter -> framebuffer terminal -> pwd/lang examples/edit works
```

Latest QEMU selftest result:

```text
selftest pass=65 fail=0
```
