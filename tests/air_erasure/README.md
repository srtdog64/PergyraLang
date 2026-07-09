# AIR erasure measurement

Empirical dashboard for "how much of each semantic axis (World / Zone / Intent /
Slot / Lifecycle) physically survives into optimized machine code." The honest
claim is **bounded, measured, attributed loss**, not zero loss. Full writeup and
interpretation: [`docs/semantics/14_air_erasure_measurement.md`](../../docs/semantics/14_air_erasure_measurement.md).

## Run

```powershell
pwsh tests/air_erasure/measure.ps1   # joins AIR-declared A/B/C + physical -> results.csv
pwsh tests/air_erasure/gate.ps1      # enforces erasure, retain attribution, and drift contracts
pwsh tests/air_erasure/parity.ps1    # C == LLVM behavioral parity on every fixture
```

Requires the committed `bin/pgy.exe`, MinGW `gcc`/`nm`, and LLVM `bin` (clang)
on `PATH`. `A_inh` includes both `retain_cause:inherent` boundaries and the
program-wide `inherent_concurrency_count` (parallel/channel retains). `B_pol`
includes both `retain_cause:policy` boundaries and the program-wide
`slot_capability_retain_count` (SecureSlot/DeviceSlot capability retains).

## What It Measures

Per fixture, two independent instruments are joined:

- **AIR declared (`--air-json`):** the compiler's own A/B/C decomposition:
  `A_inh` = boundaries with `retain_cause:inherent`, `B_pol` =
  `retain_cause:policy` plus `slot_capability_retain_count`, and `C_unprov` =
  `unproven_retain_count`.
- **Physical measured (`gcc -O2` + `nm -u`):** what actually survives:
  `phys_Axis` (out-of-line axis call), `phys_Sync` (pthread), and
  `phys_Abort` (fail-closed path).

The join is the point: AIR claims, the binary shows. Physical residue is allowed
only when it is either part of the pinned substrate floor or listed in
`baseline.json` `retained_runtime_attribution`.

## The Gate

`gate.ps1` consumes `results.csv` and `baseline.json`:

1. **Substrate-floor pin (hard):** every emitted program carries a runtime
   substrate independent of axis use. Since the R6 wall-time watchdog, that
   floor is `abort + pthread_create + pthread_detach` (`Sync=2`, `Abort=1`).
   The control fixture `00_pure_value` must match `baseline.substrate_floor`
   exactly, by count and by named symbol list.
2. **Erasure contract (hard):** provable fixtures must hold `phys_Axis=0` and
   carry nothing beyond the substrate floor.
3. **Bucket-C monotonicity (hard):** total `C_unprov` must not exceed
   `baseline.total_unproven_retain`. C may only shrink.
4. **Declared-vs-measured drift (hard unless expected):** if AIR declares
   `A+B+C == 0` but residue survives beyond the floor, the fixture must be in
   `baseline.expected_drifts`.
5. **Retained-runtime attribution (hard):** any fixture with nonzero A/B/C
   declaration or physical residue beyond the floor must have an exact row in
   `baseline.retained_runtime_attribution`, including A/B/C counts,
   floor-excess physical counts, and a human reason.

Promoted to CI 2026-07-04 (WO-A2): `make air-erasure-gate` re-measures and
gates, wired into ci-windows' runnable block. RED was verified both ways: an
injected axis symbol on a provable fixture and an injected floor symbol both
fail the gate. Baseline updates are explicit commits only.

`Axis (phys) == 0` everywhere means the axis vocabulary is fully compiled out.
What survives is the irreducible primitive the axis stood for, such as pthread
sync for channel/concurrency coordination or a fail-closed path for a guard.

## Fixtures

| File | Axis | Expectation |
|---|---|---|
| `00_pure_value.pgy` | none | baseline, zero axis residue |
| `01_slot_provable_with.pgy` | Slot | provable, no residue beyond floor |
| `02_slot_provable_claim.pgy` | Slot | provable, no residue beyond floor |
| `03_secure_slot.pgy` | Slot+authority | folds when token provenance is statically traceable |
| `04_channel_parallel.pgy` | World | retained pthread sync, bucket A |
| `05_zone_intent.pgy` | Zone+Intent | retained runtime-visible coordination, bucket A |
| `06_lifecycle_branch.pgy` | Lifecycle | unproven lifecycle guard, bucket C |
| `07_lifecycle_linear.pgy` | Lifecycle | provable, no residue beyond floor |
| `08_secure_slot_method.pgy` | Slot+authority | SecureSlot capability policy retain, bucket B |

## Per-Program Erasure Contract

> A program using only statically-provable value-Slots emits zero surviving axis
> calls and zero slot-safety residue beyond the substrate floor in its `-O2`
> object.

Rows `01`, `02`, and `07` satisfy it today. The watchdog is not slot-safety
machinery; it is the R6 DoS bound every program carries. `make air-erasure-gate`
fails the build if a provable-axis fixture regresses to a surviving axis call.

## 2026-07-04 Refresh

Between the 2026-06 snapshot and this refresh, every fixture gained
`phys_Sync +2, phys_Abort +1`: the R6 wall-time watchdog inlined
`pthread_create`/`pthread_detach` plus a fail-close `abort` into every emitted
program. That is designed bucket-B substrate, not an erasure regression:
`phys_Axis` stayed 0 everywhere.

Retained-runtime fixtures are pinned in `retained_runtime_attribution`. Current
snapshot values include `01_slot_provable_with` `A_inh=1` with zero physical
excess; `04_channel_parallel` `A_inh=6`, `phys_Sync_excess=13`;
`05_zone_intent` `A_inh=2`, `phys_Sync_excess=2`; `06_lifecycle_branch`
`C_unprov=1`; and `08_secure_slot_method` `B_pol=2`.
