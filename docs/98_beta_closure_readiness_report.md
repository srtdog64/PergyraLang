# Beta Closure Readiness Report

Date: 2026-04-26

This document summarizes the current codebase state, the remaining improvement opportunities, and the concrete work needed to close PergyraLang for beta. It is based on the current README/TODO/status docs, the C/LLVM backend paths, the IR pipeline tests, the ABI smoke matrix, and backend-compare coverage.

## Current Verdict

PergyraLang is no longer blocked by broad surface absence. The remaining beta risk is concentrated in a small number of deep implementation contracts:

- runtime propagation must be generalized beyond the closed slices;
- runtime recoverable failure needs a richer queryable surface;
- declaration-side MIR inventory still carries AST-shaped metadata;
- function/action/intent body safety is not yet fully CFG/dataflow-backed, even though HIR/MIR CFG infrastructure exists;
- type-resolution DAG exists and is now much more visible, but is not yet the
  full semantic execution truth;
- arena/lifetime rules are mostly settled but a few owner/runtime ABI boundaries remain.

Current beta readiness is approximately **50%**.

This is intentionally lower than a feature-count reading. Many core and foundation surfaces are already implemented, tested, and documented, but strict beta readiness depends on the trust of the underlying closure mechanisms. Until function body safety is CFG/dataflow-backed, until type-resolution DAG becomes the source of truth for frozen-subset dependency ordering, and until long-term modularization reaches stable owner boundaries, the project should not be described as 90%+ beta-ready.

The current beta posture is best described as:

> Narrow beta is close, but strict beta still needs CFG-backed body safety plus the remaining propagation, failure, MIR inventory, DAG, and lifetime closure work to be either completed or explicitly downgraded from the beta contract.

