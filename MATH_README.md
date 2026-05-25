# Tabla Rusa OS Math Kernel

The math kernel is a freestanding integer-first command surface for scientific computing experiments inside Tabla Rusa OS.

## Command Families

- `math vec`: fixed-size vector operations
- `math mat`: small matrix operations
- `math num`: number theory and modular arithmetic
- `math group`: finite group tables and unit groups
- `math stats`: simple descriptive statistics
- `math poly`: polynomial evaluation
- `math object`: named vectors and matrices in a tiny live workspace
- `math job`: fixed-size compute job tracking
- `math latex`: LaTeX conversion helpers
- `math rat`: exact rational arithmetic
- `math modmat`: modular 2x2 matrix arithmetic
- `math sym`: tiny symbolic polynomial tools
- `math bench`: simulated benchmark accounting

## Examples

```txt
math vec dot 1 2 3 | 4 5 6
math vec add 1 2 3 | 10 20 30
math vec axpy 2 1 2 3 | 10 10 10
math vec norm2 3 4

math mat det2 1 2 3 4
math mat det3 1 0 0 0 1 0 0 0 1
math mat mul2 1 2 3 4 5 6 7 8
math mat inv2 1 2 3 4
math mat solve2 1 2 3 4 5 6
math mat transpose2 1 2 3 4
math mat charpoly2 1 2 3 4

math num gcd 84 30
math num lcm 21 6
math num modpow 2 10 17
math num prime 97

math group cyclic 5
math group units 12
math group order 12 3

math stats 1 2 3 4 5
math poly eval 2 1 0 -1

math rat add 1/3 1/6
math rat mul 2/5 10/3
math rat reduce 84/126
math rat latex 22/7

math modmat det2 7 1 2 3 4
math modmat mul2 5 1 2 3 4 5 6 7 8
math modmat inv2 11 1 2 3 4

math sym diff 1 0 -1
math sym latex 1 0 -1
math sym simplify 1 0 -1

math object vector a 1 2 3
math object vector b 4 5 6
math object dot a b
math object matrix A 1 2 3 4
math object det A
math object list
math object save A /home/math/A.obj
math object load B /home/math/A.obj

math job submit vec dot 1 2 3 | 4 5 6
math job list
math job run 1
math job run-all
math job priority 1 99
math job result 1
math job clear

math latex vec 1 2 3
math latex mat2 1 2 3 4
math latex frac 22 7
math latex poly 1 0 -1
math latex object A

math bench math
math bench vec
math bench mat
math bench num

math phys grav M m r
math phys electric q1 q2 r
math phys magnetic q v b
math phys energy m v
math phys orbit M r
math phys fields Ex Ey Ez | Bx By Bz
```

## Compute Integration

Math commands update the `compute` process with a workload class such as `vector`, `linear-algebra`, `number-theory`, `group-theory`, `statistics`, or `polynomial`.

Physics commands use integer/scaled first-principles formulas and account work to both the `compute` process and the `physics-worker` job. Inspect the pressure with `taskman top`, `ps`, or `jobs`.

Useful inspection commands:

```txt
compute status
process["compute"].trace()
inspect compute
```

## Current Limits

This first pass is intentionally kernel-friendly:

- integer arithmetic only
- fixed vector size: 8
- fixed matrix support: 2x2 and 3x3 operations
- no heap allocation
- no floating-point dependency

Future directions: rational numbers, modular matrices, Gaussian elimination, Smith normal form, FFT/NTT foundations, sparse vectors, tensor shapes, symbolic expressions, and a real scheduler that prioritizes scientific workloads.

## LaTeX Conversion

The LaTeX converter is intentionally direct: it prints TeX fragments to the console.

Examples:

```txt
math latex vec 1 2 3
math latex mat2 1 2 3 4
math latex frac 1 3
math latex poly 1 0 -1
```

Named objects can be converted too:

```txt
math object matrix A 1 2 3 4
math latex object A
```
