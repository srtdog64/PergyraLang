# Measurements

Owner: GPT root lane. Checked 2026-10-18 KST; no inferred performance claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current unchanged canonical observations

V9 source/executable identification is in [gates](gates.md).
YSfo5x: full fixed33.64 s/1730224 KiB; current31.75 s/1728480 KiB.
The fixed program remains4273 routines/3188 keys, not reduced or replaced.
Scratch observert9AMcV:35.84 s/1729804 KiB; first prior-error=0 is in
[blocker](blocker.md). Later prior-error=1 diagnostics are cascades, not an
authoritative count of distinct defects. Refusal precedes HIR allocation;
this is not compiler-scale HIR-census cost or speedup evidence.

## Historical, not current-main evidence

`b8e66a387`: installed driver203 s/13.7 GB, codegen905 s,
DRV-2 MIR consumer3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
Concurrent timings are not a controlled performance comparison.
