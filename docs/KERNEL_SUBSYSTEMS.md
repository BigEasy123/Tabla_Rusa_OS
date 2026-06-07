# Kernel Subsystems

Runtime descriptors are exposed inside the OS:

```text
/system/kernel/modules.txt
/system/kernel/boundaries.txt
/system/gui/state.txt
/system/boot/startup.txt
/system/net/stack.txt
/system/input/keyboard.txt
/var/log/system.log
/var/log/security.log
/var/log/network.log
```

## Boundaries

- Boot orchestration stays in `kmain.c`.
- Command/session state belongs in `shell.c`, `editor.c`, `lang.c`, or app modules.
- GUI window routing, z-order, and app composition remain in `gui.c`, while GUI Terminal state/input/history/clipboard/capture now lives in `gui_terminal.c`.
- File, process, network, security, and math state should expose typed accessors rather than requiring console output parsing.
- RAMFS nodes expose simple read/write/execute permission bits, and file descriptors enforce those bits for open/read/write paths.
- File descriptors support open/read/write/append/seek/tell/chunk/partial-write/dup/inherit/close operations over RAMFS files and socket descriptor labels.
- Process lifecycle, local IPC, pipe buffers, and simple signals live in `process.c`; scheduler wrappers live in `sched.c`.
- Network and security policy checks live in `net.c`, `security.c`, and `policy.c`, with GUI/terminal code calling those APIs instead of duplicating policy.
- Research project/notebook/dataset/experiment metadata lives in `research.c`, separate from the older source-project scaffolder in `project.c`.
- Science units/constants/arrays/fitting/signal/spectroscopy/crystals/simulations live in `science.c`, with `mathlib.c` remaining the older command-heavy math surface.
- Math plugins, theorem metadata, algorithm metadata, and object-type metadata live in `mathcore.c`; `mathlib.c` should call or dispatch through this registry as it is split up.
- Active proof states, assumptions, steps, replay, explanations, and export live in `proofcore.c`; theorem metadata stays in `mathcore.c`.

## Current Technical Debt

- Legacy package, mount, user, service, and event scaffolds still exist in `kmain.c`.
- Most GUI apps are still centralized in `gui.c`; GUI Terminal is the first extracted app-state module.
- Network is loopback/socket-like, not a hardware NIC driver.
- Security scanning is lightweight pattern detection, not comprehensive malware analysis.
- Process execution is accounting/cooperative, not independent preemptive execution.
- IPC and pipe buffers are intentionally tiny local-kernel scaffolds, not full POSIX process communication yet.
- File descriptors do not yet support persistent disk-backed storage; RAMFS permissions do not yet include owners, groups, or inherited directory defaults.
- Science fitting and FFT paths are fixed-point scaffolds, not production numerical libraries yet.
- MathCore is a registry and dispatch scaffold; individual plugin engines still need real callback implementations.
- ProofCore validates a small tactic subset and must not be treated as a complete proof assistant yet.
