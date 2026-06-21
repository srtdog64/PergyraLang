# AIR erasure measurement

Empirical dashboard for "how much of each semantic axis (World / Zone / Intent /
Slot / Lifecycle) physically survives into optimized machine code." The honest
claim is **bounded, measured, attributed loss** — not zero loss. Full writeup and
interpretation: [`docs/semantics/14_air_erasure_measurement.md`](../../docs/semantics/14_air_erasure_measurement.md).

## Run

```
pwsh tests/air_erasure/measure.ps1   # joins AIR-declared A/B/C + physical -> results.csv
pwsh tests/air_erasure/gate.ps1      # enforces the erasure + bucket-C + drift contracts
pwsh tests/air_erasure/parity.ps1    # C == LLVM behavioral parity on every fixture
```

Requires the committed `bin/pgy.exe`, mingw `gcc`/`nm` and LLVM `bin` (clang) on
PATH. `A_inh` includes both `retain_cause:inherent` boundaries and the
program-wide `inherent_concurrency_count` (parallel/channel retains).

## What it measures (the join)

Per fixture, two *independent* instruments, joined:

- **AIR declared (`--air-json`):** the compiler's own A/B/C decomposition —
  `A_inh` = boundaries with `retain_cause:inherent` (runtime fact, bucket A),
  `B_pol` = `retain_cause:policy` (kept-by-policy, B), `C_unprov` =
  `unproven_retain_count` (lifecycle CHECK guards the analysis could not erase, C).
- **Physical measured (`gcc -O2` + `nm -u`):** what actually survives —
  `phys_Axis` (out-of-line axis call), `phys_Sync` (pthread), `phys_Abort`
  (fail-closed path).

The join is the point: AIR *claims*, the binary *shows*. `lifecycle_branch`
declares `C_unprov=1` and exactly one `phys_Abort` survives — they match.
`channel_parallel` declares nothing but 11 sync survive — a recorded
`compression_residue_mismatch` drift (AIR under-covers bare `parallel`).

## The gate (`gate.ps1` + `baseline.json`)

1. **Erasure contract (hard):** provable fixtures must hold `phys_Axis=0,
   phys_Abort=0`. Regression = build failure.
2. **Bucket-C monotonicity (hard):** total `C_unprov` must not exceed
   `baseline.json`. C may only shrink. Lower the baseline when analysis improves.
3. **Drift:** a `compression_residue_mismatch` (AIR declares nothing yet residue
   survives) is **hard-failed if new**, or reported if listed in
   `baseline.json` `expected_drifts` (a documented modeling gap).

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
| `08_secure_slot_method.pgy` | Slot+authority | token check survives across a method boundary (B physical=1); expected-drift until slot-capability is modeled in AIR |

## Per-program erasure contract (the falsifiable claim)

> A program using only statically-provable value-Slots emits **zero surviving
> axis calls and zero slot-safety abort paths** in its `-O2` object.

Rows `01`, `02`, `07` satisfy it today. The intended end state is a CI gate that
fails the build if a provable-axis fixture regresses to a surviving axis call.
