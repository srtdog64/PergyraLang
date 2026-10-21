# Measurements

Owner: GPT root lane. Checked 2026-10-21 KST; no inferred speedup claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current canonical frontend observations

The [native loop receipt](../evidence/2026-10-21_native_mir_loop_local_cleanup.md)
identifies the source, executable, unchanged canonical entrypoint and logs.
Current input: 46.07 s / 3,439,456 KiB. Exact fa66a2fb self-host input archive
on that same executable: 42.95 s / 3,440,252 KiB. Both refuse before MIR and
have identical diagnostic streams. Concurrent work ran; this is not a
controlled comparison or a measurement of the new MIR cleanup pass.

The isolated emitted ownership fixture passes leak detection, not a whole-
compiler leak or memory bound. No canonical MIR cleanup cost, fresh all-
final-owner census cost or installed-driver memory result is established.

## Historical, not current-main evidence

`b8e66a387`: installed driver203 s/13.7 GB, codegen905 s,
DRV-2 MIR consumer3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
Concurrent timings are not a controlled performance comparison.
