# Measurements

Owner: GPT root lane. Checked 2026-10-22 KST; no inferred speedup claim.

## Existing budgets

| Budget | Value | Boundary |
|---|---|---|
| CI memory | 16 GB | ubuntu-latest |
| Codegen/self-host bootstrap | 30/60 min | existing CI workflow |
| Static/focused/integration edit loop | 60 s / 5 min / 30 min | existing agent rules |

## Current canonical frontend observations

The [integration receipt](../evidence/2026-10-22_array_preservation_native_consumer_integration.md)
identifies the source, executable, unchanged canonical entrypoint and logs.
Final source f25b7bb6e: 93.70 s / 3,439,836 KiB on the unchanged canonical
driver input. Pinned baseline3cdb: 77.46 s / 3,440,124 KiB. Both refuse before
MIR and have identical diagnostic streams. Concurrent work ran; this is not
a controlled comparison or a measurement of the new MIR cleanup pass.

The isolated emitted declared-call ownership fixture passes leak detection.
MIR/AIR units and 28 move/loan probes use leaks OFF. This is not a whole-
compiler leak or memory bound. The fixed final-owner census at `57baf8426`
took 1154.78 s and 3,980,968 KiB while refusing before HIR; no successful
canonical MIR cleanup cost or installed-driver memory result is established.

## Historical, not current-main evidence

`b8e66a387`: installed driver203 s/13.7 GB, codegen905 s,
DRV-2 MIR consumer3:45/16.3 GB; see
[original record](../evidence/2026-10-16_b8e66a387_fixed_point.md).
Post-`3cc0ecc47` consumer peak is unestablished here.
Concurrent timings are not a controlled performance comparison.
