# Measurements

Owner: Claude lane (GPT root lane's work since 2026-11-19). Checked
2026-11-20. Heavy measurements ran serially in WSL; the tree and driver
hashes are in each linked record.

## Complete DRV-2 (`driver_rung2_main.pgy`)

The 14 GiB address-space and 1,800 s full-phase limits are unchanged.

| Run | Peak KiB | Result |
|---|---:|---|
| One-process `--emit-c-artifact-verified`, `6ea9e53a8` + change | 12,681,836 | exit 0, C artifact written |
| Producer `--emit-mir-json-verified`, same driver | 5,184,116 | MIR written |
| Consumer `--mir-json` on the fixed `6ea9e53a8` MIR, same driver | 7,313,908 | C bytes equal to the `6ea9e53a8` driver's |

Run record: [one-process within the bound](../evidence/2026-11-20_drv2_one_process_within_bound.md).
On the `6ea9e53a8` driver the producer peaked at 8,352,036 KiB and the
consumer at 10,561,500 KiB. The last one-process run before it exited 134
with an OOM at 14,077,584 KiB ([v5 record](../evidence/2026-11-18_selfhost_array_transform_lifetime.md)).
The margin to the bound is about 1.9 GiB. It came from removing string
waste, not from releasing compiler temporaries, which stays open
([blocker](blocker.md)). No wall-time claim is made.

## Semantic MIR consumers

| Same fixed MIR consumer | Baseline peak KiB | A2 peak KiB | Output |
|---|---:|---:|---|
| Complete semantic program | 1,058,664 | 797,600 | C bytes equal |

| Declaration-identity candidate (2026-11-18) | Peak KiB | Ceiling KiB |
|---|---:|---:|
| Complete semantic MIR consumer | 776,668 | 819,200 |
| Complete MIR-lower source-to-C | 5,249,316 | 8,388,608 |

The same final source through source-to-C and source-to-MIR-to-C has equal
C bytes ([declaration identity record](../evidence/2026-11-18_selfhost_ownership_declaration_identity.md)).
