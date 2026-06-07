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
- GUI behavior belongs in `gui.c` until individual app modules are split out.
- File, process, network, security, and math state should expose typed accessors rather than requiring console output parsing.
- Network and security policy checks live in `net.c`, `security.c`, and `policy.c`, with GUI/terminal code calling those APIs instead of duplicating policy.
- Research project/notebook/dataset/experiment metadata lives in `research.c`, separate from the older source-project scaffolder in `project.c`.
- Science units/constants/arrays/fitting/signal/spectroscopy/crystals/simulations live in `science.c`, with `mathlib.c` remaining the older command-heavy math surface.

## Current Technical Debt

- Legacy package, mount, user, service, and event scaffolds still exist in `kmain.c`.
- GUI apps are still centralized in `gui.c`.
- Network is loopback/socket-like, not a hardware NIC driver.
- Security scanning is lightweight pattern detection, not comprehensive malware analysis.
- Process execution is accounting/cooperative, not independent preemptive execution.
- Science fitting and FFT paths are fixed-point scaffolds, not production numerical libraries yet.
