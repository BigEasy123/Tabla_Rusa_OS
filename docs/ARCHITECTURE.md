# Tabla Rusa OS Architecture

Tabla Rusa OS is a small i386 experimental OS where the shell, GUI, Rusa language, and system objects share one command/runtime surface.

## Boot Shape

- `kmain.c` still orchestrates boot and holds legacy compatibility dispatch.
- Drivers and subsystems initialize into inspectable RAM filesystem descriptors under `/system`, `/proc`, `/dev`, `/var/log`, and `/share`.
- GUI boot is the default path. Recovery and safe graphics are available through `gui boot recovery` and `gui boot safe`.

## Main Subsystems

- `shell.c`: shell session, history, prompt, and eval bridge.
- `editor.c`: command-line editor state.
- `gui.c`: framebuffer desktop, window manager, GUI apps, input routing, wallpaper/screensaver state.
- `lang.c`: Rusa parser, evaluator, diagnostics, imports, event handlers, object syntax.
- `fs.c`, `vfs.c`, `block.c`, `fd.c`: RAM filesystem, VFS descriptors, block surface, and file/socket descriptors.
- `process.c`, `sched.c`, `jobs.c`, `taskman.c`: process lifecycle table, cooperative scheduler accounting, local IPC/pipes/signals, job queues, and task manager.
- `net.c`: loopback packet queues, socket-like handles, shield/rate-limit controls, and IP masking.
- `security.c`, `privacy.c`, `service.c`, `policy.c`: capabilities, permissions, privacy controls, services, audit events, and firewall/resource policy.
- `research.c`: project metadata, notebook cells, datasets, experiments, results, and saved research manifests.
- `science.c`: units, constants, numerical arrays, fitting, signal scaffolds, spectroscopy peaks, crystal lattices, and simulation jobs.
- `mathcore.c`: Math Lab plugin registry, theorem metadata, algorithm metadata, and math object-type registry.
- `proofcore.c`: active proof states, assumptions, proof steps, proof replay, explanations, suggestions, and export.
- `mathlib.c`: math, physics, LaTeX, proof/logic helpers, and compute accounting.

## Cleanup Direction

Keep new behavior out of `kmain.c` unless it is true boot orchestration. Add reusable APIs to subsystem modules and expose descriptors in the RAM filesystem when useful.
