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

Recent full QEMU selftest after the networking/security, research-framework, Rusa-runtime API, and science-engine pass:

```text
selftest pass=185 fail=0
```

The network/security expansion adds tests for socket lifecycle, bind/listen/connect, loopback send/receive, connection and port tables, idle shutdown, permissions, firewall rules, policy checks, structured events, scanner output, service registry, and Security Center panels. The research-framework pass adds tests for project creation, notebook cells, dataset registry entries, experiment tracking, result recording, table listing, and saved project manifests. The Rusa-runtime pass adds tests for lexer tokens, parse/typecheck/compile descriptors, VM execute, eval, REPL, native registration, and import APIs. The science-engine pass adds tests for units, dimensions, constants, arrays, fitting, smoothing, FFT scaffold output, spectroscopy peaks, crystal lattices, and simulation job accounting.

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
