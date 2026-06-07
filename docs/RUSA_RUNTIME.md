# Rusa Runtime API

Rusa is the native Tabla Rusa OS language. The current implementation is an interpreter with a public runtime pipeline API layered over it. The API gives future GUI tools, packages, tests, and OS automation a stable place to call into Rusa without parsing console output.

## Public Pipeline

```c
struct rusa_lexer lexer;
struct rusa_ast ast;
struct rusa_bytecode bytecode;
struct rusa_vm vm;

rusa_lexer_init(&lexer, "let n: int = 1");
rusa_lexer_next_token(&lexer);

rusa_parse_source(source, "/home/app.rusa", &ast);
rusa_typecheck(&ast);
rusa_compile(&ast, &bytecode);
rusa_vm_init(&vm);
rusa_vm_execute(&vm, &bytecode, "");
rusa_ast_free(&ast);
```

Convenience calls:

```c
rusa_eval_source("print 1", "<eval>", "");
rusa_repl_step("print 2");
rusa_register_native("os.tick", native_fn);
rusa_import_module("std");
rusa_native_count();
```

## Current Meaning

- Lexer: real token stream for keywords, identifiers, numbers, strings, symbols, comments, line, and column tracking.
- Parser: scaffold that validates braces/parentheses/strings and counts statement-like tokens into a small AST descriptor.
- Typecheck: preflight validation scaffold.
- Compile: bytecode descriptor scaffold that points at source and tracks an operation count.
- VM execute: delegates to the existing Rusa interpreter, preserving current language behavior.
- Imports: load `/lib/rusa/NAME.rusa` through the existing module path.
- Native registry: stores native function names for future host calls.

This is not a finished independent bytecode compiler or VM yet. It is the smallest tested API boundary that lets later work replace internals without changing callers.

## Tested Language Features

The interpreter already covers:

- variables and typed values
- expressions
- functions and calls
- `if`, `while`, `repeat`, and `return`
- real brace blocks
- imports
- persistent event handlers
- readable diagnostics with source locations
- suspicious-code preflight scanning
- `nswitch` n-dimensional switch blocks

## Remaining Work

- Real AST node allocation.
- Symbol tables and nested scope objects.
- Type checker beyond syntax/preflight checks.
- Real bytecode format and VM instruction loop.
- Native function calls from Rusa source.
- Standard library package namespaces such as `std.fs`, `std.gui`, `std.net`, and `std.notebook`.
