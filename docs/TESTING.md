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

Recent full QEMU selftest after Phase 14:

```text
selftest pass=127 fail=0
```

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
