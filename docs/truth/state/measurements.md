# Measurements

Owner: GPT root lane. Checked 2026-10-18 KST; no inferred performance claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current unchanged canonical observations

Final V10 source/executable identification and raw receipts are in the
[root replay](../evidence/2026-10-18_v10_root_replay.md).
Full fixed: 33.67 s / 1,730,056 KiB; full current: 33.92 s / 1,728,048 KiB.
The fixed program remains4273 routines/3188 keys, not reduced or replaced.
The first refusal precedes HIR allocation; later diagnostics are cascades,
not an authoritative count of distinct defects. These timings are not a
compiler-scale HIR-census cost or speedup measurement.

## Historical, not current-main evidence

`b8e66a387`: installed driver203 s/13.7 GB, codegen905 s,
DRV-2 MIR consumer3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
Concurrent timings are not a controlled performance comparison.
