# Testing

Build from the repository root:

```sh
make
make iso
```

Run the graphical desktop:

```sh
make run
```

Run the text/curses backend for serial-style selftests:

```sh
make run-text
```

Inside Tabla Rusa OS, run:

```text
test
```

Recent full QEMU selftest after the networking/security, expanded research framework, Rusa-runtime API, science-engine, MathCore registry, ProofCore, and process/scheduler API pass:

```text
selftest pass=260 fail=0
```

The network/security expansion adds tests for socket lifecycle, bind/listen/connect, loopback send/receive, connection and port tables, idle shutdown, permissions, firewall rules, policy checks, structured events, scanner output, service registry, and Security Center panels. The research-framework pass adds tests for project creation, notebook cells, dataset registry entries, experiment tracking, result recording, dataset aliases, citations, notebook create/add/run/export, LaTeX export, table listing, saved project manifests, tasks, timeline entries, and graph relations. The Rusa-runtime pass adds tests for lexer tokens, parse/typecheck/compile descriptors, VM execute, eval, REPL, native registration, and import APIs. The science-engine pass adds tests for units, dimensions, constants, arrays, fitting, smoothing, FFT scaffold output, spectroscopy peaks, crystal lattices, simulation job accounting, quantum states, materials records, band points, and phonon modes. The MathCore pass adds tests for plugin listing/find/dispatch, theorem status guarding, algorithm registry, object type registry, and plugin test hooks. The ProofCore pass adds tests for proof creation, assumptions, invalid/valid step status, incomplete split steps, state listing, export, and replay failure on invalid proofs. The process/scheduler pass adds tests for process create/spawn/sleep/wake/kill, structured listing, current-process lookup, priority setting, local IPC, pipes, signals, scheduler compatibility wrappers, process memory/background metadata, and process handles. The GUI-terminal pass adds line selection, copy, and paste coverage. The Settings privacy pass adds coverage for master, network, cookie, device, and telemetry toggles backed by privacy APIs. The descriptor pass adds seek/tell, chunk reads, and append-mode coverage.

Useful manual GUI checks:

```text
gui app terminal
gui app files
gui app editor
gui app math
gui app security
gui network open
gui network send
gui boot safe
gui boot recovery
```
