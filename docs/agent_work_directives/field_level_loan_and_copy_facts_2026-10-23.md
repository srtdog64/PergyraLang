# Field-level loan and local-copy facts

Status: slice 1 IMPLEMENTATION COMPLETE, slice 2 PLAN; Claude lane. Base revision `824cf8bdd` on branch
`claude/p0-native-checker` (local, not pushed). Outputs are implementation
candidates; each slice lands only with its census, fixtures and mutations.

## Objective card

- **Objective.** Admit the canonical refusals whose storage proof fails only
  because a fact is kept per binding where the program changes and reads
  disjoint rows of one value. Weaken no rule: each slice must keep every
  negative and add one for the row overlap it now tells apart.
- **Priority order.** One owner per fact, no fallback, a behavioral
  negative per slice, then patch size.
- **Fact owners.** The move walk's copy rows already carry field paths
  (`default_aggregate_copy_owner.c`, `carrier_fields`); the storage walk
  takes a focus field for `inout` callees
  (`function_param_field_preservation_owner.c`). The loan frame keeps one
  origin set per binding (`function_param_flow_backing_*`).
- **Last consumers.** The move walk's other-side use (`refuse_use` in
  `default_aggregate_move_owner.c`) and the loan frame's call escape for a
  formal whose storage proof fails (`function_param_flow_backing_dependencies.c`).
- **Forbidden.** A copy of a whole fact graph where a proof would do
  ([measured](../truth/evidence/2026-10-22_deep_clone_counterfactual.md));
  per-routine exemptions; a field fact read without the owner's verdict.

## Refusals in scope (census at `824cf8bdd`, 215 in all)

- **Slice 1, callee read paths: 13 local copies.** `SemanticAstAnalysisResolve*`
  routines copy `analysis.expression_surfaces.expression_graph`, set rows of
  the copy's `arena.identities` (or return types, places, role targets), and
  pass `analysis` or its surfaces to callees that read other rows, as
  `SemanticAstExpressionFunctionNodeAt` reads `function_node_ids`. The walk
  counts each such call as a read of the whole value.
- **Slice 2, per-field loan origins: 12 moves.**
  `SemanticExpressionGraphCallTargetCaptureFromSignatures` sets its graph's
  `call_target_kinds` and `call_target_names` in place. A loan of the
  artifact reaches the graph only in its topology and node rows, but the
  loan frame sees one origin set for the whole surfaces value and refuses
  the call as a change of the artifact (`CompileSourceDirTextVerified` 3,
  `CompileSourceCapabilityManifestVerified` 2, the analyzer's own 6, one in
  `SemanticAstArtifactAnalyzeWithExpressionGraph`).

Not in scope: 14 `function_tables` maps and 8 `cfg` rows grown in place
(real changes; a copy needs the deep `Clone` GPT builds), 9 `valid` (a real
alias), 2 `Compile` (a borrowed String released on overwrite of
`self.outcome`; String landing lane), and the 9 `SemanticAstCollectionExclusiveValueFactsFrom`
map copies (a rehash would leave a copy reading freed buckets).

## Slice 1 plan

Landed for an other side that is an `inout` formal only: under a by-value
or `ref` formal or a local the copy refusal lands a copy that keeps the
change the routine's own, and narrowing it there moved 22 uses of `facts`
in `SelfMirCanonicalInstructionIds` (census 215 -> 226). Kept version:
215 -> 209 ([evidence](../truth/evidence/2026-10-23_native_census_callee_read_paths.md)).


1. Owner: a per-formal summary of the access paths a routine reads of that
   formal, computed with the existing summary demand (one fixed point,
   recursion through callees' summaries; any path the walk cannot name reads
   the whole formal).
2. Consumer: the move walk's other-side use, for a whole value passed to a
   by-value or `ref` formal, asks that summary; a read path disjoint from the
   changed field path is no use.
3. Negatives: a callee that reads the changed row; one that passes the value
   on to a callee that does; one the walk cannot name (whole read).
4. Gate: the flow gate, the 776-fixture compare of errors, landings and
   `Clone` counts, the canonical census, mutations of each condition.

## Slice 2 plan

Findings at `d35052591`:

- `SemanticExpressionGraphFactsFromAstRows` builds the graph's call-target
  rows fresh (`[]` and pushes of constants) and `Clone`s the topology's
  rows; the artifact's storage reaches the graph, if at all, through the
  String elements of `texts` and of the surface rows.
- `SemanticExpressionGraphCallTargetCaptureFromSignatures` sets elements of
  `graph.arena.call_target_kinds` and `call_target_names` in place through
  local copies of those formal paths. Those Arrays are the fresh ones.
- The loan frame gives the whole surfaces value the artifact's origin, and
  `function_param_flow_field_preservation_failures` answers per top-level
  field only (`arena` covers both the topology and the call-target rows).

Two ways to the proof, both multi-step: field paths on the loan frame's
origins, through constructors and through callee results (a per-field
return relation in the summaries); or a String-only lend, a loan that
reaches the value only as String bytes, which no Array write changes and
only a release could. Not started.

## Unknowns

- Whether the callees in slice 1 read the changed rows through a helper
  chain; slice 1 refuses where any path is unnamed.
- The cost of a read-path summary per formal on the canonical input; the
  census time stays within the session's 930 to 1036 s spread or the slice
  does not land.