2026-04-26 correction: the strict readiness number remains **50%** because CFG
and AIR/body-dataflow source-of-truth are still not closed, but two structural
risks improved materially. The type-resolution DAG fallback cap is now
`materializer_fallbacks<=1296` with exact family accounting, and the production
runtime/codegen/compiler `.inc` size gate is green again with
`src/compiler/mir_public_part_a.inc=959` and
`src/compiler/mir_public_part_b.inc=800`. The first lean debt-slice after the
process change moved C backend type-alias declaration emission out of a near-cap
include body into `src/codegen/transpiler_type_alias.c`, reducing
`src/codegen/transpiler_emitters_base_b_part_c.inc` to 976 LOC without adding a
new `.inc` split. The next debt-slice deleted
`src/codegen/transpiler_emitters_type_require.inc` and moved type requirement
checks into `src/codegen/transpiler_type_require.c`, reducing the source `.inc`
cap to 159 while keeping `src/codegen/transpiler_emitters_base_a_part_a.inc` at
905 LOC. The extern declaration pass now has its own
`src/codegen/transpiler_extern.c` owner, reducing
`src/codegen/transpiler_emitters_base_b_part_b.inc` from 998 LOC to 957 LOC.
Declarator rendering for event-handler/function types now lives in
`src/codegen/transpiler_type_declarator.c`, reducing
`src/codegen/transpiler_helpers_core_b_part_c.inc` from 992 LOC to 849 LOC.
LogBanner indentation normalization now lives in
`src/codegen/transpiler_log_normalize.c`, reducing
`src/codegen/transpiler_expr_emitters_part_a.inc` from 991 LOC to 878 LOC.
Generated-C runtime intent exit cleanup now lives in
`src/runtime/pgy_runtime_intent_exit.h`, preserving the
`pgy_intent_exit_export(...)` inline ABI while reducing
`src/runtime/pgy_runtime_part_ba_part_b.inc` from 996 LOC to 894 LOC.
Generated-C DeviceSlot/SecureSlot macro bodies now live in
`src/runtime/pgy_runtime_slot_macros.h`, preserving built-in instantiation order
while reducing `src/runtime/pgy_runtime_part_ba_part_c.inc` from 996 LOC to
808 LOC. Generated-C intent last-history step accessors now live in
`src/runtime/pgy_runtime_intent_history.h`, preserving borrowed string ABI
accessor names while reducing `src/runtime/pgy_runtime_part_ba_part_a.inc` from
989 LOC to 867 LOC. Generated-C intent last/active borrowed exports now live in
`src/runtime/pgy_runtime_intent_active_exports.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_a.inc` again from 867 LOC to 558 LOC and
making active/recent ABI owner checks explicit. LLVM-linkable runtime core
exports now live in
`src/runtime/pgy_runtime_lib_core_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_a.inc` from 986 LOC to 909 LOC while
keeping exported symbol names unchanged. LLVM-linkable raw `List<T>` exports
now live in `src/runtime/pgy_runtime_lib_list_raw_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_a.inc` further from 909 LOC to 759 LOC
while keeping raw collection ABI smoke green. C backend `let` destructuring lowering
now lives in `src/codegen/transpiler_destructure_emit.h`, reducing
`src/codegen/transpiler_emitters_base_b_part_c.inc` from 976 LOC to 873 LOC and
keeping destructure array/tuple C/LLVM parity green. Generated-C queue inline
runtime now lives in `src/runtime/pgy_runtime_queue_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_e.inc` from 969 LOC to 773 LOC while
keeping queue/channel smoke parity green. Generated-C `HashMap<Int>` key
adapters now live in `src/runtime/pgy_runtime_map_int_key_inline.h`, reducing
`src/runtime/pgy_runtime_part_ba_part_d.inc` from 963 LOC to 815 LOC while
keeping map backend-compare cases green. LLVM-linkable primitive slot exports
for `Slot<Double>`, `Slot<Bool>`, and `Slot<String>` now live in
`src/runtime/pgy_runtime_lib_slot_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_d.inc` from 947 LOC to 790 LOC while
keeping runtime panic ABI/codegen and full ABI smoke green. LLVM-linkable
standard string/conversion/math/random exports now live in
`src/runtime/pgy_runtime_lib_std_exports.h`, reducing
`src/runtime/pgy_runtime_lib_part_b_part_e.inc` from 817 LOC to 761 LOC while
keeping runtime ABI lifetime and ABI pipeline smoke green. MIR declaration
header inventory helpers now live in `src/compiler/mir_decl_headers.h`, reducing
`src/compiler/mir_public_part_a.inc` from 959 LOC to 789 LOC while keeping DAG,
AIR drift, and ABI smoke green. RIR public vocabulary name helpers now live in
`src/compiler/rir_names.h`, reducing `src/compiler/rir_public.inc` from 911 LOC
to 804 LOC while keeping RIR validation/dump consumers on the same vocabulary.
C backend parallel capture analysis now lives in
`src/codegen/transpiler_parallel_capture.h`, reducing
`src/codegen/transpiler_emitters_base_b_part_b.inc` from 957 LOC to 730 LOC
while keeping the parallel channel-sum backend-compare path green.
C backend stdlib call lowering now lives in
`src/codegen/transpiler_expr_stdlib_builtin.h`, reducing
`src/codegen/transpiler_expr_emitters_part_d.inc` from 946 LOC to 26 LOC while
keeping representative stdlib/string/collection backend-compare paths green.
C backend overlay/projection invalidation and zone-layer bind helpers now live
in `src/codegen/transpiler_overlay_projection.h`; the old
`transpiler_helpers_core_a_part_b.inc` include body was removed and the source
`.inc` count is now 158/159. The runtime frontier contract smoke also now reads
the real world frontier owner in `transpiler_domain_role_part_d.inc`.
C backend `let` declaration lowering now lives in
`src/codegen/transpiler_let_emit.h`, reducing
`src/codegen/transpiler_emitters_base_a_part_a.inc` from 905 LOC to 138 LOC
while keeping the C transpile suite and representative let-heavy backend
compare cases green.
C backend MIR block statement emission now lives in
`src/codegen/transpiler_mir_block_emit.h`; the old
`transpiler_emitters_base_a_part_c.inc` include body was removed, dropping
source `.inc` total to 49,911 LOC while keeping MIR/DAG/AIR smoke and
representative MIR-heavy backend compare paths green.
C backend intent declaration emission now lives in
`src/codegen/transpiler_intent_emit.h`; the old
`transpiler_emitters_intent.inc` include body was removed, dropping source
`.inc` total to 48,949 LOC and leaving no production `.inc` above 900 LOC.
Generated-C runtime intent-recent accessors, panic helpers, and checked
arithmetic exports now live in `src/runtime/pgy_runtime_panic_checked_inline.h`,
reducing `src/runtime/pgy_runtime_part_ba_part_b.inc` from 894 LOC to 705 LOC
while keeping panic codegen, panic ABI, runtime lifetime, and full ABI smoke
green.

