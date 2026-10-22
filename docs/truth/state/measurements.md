# Measurements

Owner: GPT root lane. Checked 2026-10-23 KST; no inferred speedup claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current observations

At `f90dd7157`, the exact baseline canonical product run took 141.23 s and
3,493,988 KiB; the exact candidate took 180.18 s and 3,494,172 KiB. An
unrelated compiler process was active in the shared WSL environment, so these
figures are observations only and are not a speed or memory regression claim.
Both runs failed before MIR with the same 33647-error/21-warning summary; only
the first diagnostic and timing line differed.

The reporting-instrumented candidate took 177.10 s and 3,514,512 KiB. It found
none of the eight targeted release rows, then exposed a deeper hard layer. Its
final total is not comparable to the prior fixed 309 census.

The minimal inout-member release/restoration C output passed ASan+UBSan with
leak detection enabled. The output-bearing fixture exposed 7 bytes of existing
temporary-String leakage, so this evidence covers the no-temporary release
path, not general temporary cleanup or whole-compiler memory safety.

## Historical, not current-main evidence

`b8e66a387`: installed driver 203 s/13.7 GB, codegen 905 s,
DRV-2 MIR consumer 3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
