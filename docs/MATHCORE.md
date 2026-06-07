# MathCore Plugin Registry

`mathcore.c` / `mathcore.h` are the first plugin-registry layer for the Math / Physics Lab. The older `mathlib.c` still owns many commands, but new math infrastructure should register through MathCore so GUI panels, Rusa packages, theorem tools, and notebooks can discover capabilities without scraping console output.

## Plugin API

```c
mathcore_init();
math_plugin_register("logic-proof", "0.1", "propositions proof steps", "intro exact apply", "Math Lab/Proof");
math_plugin_find("logic-proof", &plugin);
math_plugin_list(out, max);
math_plugin_dispatch("physics", "status", out, out_max);
math_plugin_run_tests("logic-proof");
```

Seeded plugins include:

- symbolics
- numerical
- linear-algebra
- abstract-algebra
- graph-theory
- logic-proof
- number-theory
- topology
- statistics
- physics
- visualization
- notebook

## Theorem Registry

```c
math_plugin_register_theorem(plugin, name, field, statement, status);
math_theorem_list(out, max);
```

Theorem statuses:

```text
stated
proof sketch
formally checked
computationally verified
assumed axiom
external reference
```

Safety rule: a theorem requested as `formally checked` is downgraded to `stated` unless it comes from the `logic-proof` plugin. This prevents the OS from claiming formal proof status without a checker path.

## Algorithm Registry

```c
math_plugin_register_algorithm(plugin, name, field, input_types, output_types, complexity, status);
math_algorithm_list(out, max);
```

Seeded examples include Bisection, Gaussian elimination, Breadth-first search, Term rewriting, and a Modus ponens checker.

## Object Type Registry

```c
math_plugin_register_object_type(plugin, name, display, latex_hint);
math_object_type_list(out, max);
```

Seeded object types include Expression, Vector, Matrix, Group, Graph, and Theorem.

## Terminal Commands

```text
mathcore plugins
mathcore theorems
mathcore algorithms
mathcore objects
mathcore run physics status
mathcore test logic-proof
```

`mplugin` is an alias.

## Remaining Work

- Real plugin dispatch into each math engine.
- Live proof-state objects and proof replay.
- Theorem search, dependency graph, and proof checker integration.
- Algorithm runnable callbacks and parameter schemas.
- GUI Math Lab plugin browser.
