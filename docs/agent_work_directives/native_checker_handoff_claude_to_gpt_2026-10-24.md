# Native checker handoff, Claude lane to GPT root lane (2026-10-24)

Status: `HANDOFF`. Base revision: `2d1b9a7fa` on branch
`claude/p0-native-checker` (local, not pushed), which merges main at
`06a7980be`. Decided by the user on 2026-10-24: everything goes into main,
and the GPT root lane does all further work; two lanes on one finish line
caused problems.

## Objective card (unchanged)

- Objective: the V1 finish line (`AGENTS.md`, V1 Scope; the
  [scope directive](v1_scope_ownership_and_selfhost_generality_2026-10-14.md)):
  zero ownership refusals on the unchanged canonical driver input,
  bootstrap, gen2 == gen3, installed driver and required CI on one
  published commit. SoT rows close only where these steps pass through
  them; the row count is not a goal.
- Owner from now on: the GPT root lane, for main integration and every
  ownership producer. The Claude lane edits nothing further.
- Forbidden: as in `docs/truth/state/blocker.md` (no broad `ref`, no
  whole-analysis `Clone`, no per-program `direct_mir_*` route, no weakened
  refusal).

## 1. Integrate the branch into main

`claude/p0-native-checker` at `2d1b9a7fa` contains main `06a7980be` plus
seven branch commits, so main can fast-forward to it. The working tree
of `D:\PergyraLang` has uncommitted GPT work in files the branch also
changes (`src/semantic/formal_string_store_landing_owner.c` and `.h`,
`src/semantic/function_param_flow_backing_dependencies.c`,
`tests/ir/function_param_flow_summary_smoke.sh`), so commit or set that work
aside first, then `git merge --ff-only claude/p0-native-checker` and
replay it.

Gates run on the merge commit `2d1b9a7fa` (fresh WSL tree, Linux):

| Gate | Result |
|---|---|
| `tests/ir/function_param_flow_summary_smoke.sh` | pass (256 s) |
| `tests/ownership/aggregate_storage_transfer_smoke.sh` | pass |
| `backend_compare/map_clone_deep`, C against LLVM | same output |
| `operator_overload_rules_unit`, `default_aggregate_copy_graph_unit`, `flow_rows_check_memo_unit`, `flow_closure_worklist_unit` | pass |
| `readonly_map_query_fact_phase_unit` | 1 failure of 13431, the same on main `06a7980be` (the GPT working tree edits this test) |
| `tests/repo/semantic_tu_size_smoke.sh` | fails as on main; the merge takes `default_aggregate_move_owner.c` from 600 to 613 counted lines and `formal_string_store_landing_owner.c` from 767 to 775 |

Not run: the canonical census on the merge, bootstrap, installed driver,
gen2 == gen3, CI.

The seven commits, oldest first:

| Commit | What |
|---|---|
| `b13178024` | constructor snapshot checks only types; member qualifiers in declared-result frames (main has its own copy, `66668a207`) |
| `1a75ca2ee` | census record after the main merge (118, 77 outside `direct_mir_*`) |
| `f22d406d9` | rows check reads only established declared results (main: `507732d49`) |
| `4fa9d8d65` | String receipts census (116, 75) |
| `df32ffb0e` | bounded landing planner experiment directive |
| `f6de1d1b7` | a refused loan copies its whole chain in one analysis: 4 landing analyses instead of 10, same verdict |
| `707cbfc24` | a refused loan copies its lent argument when its result cannot be copied: 116 to 91 (75 to 51 outside `direct_mir_*`) |

The merge keeps main's argument-copy batch (`refuse_use` reports a planned
argument copy and the walk removes the move) for moves, and reports a
copied lent argument the same way.

### Red-team review before the handoff

Four read-only reviewers (soundness, owner duplication, gates, hidden
control flow) went over the branch's code against main. The commit "Fix what a
red-team review of the loan landing found" fixes what they confirmed:

- The lent-proof verdict was cached per call, so a call lending two owners
  answered the second owner with the first one's verdict; it is now per
  call and owner ([decision](../truth/decisions/2026-10-24_lent_proof_verdict_is_per_owner.md)).
  Main has the same defect.
- The driver returned an analysis whose landing selections could not be
  merged as if it were a verdict; it now fails closed.
- A dropped chain still copied its first place; the proof now publishes one
  verdict (`FunctionParamFlowLoanStop`) both consumers read, shown as
  `[semantic loan proof]` under `PGY_DEBUG_PIPELINE_STAGE`.
- The store-landed check rescanned the routine's claims; it uses the call
  the walk reports, and the value-ordinal rule lives in the store owner.
- Gates: three new fixtures, placement checks for multiple copies and for
  the getter copy, `landing_rounds` 0 for the move negatives, outputs that
  show sharing, and negatives whose blocking element holds a String.

Not changed, for the GPT lane: the walk itself selects inout-output store
landings (`semantic_formal_store_landing_select_held` in
`function_param_flow_backing_dependencies.c`), also from query walks, and
nothing withdraws such a selection when a getter copy is chosen later.

## 2. Measurements are in two tools

The Claude census (every error downgraded, every final owner and the String
receipts pass run, `PGY_CENSUS_*` instrumentation applied to a scratch
copy) reads 91 after `707cbfc24`. The GPT fixed final-owner census in
`docs/truth/state/blocker.md` reads 309 on `4716f9539`. They differ in
tool, mode and tree; agree on one before counting progress. A fast way
to reproduce one routine's refusals is to compile only its module with the
instrumented compiler (about a minute instead of 25 to 45 for the
canonical input).

## 3. Work in progress, not for main as it is

Branch `claude/wip-plain-struct-array-clone` at `9f1d44cc2` (on
`707cbfc24`): Clone of an Array of plain structs (structs of physical
scalars) is a whole copy. C already copied such an Array element by
element; LLVM now calls a new `pgy_array_clone_raw_export`; the checker
counts the Clone as independent. The canonical census goes from 91 to 74,
and `backend_compare/struct_array_clone_deep` prints the same on C and LLVM.

It is not ready: 26 flow-summary negatives lose their refusal, because they
use `Array<Cell>` with `Cell { value: Int }` precisely so that no copy can
stand (`loan_owner_*`, `loan_read_*`, `loan_payload_*`, `loan_kept_*`,
`local_copy_other_side_*` and others; the list is in the WIP commit's
smoke run). Each must keep its refusal with an element a Clone copies only
by value (the five default-aggregate-move negatives on that branch use a
`Pair` with a String field), or be shown to be a sound landed copy and
become a positive.

## 4. What is left outside `direct_mir_*` after `707cbfc24` (51)

The grouped list is in the
[census record](../truth/evidence/2026-10-24_native_census_lent_argument_copy.md).
Two items found but not changed:

- `CanonicalMirIdentityEpochArtifactFromTreeText` (2): a landed Clone
  wraps an `inout` argument, which is refused. `AppendLine(acc, line)`
  returns `line` itself when `acc` is empty, so `output` shares an element
  of `lines` when passed `inout`; the copy belongs where `output` is
  assigned, which a node-wrapping landing cannot place today.
- `admitted` (8) reaches `direct_mir_*` routes and goes with them.

## 5. User decisions

- Answered 2026-10-25: a real sharing site in the canonical source may
  take an explicit `Clone`
  ([decision](../truth/decisions/2026-10-25_real_sharing_site_may_take_explicit_clone.md)),
  and the direction is re-evaluated on 2026-11-08
  ([decision](../truth/decisions/2026-10-25_ownership_direction_reevaluated_2026-11-08.md)).
- Open: pushing. Nothing of this branch has been published.
