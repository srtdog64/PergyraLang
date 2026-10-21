# 2026-10-21: canonical census after StringJoin is related

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`a8f09f5aa` plus the change measured (local, not pushed).

- **Input:** unchanged `src/self_hosted/compiler/driver_bootstrap_main.pgy`.
- **Method:** as in the [landings record](2026-10-20_native_census_landings.md).
- **Change measured:** `StringJoin` and its alias `Join` have a checked
  result relation that depends on neither argument and keeps neither
  (runtime `StringJoin` copies every part into one new String). The
  checker records the result type name the relation's claims read.

| Run | Refusals by analysis |
|---|---|
| `a8f09f5aa` | 3977, 2207, 2034, 2027 |
| StringJoin related | 3936, 2055, 1850, 1832 |
| a scratch relation with more identity conditions, `PGY_VERIFY_FLOW_WORKLIST=1` | 3936, 2055, 1850, 1832 |

No flow store failed in any analysis. Before the
[worklist](../decisions/2026-10-21_flow_passes_evaluate_only_changed_reads.md)
the first analysis failed at the cap
([record](2026-10-21_native_census_string_join_hits_work_cap.md)).

Final analysis outside `direct_mir_*` modules (600 refusals in them both
times):

| Kind | Before | After |
|---|---|---|
| a by-value argument moved and used again | 413 | 268 |
| a borrowed `ref` escapes through a call | 321 | 276 |
| other borrowed `ref` escapes | 79 | 79 |
| a String element crosses an opaque boundary | 174 | 173 |
| an Array<String> ownership receipt cannot be sealed | 37 | 33 |
| every other kind | 403 | 403 |

Without a relation, a frame that passed a formal's storage to
`StringJoin` gave that formal a call escape. Callers then saw the
by-value argument changed, kept or released, so they refused its next
use as a move.