## Closed Or Mostly Closed

### Stable Surface

The frozen beta subset is now fairly clear:

- generics: exact, ability, multi-bound, and implemented default type argument resolution;
- own/ref: anchored slot-handle and boundary-visible aggregate subset;
- collections: `List<T>`, `Set<T>`, `HashMap<String|Int|Long|Bool, T>`;
- runtime observability: `last / history / active / recent`;
- C/LLVM parity for the currently smoke-covered stable paths.

The main surface trust risk is no longer "what should exist", but whether every parser-accepted surface is either fully closed or explicitly rejected.

### Runtime Propagation Slices

The following propagation slices are now locked by tests:

- world derived-state bounded recompute: `world_fixpoint_abi`;
- relation/effect/zone projection-chain bounded recompute: `projection_chain_abi`;
- zone lifecycle bounded frontier loop: `zone_frontier_abi`;
- v1 handoff materialization projection freshness: `handoff_projection_frontier_abi`;
- active world-owned zone handoff to projection-backed world-state freshness: `handoff_world_state_frontier_abi`;
- action-caused layer/state handoff to active world-derived aliases: `handoff_layer_state_frontier_abi`;
- embedded world-zone projection freshness after direct assignment: `world_embedded_projection_abi`;
- embedded world-zone projection freshness after subject method call: `world_embedded_method_projection_abi`;
- embedded world-zone projection freshness across a simple branch/join: `world_embedded_branch_projection_abi`;
- embedded world-zone action-caused layer/state freshness after subject action call: `world_embedded_action_frontier_abi`;
- embedded world-zone action-caused layer/state freshness after subject action call with fixed-capacity effect pool: `world_embedded_action_pool_frontier_abi`;
- direct C/LLVM stdout parity for branch/join embedded projection visibility: `world_embedded_branch_projection_visibility`.
- direct C/LLVM stdout parity for handoff materialization projection visibility: `handoff_projection_frontier`.
- direct C/LLVM stdout parity for active world-owned handoff state visibility: `handoff_world_state_frontier`.
- direct C/LLVM stdout parity for action-caused handoff layer/state visibility: `handoff_layer_state_frontier`.
- direct C/LLVM stdout parity for embedded world-zone action-caused layer/state visibility: `world_embedded_action_frontier`.
- direct C/LLVM stdout parity for embedded world-zone action-caused fixed-capacity effect pool visibility: `world_embedded_action_pool_frontier`.

This is meaningful progress: the remaining propagation problem is no longer the absence of bounded loops. It is now the generalization of those loops across handoff and broader world-zone propagation families.

### Backend Parity

The current parity baseline is strong for the covered subset:

- `make test-abi` covers C and LLVM ABI smoke cases.
- `make llvm-test-backend-compare` now includes 52 backend-compare cases.
- authority failure bool/string/code drift was closed through `authority_failure_surface`.
- intent authority snapshot propagation was closed through `intent_authority_snapshot_abi` and `intent_authority_snapshot`.
- embedded branch projection visibility is now a direct backend-compare case, not only ABI smoke.

### MIR Path

Routine body emission is largely MIR-driven. Missing ordinary function, method, and intent carriers are hard errors instead of silent partial output. The remaining issue is structural: declaration inventory is still carried as AST-shaped data inside the MIR program instead of a dedicated declaration IR.

## Remaining Beta Blockers

### 0. Function CFG And Body Dataflow Source Of Truth

