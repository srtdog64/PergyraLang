# Aggregate release: inferred sink crossings (P1b)

Status: **IMPLEMENTATION CANDIDATE (Claude, sole lane, 2026-10-10)**. Base
`main @ 2b08d86d`. The user asked to make CI green and then do sink inference;
the first CI red is this refusal, so the two are one rung.

## Objective card

- Objective: DRV-2 admits the driver source's aggregate releases without
  explicit `own` annotations, and every newly admitted crossing still proves
  that no live name reads the released storage.
- Priority: sound admission (fail closed) > no new source annotation, copy or
  carrier restoration > reuse of the existing lineage/uniqueness owners >
  patch size.
- Fact owners: the lineage trace
  (`ast_collection_aggregate_value_lineage_owner.pgy`) decides the sink use;
  the new caller-side owner
  (`ast_collection_aggregate_sink_consumption_owner.pgy`) decides consumption;
  the uniqueness owner decides storage sharing between demands.
- Last consumer: `SemanticAstCollectionAggregateReleasePlanFromRequirements`
  and the ordered reservation replay that follows it.
- Forbidden: `own` added to the driver to satisfy the analyzer (27
  "Source `own` is not a substitute for inferred ownership"); speculative
  copies; new manual carrier restorations; accepting a crossing whose caller
  keeps reading the consumed storage.
- Gate: `tests/self_hosted/parity/collection_aggregate_release_source_owner.sh`
  (12 positive + 20 falsifying, C/LLVM) and DRV-2 on
  `driver_bootstrap_main.pgy`.

## Measured failure set

A scratch copy that admitted default-mode formals and logged every refusal
(`.tmp/claude-sh/p1d`, three rounds) gave the complete set on the fixed DRV-2
input. 16 requirements, 4 call sites:

1. Every function-table trace mis-mapped its formal after the first upward
   step. The formal branch traced the caller argument without popping the
   callee frame, so a later call pushed past a stale frame and the next
   formal indexed the wrong call. This would also block an explicit `own`.
2. With the frame fixed, the function-table traces cross default-mode formals
   only: `verified` (x24), `semantic_analysis` (x3) and `analysis`.
3. A constructor argument on the traced path was not recorded as a lineage
   edge, so the alias check refused the sink use itself (`TableHolder(t, 1)`).
4. Demands reaching the function-table constructor through separate
   producer calls (CompileSource..., Canonicalize..., DriverSourceMir...)
   were treated as one storage by the uniqueness owner.
5. The destructure rows (`SelfMirCfgAttachLastDestructure`,
   `SelfMirAppendCfg`) pushed owned String copies into routine and program
   accumulators. Proving that storage exclusive needs reaching definitions
   through `build = F(build)` and `facts = G(facts)` reassignments inside
   loops and branches, which this AST analyzer cannot produce.
6. One function releasing the same projection on two error paths
   (`DriverRung2MirProjectionRelease(projection); Die(...)`) and once on the
   normal path: the uniqueness owner refused any earlier deep drop, even one
   followed by a statement that leaves the routine.
7. The analysis producer returns one function-table local from sixteen return
   statements. The trace recorded the same demand once per return path, and
   the uniqueness owner refused the identical copies as a shared storage.

## Change set

1. Lineage formal branch: default mode (0) is an inferred sink (27 section
   2.3 step 7: the callee stores or forwards the formal toward the release).
   Each call edge through it must pass the consumption owner. `ref` stays a
   readonly boundary. The callee frame is popped while its caller argument is
   traced and restored after; the stack length must equal the invocation
   depth, otherwise the trace refuses.
2. Consumption owner: the argument is a temporary, or a binding that is not
   an inout/ref formal and not a local view of another binding; the call does
   not repeat in a loop that keeps the binding; no read in the call statement
   or later, directly or through a local alias, overlaps the consumed path
   (argument member path plus the released fields). A partial alias written
   into another place refuses.
3. Constructor branch: a traced constructor argument is a lineage edge.
4. Uniqueness: each demand records its producer invocation path; two demands
   whose paths differ at a common depth reach the origin through distinct
   calls and do not share storage. Equal or prefix paths keep every existing
   ordering check.
5. Destructure rows follow the match-fact row contract (`4415adf4`): the
   binding input is a readonly `ref` array, each row stores `Concat("", ...)`
   copies through plain `ArrayPush`, and the routine-build retirement frees
   the backing arrays only. Sharing the semantic spellings instead was
   refused by the element-retention owner (`unproved_formal_element_use_entry`
   at `SelfMirCfgAttachLastDestructure`).
6. Each call row records whether the statement after it in its block leaves
   the routine (return, exit, fail, or a call of a `Never` routine). An
   earlier shared deep drop is ordered only in that case.
7. A demand identical in requirement, release edge, origin, input, caller
   chain and producer invocation path to a recorded one is the same flow
   reached through another return path; it is recorded once.

## Recorded decisions

- Destructure rows (step 5) replace the stopped lane's owned-push exception
  with the match-row contract. Reason: an owned push needs an exclusive-storage
  proof through `build = F(build)` and `facts = G(facts)` accumulators that
  only the MIR ownership pass's reaching definitions can give (27 section 5).
  Cost: the row copies are not released, as for match rows (a few strings per
  destructure statement). Annotation added: `ref bindings_in`, a readonly
  input the routine only reads, as in `SelfMirMatchFactRowsAttachCase`.
  Removal condition: the MIR ownership pass derives these rows' releases.
- Residual gap, equal to explicit `own`: storage captured into another value
  by a call or constructor before the crossing is not tracked by the
  consumption owner. Neither compiler tracks it for `own` today.

## Falsifiers

`aggregate_release_sink_reuse_negative`, `..._alias_negative` and
`..._loop_negative` refuse with `aggregate_release_plan_unproved`;
`..._default_formal_positive`, `..._nested_positive` (frame mapping),
`..._disjoint_field_positive`, `..._distinct_invocation_positive` and
`aggregate_release_error_path_positive` and
`aggregate_release_alternative_returns_positive` admit;
`aggregate_release_error_path_continue_negative` refuses. The 22 earlier
cases keep their verdicts (gate: 12 positive + 20 falsifying, C/LLVM).
