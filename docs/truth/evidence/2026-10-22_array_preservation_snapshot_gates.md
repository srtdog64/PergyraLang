# 2026-10-22: gates for the published Array preservation verdict

Recorded by the Claude lane on branch `claude/p0-native-checker`, rebased
onto main `7e7b63673` (GPT's loop cleanup `c9513d5b2` and its handoff).

- **Producer commit:** `a35ecdd60` "Publish each formal's Array
  preservation verdict in the flow snapshot". Files: the fact header, the
  snapshot, the existing transfer owner (its verdict split into a demanded
  query and a settled read the snapshot uses), and one `test_semantic`
  case file. No `src/compiler` or `src/codegen` file.
- **Field:** [decision](../decisions/2026-10-21_array_preservation_published_per_formal.md).

## Gates at `a35ecdd60` (WSL copy `/root/cc-defer2`)

| Gate | Result |
|---|---|
| `test-semantic` | 3235 passed, 0 failed (5 new) |
| `test-transpile` / `test-dir` / `test-air` / `test-rir` / `test-hir` | 1014 / 15 / 161 / 44 / 58 passed, 0 failed |
| `test-mir` units | 691 passed, 0 failed; the target still exits 2 at its self-host driver step, as on main |
| `function-param-flow-summary-test-smoke` | passed |
| `tests/loop_local_collection_cleanup_smoke.sh` | PASS |
| canonical census (`driver_bootstrap_main.pgy`) | 3463, 1184, 1059, 1050 by analysis; 744 outside `direct_mir_*`, no kind changed; wall 910 s (earlier runs 837 to 1062 s) |

## Mutations

| Mutation | Caught by |
|---|---|
| publish the descriptor bit whatever the failures | `test_semantic` (formal used in a `defer`) |
| publish the element bit whatever the failures | `test_semantic` (element write) |
| drop `output_dependencies_ready`, currentness or the complete-state check from the shared settled read | the flow gate's fact-phase unit (`!preserved`, Array pending currency faults) |
| drop `analysis_unsupported` from the settled read | nothing: the flow summary sets both failure bits whenever it sets that flag (`function_param_flow_summary.c`, same assignment), so the condition is implied. It predates this change and stays. |

## Probes

- The handoff's loop falsifier shape (`Count(ref rows)` in a `while` with a
  loop-local `Array<String>`) is in the semantic case and publishes 3.
- An `own` Array formal has no row: a hand-off demands no summary.
- No admitted program produced a descriptor-only failure. A writer through
  a formal's element path (`ArrayPush(rows[0], 1)`, `ArraySet(rows[0], ...)`)
  and a push through a by-value formal are refused by the mutator rules, and
  a by-value struct formal is moved by the call.