Status: highest semantic architecture blocker.

Already closed:

- HIR has function CFG v0 with predecessor/reachability, dominator/frontier, loop-depth, local-def, and phi-candidate skeleton facts.
- MIR has routine/block/instruction/cleanup blocks, SSA version maps, def/use summaries, rollback/invalidation exceptional CFG, liveness/DCE slices, and backend vertical slices.
- RIR carries flow-block summaries for resource/projection/world-handoff/invalidation/authority-loss style facts.

Remaining work:

- promote all-path return, reachability, the general branch/join assignment
  lattice beyond the sealed local-`let` surface, move/use-after-move, borrow/ref
  lifetime, drop/cleanup, zone/effect transition, projection freshness, and
  parallel/channel task-boundary checks to CFG/dataflow facts;
- add interprocedural body summaries for return, escape, move, borrow, drop, effect, zone, task, and channel behavior;
- require diagnostics to include path provenance, previous state, `Reason`, and `Fix`;
- make C and LLVM consume the same facts for the frozen subset.

Concrete next work:

- write a CFG/body dataflow inventory test that compares HIR/RIR/MIR facts for representative bodies;
- all-path return is now migrated, direct/terminating-if/exhaustive-match
  unreachable warnings are emitted as `PGY_SEM_UNREACHABLE_CODE`, and stable local `let`
  use-before-init is sealed by syntax plus `PGY_SEM_UNINIT_LOCAL`;
- `QubitSlot` loop move/join now has source-level regression for break-exit
  consumption and continue-backedge consumed-resource detection; migrate richer
  nested/exceptional reachability provenance and the wider branch/join
  assignment lattice next if delayed assignment becomes part of the beta-stable
  surface;
- `defer` cleanup-body terminators are now isolated from the surrounding CFG
  path, and cleanup-body resource moves/releases are snapshot-restored so they
  do not consume the current path's live resources;
- the direct `type_check_statement()` fallback now uses the same defer cleanup
  snapshot helper as CFG body flow; full drop insertion/validation remains a
  beta blocker;
- resource snapshots now include anchored slot state, so terminating-branch
  releases no longer poison reachable fallthrough paths and fallthrough releases
  remain conservatively joined;
- parallel task bodies now use CFG/resource snapshots: task-local terminators do
  not terminate the outer path, resource moves/releases are joined after the
  parallel block, and duplicate cross-task consumption reports
  `PGY_SEM_PARALLEL_SLOT_CONFLICT`. Blocking channel send of a movable resource
  in a parallel task is now covered by the same consume/join regression;
- then migrate remaining ownership borrow/drop, followed by zone/effect and
  channel receive/backpressure/cancellation facts.

Beta closure condition:

- function/action/intent body safety is no longer AST traversal policy; it is a CFG/dataflow contract with semantic diagnostics and backend parity coverage.

### 1. Handoff And Broader World-Zone Propagation

Status: highest runtime blocker.

Already closed:

- bounded loops exist for world derived state, zone lifecycle, and projection chains;
- embedded projection freshness works for direct assignment, method call, and a simple branch/join;
- v1 handoff materialization keeps source and target projection freshness aligned on C and LLVM.
- active world-owned zone handoff keeps projection-backed world state and composed `all` state fresh on C and LLVM.
- embedded world-owned zone subject action calls now propagate action-caused effect layer/state freshness to active world-derived aliases on C and LLVM for single effect slots and fixed-capacity effect pools.
- action-caused effect handoff keeps target zone layer/state and active world-derived layer/state aliases fresh on C and LLVM.

Remaining work:

- define handoff propagation as a runtime contract, not just a semantic trace;
- ensure C and LLVM use the same recompute order, pass limit, hard-fail boundary, and provenance stamp vocabulary;
- document whether handoff is materialization, identity move, snapshot, or another explicit beta rule.

Concrete next tests:

- `handoff_authority_rejection_abi`;
- broader real authority rejection path beyond intent step-local `authorized by` validation.

Beta closure condition:

