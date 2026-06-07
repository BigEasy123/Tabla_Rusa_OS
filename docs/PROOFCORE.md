# ProofCore

`proofcore.c` / `proofcore.h` provide the first live proof-state engine for Tabla Rusa OS. MathCore owns theorem and plugin metadata; ProofCore owns active proofs, assumptions, proof steps, status, explanations, replay, and export.

## API

```c
proofcore_init();
proof_create("demo", "Q", PROOF_MODE_GUIDED);
proof_add_assumption(proof_id, "P");
proof_add_step(proof_id, "exact", "P");
proof_get_state(proof_id, &state);
proof_list_states(out, max);
proof_list_steps(proof_id, out, max);
proof_replay(proof_id);
proof_export_text(proof_id, out, max);
proof_status_text(status);
```

## Modes

```text
strict
guided
educational
informal
```

The current command path creates guided proofs. Informal steps can be marked `unknown`, but they are not treated as formally verified.

## Step Status

```text
valid
invalid
incomplete
unknown
```

The checker currently recognizes:

- `assume`
- `exact`
- `intro`
- `split`
- `modus`
- `rewrite`
- `simplify`
- `apply`
- `cases`
- `qed`

This is a proof-state engine, not a complete theorem prover. It deliberately labels unfinished or unsupported reasoning as `incomplete`, `invalid`, or `unknown`.

## Terminal Commands

```text
proof list
proof new demo Q
proof assume 1 P
proof step 1 exact P
proof show 1
proof export 1
proof replay 1
```

## Integration

`proofcore_init()` registers the ProofCore step checker as a `logic-proof` MathCore algorithm. Proof steps account work to `proof-worker` and update the compute process workload to `proofcore`.

## Remaining Work

- Real parser for implication/conjunction/proposition terms.
- Assumption contexts and subgoal stacks.
- Strict proof replay that rebuilds state from scratch.
- Tactic expansion for `intro`, `apply`, `rewrite`, `cases`, and induction.
- GUI proof editor panels for goals, assumptions, steps, suggestions, and theorem search.
