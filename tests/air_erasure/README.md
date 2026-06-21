# AIR erasure measurement

Empirical dashboard for "how much of each semantic axis (World / Zone / Intent /
Slot / Lifecycle) physically survives into optimized machine code." The honest
claim is **bounded, measured, attributed loss** — not zero loss. Full writeup and
interpretation: [`docs/semantics/14_air_erasure_measurement.md`](../../docs/semantics/14_air_erasure_measurement.md).

## Run

```
pwsh tests/air_erasure/measure.ps1
```

Requires the committed `bin/pgy.exe`, mingw `gcc`/`nm`/`size`, and LLVM `bin` on
PATH. Writes `results.csv`.

## What it measures

Per fixture, two views:

- **Emitted (logical):** `call @pgy_<axis>` in pre-link LLVM IR — how many named
  axis runtime ops the codegen produces (naming = traceability, not a leak).
- **Physical (survived):** external symbols that survive `gcc -O2` of the emitted
  C, categorized by `nm -u`: `Axis` (out-of-line axis call), `Sync` (pthread/
  fiber), `Heap` (malloc/free), `Abort` (fail-closed guard path), `IO` (Log).

`Axis (phys) == 0` everywhere = the axis vocabulary is fully compiled out. What
survives is the irreducible primitive the axis stood for (a `pthread_mutex` for a
channel, an `abort` path for a fail-closed guard, nothing for a provable slot).

## Fixtures

| File | Axis | Expectation |
|---|---|---|
| `00_pure_value.pgy` | none | baseline, zero axis residue |
| `01_slot_provable_with.pgy` | Slot | provable → 0 physical (check DCE'd) |
| `02_slot_provable_claim.pgy` | Slot | provable → 0 physical |
| `03_secure_slot.pgy` | Slot+authority | folds when token statically traceable |
| `04_channel_parallel.pgy` | World | irreducible: pthread sync (bucket A) |
| `05_zone_intent.pgy` | Zone+Intent | authority folds; intent-trace residue (B) |
| `06_lifecycle_branch.pgy` | Lifecycle | 1 attributed abort at the ambiguity (C) |
| `07_lifecycle_linear.pgy` | Lifecycle | provable → all zeros (taint) |

## Per-program erasure contract (the falsifiable claim)

> A program using only statically-provable value-Slots emits **zero surviving
> axis calls and zero slot-safety abort paths** in its `-O2` object.

Rows `01`, `02`, `07` satisfy it today. The intended end state is a CI gate that
fails the build if a provable-axis fixture regresses to a surviving axis call.
