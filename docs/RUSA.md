# Rusa Language Notes

Rusa is the native language/shell surface for Tabla Rusa OS. Source files use `.rusa`.

## Core Syntax

```rusa
import std

let count: int = 0

fn hello(name: string) {
  print "hello " + name
}

while count < 3 {
  call hello("rusa")
  set count = count + 1
}
```

## Features

- Variables: `let` and `set`
- Types: `int`, `bool`, `string`
- Functions: `fn`, parameters, optional annotations, `return`, `call`
- Blocks: `{ ... }`
- Control flow: `if`, `else`, `while`, `repeat`, `parallel`
- Imports: `import std`
- Persistent event handlers: `on "event.name" { ... }`
- Object syntax: `file["/home/readme.txt"].read()`
- N-dimensional switch: `nswitch x, y { case 1, * { ... } default { ... } }`

## Diagnostics

Use:

```text
lang check /home/projects/demo.rusa
lang scan /home/projects/demo.rusa
lang last-error
lang open-error
```

Diagnostics include file, line, column, plain-English explanation, a source snippet, a caret, and a suggested fix.