- handoff mutation/query results are identical on C and LLVM;
- stale projection or stale world-state reads after handoff are covered;
- pass-limit overflow remains hard-fail;
- docs describe the beta handoff rule without implying a richer v1 ownership model than exists.

### 2. Recoverable Runtime Failure Surface

Status: baseline exists, richer query surface remains.

Runtime panic contract progress:

- `src/runtime/pgy_runtime_panic_contract.h` now provides the shared hard-fail panic vocabulary.
- Exported typed slot and secure-slot runtime paths no longer silently fallback for released-slot or invalid-token hard failures.
- Inline typed slot and secure-slot runtime paths use the same released-slot, invalid-secure-token, and double-release classes.
- `runtime-panic-contract-test-smoke` gates the implementation contract, and `runtime-panic-abi-test-smoke` provides executable evidence for released-slot, invalid-token, and double-release aborts.

Already closed:

- generated C and LLVM runtime-lib share authority validation vocabulary;
- non-aborting authority validation exposes `last_ok`, `last_zone`, `last_participant`, and `last_reason`;
- `authority_failure_abi`, `authority_failure_surface`, `intent_authority_snapshot_abi`, and `intent_authority_snapshot` are green.
- semantic `authorized by` validation now resolves participants to concrete zone subject slots, so same-type non-authority slots and ambiguous same-type authority mappings are rejected before lowering.

Remaining work:

- decide the minimum stable query surface for zone/world/authority boundary failures;
- expose the same reason/state model for the major recoverable paths, not just authority validation;
- keep invariant breaks as hard-fail and do not blur them into recoverable errors.

Concrete next tests:

- authority missing-zone query;
- authority missing-participant query;
- boundary mismatch query after world handoff;
- intent step failure reason tied to runtime state.

Beta closure condition:

- recoverable failures return `Bool`, `Result<T>`, or queryable runtime state consistently;
- hard-fail cases remain hard-fail;
- docs, diagnostics, and runtime accessors use the same reason vocabulary.

### 3. Declaration-Side MIR Inventory Debt

Status: functional but structurally incomplete.

Already closed:

- backend entrypoints receive MIR bundle data;
- routine body paths use MIR routines and fail hard on missing carriers;
- intent step check/eval/meta carrier absence is no longer silently tolerated.

Remaining work:

- split declaration inventory into a dedicated IR shape instead of AST-carried inventory;
- remove raw host-name state and duplicated named-decl lookup from helper paths where possible;
- keep declaration emission errors structured when inventory is incomplete.

Concrete next work:

- introduce a small `MIRDeclInventory`/`MIRDeclHeader` view that contains only the metadata backends need;
- migrate zone/world/relation/effect declaration emitters to that view first;
- add regression that a missing declaration inventory item fails with a backend error, not partial output.

Beta closure condition:

- remaining AST references are documented as declaration metadata carriers or removed;
- backend users can tell which declaration metadata is required;
- LLVM and C consume the same declaration inventory truth for the frozen subset.

### 4. Type-Resolution DAG Source Of Truth

Status: graph infrastructure exists; source-of-truth authority is not complete. This is now one of the two main structural blockers alongside CFG-backed body safety.

Already closed:

- graph inventory, cycle diagnostics, and topo derivation exist;
- provider-first staged worklist is active for top-level declarations and synthetic local/projection nodes;
- generic default type, constraints, where-bound, and several ability consumers run through staged DAG paths;
- cycle diagnostics use `Contract source`, `Reason`, and `Fix` vocabulary.
- non-generic nominal class type references and known non-class scope symbols
  now materialize through DAG metadata, cutting central materializer fallbacks
  from `4135` to `1296` while preserving generic default/provenance paths.

Remaining work:

- move alias materialization, effective generic/default/bound facts, and
  module/nominal provenance from recursive lookup to graph-backed execution;
- make the graph the default source for provider/consumer ordering in the semantic paths that already have inventory edges;
- keep module import DFS and type-resolution DAG responsibilities separate.

Concrete next work:

