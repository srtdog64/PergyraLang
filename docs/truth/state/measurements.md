# Measurements

Owner: Claude lane (GPT root lane's work since 2026-11-19). Checked
2026-11-24. Heavy measurements ran serially in WSL; the tree and driver
hashes are in each linked record.

## Self-host generality (V1 goal 2)

Default-route LLVM sweep of the fixed 1,398 programs against native; method
and clusters in the [baseline record](../evidence/2026-11-22_selfhost_generality_failure_clusters.md).
Remeasure at each landing that touches the default route.

| Revision | Equal | Native refuses | Non-equal | First-error clusters | `direct_mir/` files, lines |
|---|---:|---:|---:|---:|---|
| `cc27d42c6` | 529 | 295 | 574 (0 wrong output) | 118 (11 = 50%, 32 = 80%) | 817, 74,935 |
| S5c slice 1 ([record](../evidence/2026-11-22_selfhost_compiler_clone_carriage.md)) | 535 | 295 | 568 (0 wrong output) | 119 (11 = 50%, 34 = 80%) | 818, 75,096 |
| S5c slice 2 ([record](../evidence/2026-11-23_selfhost_record_clone.md)) | 542 | 295 | 561 (0 wrong output) | 119 (11 = 50%, 33 = 80%) | 819, 75,426 |

Every corpus program (2,746; native refuses 718), default C and LLVM against native LLVM; the table above saw
neither the C route nor programs without a `var` ([record](../evidence/2026-11-24_selfhost_element_write_copies.md); `db571be36`: C 1,417 / 598 / 13; `72c2d2747`: C 1,421 / 598 / 9 / 92).

| Revision | C equal / self refuses / other output / runs a native refusal | LLVM, same columns |
|---|---|---|
| three-routine route deleted `d2df5562a` ([record](../evidence/2026-11-25_three_routine_route_deleted.md)) | 1,418 / 602 / 8 / 85 | 887 / 1,141 / 0 / 25 |
| receiver fields without `self.`, subject `Clone` ([1](../evidence/2026-11-25_receiver_field_binding.md), [2](../evidence/2026-11-25_subject_clone.md)) | 1,418 / 602 / 8 / 85 | 891 / 1,137 / 0 / 25 |
| bare field reads in class methods ([record](../evidence/2026-11-26_value_receiver_field_reads.md)) | 1,418 / 602 / 8 / 85 | 937 / 1,091 / 0 / 25 |

Full DRV-2 MIR: first refusal at transition ordinal 1438 (last measured on
v11, [blocker](blocker.md)).

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
The 1.9 GiB margin came from removing string waste, not from releasing
compiler temporaries ([blocker](blocker.md)). No wall-time claim is made.

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
