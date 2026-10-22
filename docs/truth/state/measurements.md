# Measurements

Owner: GPT root lane. Checked 2026-10-23 KST; no inferred speedup claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current observations

The fixed final-owner census at `674fb5903` took 1028.91 s and 3,981,132 KiB
on the unchanged canonical driver input. It refused before HIR with 310
semantic rows. This is a closure census, not a successful compiler runtime or
a controlled performance comparison.

The current native String-copy physical-release gate took 11.49 s and
161,176 KiB. C and LLVM output matched; deleting emitted cleanup made leak
detection fail. This proves that fixture's cleanup path, not whole-compiler
memory safety or the installed-driver bound.

## Historical, not current-main evidence

`b8e66a387`: installed driver 203 s/13.7 GB, codegen 905 s,
DRV-2 MIR consumer 3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