- audit remaining recursive `resolve_type_node` consumers and classify them as `graph-backed`, `namespace-only`, or `legacy`;
- focus the next migration on alias-heavy named fallback
  (`alias=1281`, `builtin_shell=2`, `missing=6`, `generic_named=7`) instead of
  broad builtin-shell expansion;
- do not shortcut alias metadata through the current symbol cache alone:
  local testing showed that this bypasses module visibility and generic ability
  provenance checks. Alias closure needs graph facts for export/private
  provenance and effective generic bounds before it can replace the central
  resolver fallback safely;
- add tests where declaration order would fail without provider-first topo scheduling;
- promote local contract and projection path handlers from "covered node family" to "semantic source of truth" where possible.

Beta closure condition:

- no known frozen-subset type dependency relies only on declaration order;
- graph-backed errors include stable provenance;
- the docs stop calling for a full DAG rewrite and instead name the exact remaining migration paths.

### 4b. Long-Term Modularization Boundary

Status: necessary structural work is underway, but the stop condition is not met.

Already closed:

- several semantic leaf/helper families now live in real translation units;
- module contract include-order debt was removed;
- type-resolution graph primitive, collector, label, domain, and declaration helper seams have started moving out of `.inc`;
- speed baseline and `perf-summary` are available to catch modularization regressions.
- production runtime/codegen/compiler include files are below the 1,000 LOC
  gate; the current MIR public split is `part_a=959` and `part_b=800`, guarded
  by `make backend-inc-size-test-smoke` and `make inc-sentinel-test-smoke`.
- tooling conformance is green locally; `tests/tooling_conformance_smoke.sh`
  invokes formatter smoke through `bash`, avoiding Linux execute-bit drift on
  mounted worktrees.

Remaining work:

- reduce semantic `.inc` files above 800 LOC;
- continue converting behavior-heavy codegen/runtime/compiler `.inc` families
  into real owner translation units instead of adding new split fragments;
- extract the files currently closest to the cap (`964`, `962`, `959`, `957`,
  `947`, `946`, `925`, `911`, `909` LOC) into owner `.c`/`.h` seams before adding new
  behavior to those families;
- make `type_checker.c` orchestration-only rather than an include aggregator;
- split backend/runtime owners so future core features do not require editing multi-thousand-line include fragments.

Beta closure condition:

- DAG/stage/declaration/backend/runtime owner boundaries are explicit enough that a frozen-subset feature can be changed without relying on include-order side effects;
- remaining `.inc` usage is limited to generated tables, local macro tables, or private test fixtures;
- any remaining representation debt is documented as internal and non-user-visible.

### 5. Arena And Lifetime Boundaries

Status: direction is correct; remaining boundaries are narrow.

Already closed:

- arena/index discipline is documented;
- semantic scratch arena is used across several diagnostic and coverage paths;
- LLVM has clearer scratch, persistent, and result-owned lanes;
- memory tests cover hard-fail unwrap and pointer lifetime guards.

Remaining work:

- finish owner shell boundaries such as context/registry/result outer shell ownership;
- clarify runtime ABI ownership for returned strings and helper-produced payloads;
- prevent caches from retaining arena-owned transient pointers.

Concrete next work:

- audit helper names that return `char *`, `const char *`, or grow-array payloads;
- classify each as borrowed, scratch-owned, result-owned, persistent-owned, or runtime-owned;
- add one focused regression for a returned diagnostic/runtime string that survives scratch teardown.

Beta closure condition:

- helper ownership is mechanically reviewable;
- runtime ABI return ownership is documented and tested;
- no frozen-subset diagnostic or runtime query relies on a scratch pointer after the producing phase ends.

## Secondary Improvement Opportunities

### Contract Clause Density

The language is semantically stronger than it is comfortable to write. `requires / within / authorized by / causes / refresh / publish / bind` still creates repetition across actions, intent steps, and zones.

Recommended beta approach:

- do not add new keywords before beta;
- keep the explicit form as the canonical source of truth;
- keep compressed examples only when they are already semantically equivalent and smoke-covered;
- expand `docs/69_authoring_pair_examples.md` with examples that are actually tested.

### Projection Diagnostics

