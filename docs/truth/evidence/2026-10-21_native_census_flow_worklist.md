# 2026-10-21: canonical census with the flow worklist

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`004bca9e7` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md),
  on WSL with other runs sharing the machine, so wall times are not compared.
- **Change measured:** a pass after the first evaluates only the entries
  whose routine read an entry that changed after the read
  ([decision](../decisions/2026-10-21_flow_passes_evaluate_only_changed_reads.md)).

| Run | Refusals by analysis | Final-analysis evaluations |
|---|---|---|
| `a21fe4a5e` (full passes) | 3977, 2207, 2034, 2027 | 18352 for 14508 entries |
| worklist | 3977, 2207, 2034, 2027 | 15293 for 14508 entries, 4685 skipped |
| worklist, `PGY_VERIFY_FLOW_WORKLIST=1` | 3977, 2207, 2034, 2027 | the same; no flow store failed |

## The closure that hit the cap

A scratch trace of the first analysis with `StringJoin` related and the
cap lifted, under full passes. Root `EmitStmtList`, 496 entries over 216
routines:

| Pass | Entries evaluated | Entries changed | Units |
|---|---|---|---|
| 1 | 491 | 491 | 2198 (1707 of them no-store graph visits) |
| 2 | 496 | 51 | 496 |
| 3 | 496 | 44 | 496 |
| 4 | 496 | 17 | 496 |
| 5 | 496 | 0 | 496 |

That is 4182 units against the cap of 4096. Without `StringJoin` related
the same closure took four passes and 3653 units. The
[StringJoin record](2026-10-21_native_census_string_join_hits_work_cap.md)
gave 3493; this trace counts every unit the closure charged.

With the worklist and `StringJoin` related, the census completes every
analysis without a flow store failure (see the next relation's record).