Projection diagnostics have improved, but beta should keep pushing toward a single diagnostic shape:

- target;
- source;
- projection kind;
- field path or map;
- `Reason:`;
- `Fix:`.

Recommended next work:

- add one semantic regression each for missing source field, ambiguous path, wrong projection kind, and duplicate field map;
- make those cases appear in the beta support board as completed only when the wording is stable.

### Documentation Hygiene

The docs are useful but large, and some older docs still describe broader ambitions beside current beta facts.

Recommended next work:

- keep `README.md`, `TODO.md`, `docs/17_development_status.md`, `docs/18_language_status.md`, and `docs/70_beta_closure_master_board.md` as the live truth set;
- avoid broad edits to old roadmap docs unless they contradict beta support;
- add a beta release checklist that points to exact make targets and exact smoke cases.

## Recommended Execution Order

Operating change for the next sprint:

- Use a lean debt-slice loop rather than a test-first loop. Pick one owner,
  finish the implementation slice, run the local gate, then batch the wide
  regression.
- This is not a reduction in rigor. Full regression remains required before
  closure, but running it after every small edit has been slowing actual debt
  removal.
- The next sprint should not be "add more tests"; it should be "remove one
  source-of-truth duplication or fallback seam, then prove it with the smallest
  meaningful gate."

1. Promote DAG staged resolution.
   Audit remaining recursive type-resolution consumers and move frozen-subset dependency ordering behind graph-backed paths. This is now the highest-value beta blocker because it defines whether the language can keep module/generic/authority contracts stable as the surface grows.

2. Continue long-term modularization around the DAG boundary.
   Split graph inventory/stage/declaration helpers until semantic ownership is explicit enough to avoid include-order side effects. Prioritize `.inc` seams that directly affect type resolution, module contracts, and backend declaration inventory.

3. Close the remaining handoff propagation tails.
   The active projection-backed world-state and action-caused layer/state slices are now covered; next add handoff authority/failure cases and implement any missing queryable rejection path.

4. Complete recoverable failure state.
   Expand queryable runtime failure beyond the current authority baseline while preserving hard-fail boundaries.

5. Shrink declaration inventory debt.
   Build a dedicated declaration metadata view for frozen domain declarations and migrate C/LLVM consumers.

6. Finish arena/lifetime ownership review.
   Classify returned helper ownership and add a small regression that proves result-owned data outlives scratch formatting.

7. Freeze docs and examples.
   Update the live truth set, stable examples, and release checklist only after the code paths are covered.

## Practical Beta Exit Criteria

PergyraLang can be called beta when the following are simultaneously true:

- `make test-all` is green;
- `make test-abi` is green for C and LLVM smoke;
- `make llvm-test-backend-compare` is green with the frozen subset cases;
- Linux support is C+LLVM, Windows support is C plus LLVM when the toolchain is detected;
- runtime propagation has handoff/general world-zone coverage, not only projection-chain and embedded slices;
- recoverable runtime failures expose queryable state for the stable failure surface;
- declaration-side MIR debt is either closed for the frozen subset or explicitly documented as non-user-visible representation debt;
- function/action/intent body safety is CFG/dataflow-backed for reachability, all-path return, init, move/borrow, cleanup, effect/zone, and parallel/channel facts;
- type-resolution DAG is the source of truth for frozen-subset dependency ordering;
- arena/runtime ABI ownership rules are reviewable and tested;
- stable subset, explicit reject, and beta-out-of-scope wording match across README, TODO, status docs, diagnostics, and smoke examples.

## Current Risk Summary

The project is close enough that broad new design should stop. The remaining value is in making the existing language truthful:

- every accepted beta surface should either run through semantic/runtime/C/LLVM/tests/docs or be rejected explicitly;
- every runtime state transition should be observable enough to debug;
- every parity case should fail loudly when C and LLVM drift;
- every structural debt item should be described as either user-visible beta risk or internal representation debt.

The next most valuable implementation target is the remaining handoff propagation tail: authority/failure visibility after transfer. Projection, active world-state, and action-caused layer/state handoff slices are now covered on both backends.
