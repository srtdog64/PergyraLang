# Bounded landing planner experiment (2026-10-24)

Status: piece 1 landed as the chain walk (measurement 6): 4 analyses
instead of 10, same verdict, net 8 more copies. Owner: Claude lane. Raised by the
user on 2026-10-24 after the native checker reached 116 census refusals:
the current landing loop is not practical, and one bounded experiment
should decide whether a planner can replace it. Shape-specific admission
rules are paused until the experiment reports.

## 2026-10-28 active store-candidate slice

- Revision/source: local `main` at `60fd1e26d` plus the preserved dirty V1
  tree; validation copy
  `/home/c/pgy-validation/v1-batch-20261028-2dE4h5`.
- Blocked gate: the declaration-JSON module needs ten whole semantic analyses
  (234.82 s, 839344 KiB) to reach a zero landing round.  The unchanged
  canonical input reached only round 4 in the 30-minute integration budget.
- Repeated owned operation: a later String store in a routine is deliberately
  hidden after that routine's first seed, so the driver reparses and rechecks
  the whole composed program before the indexed-String owner may judge it.
- Objective: let that owner finish judging exact later store calls, keep those
  candidates invisible during the current analysis, and publish them only at
  the existing post-type-check store-owner finalizer.
- Fact owner and last consumer: `indexed_string_borrow_owner.c` owns the exact
  needs-copy judgment; `formal_string_store_landing_owner.c` validates and
  publishes it; `semantic_analyze_landing_fixed_point` is the last consumer.
- Forbidden fallback: no source annotation or Clone, no weaker borrow rule,
  no function-name/source-line exception, no larger timeout, and no candidate
  inferred merely from an Array<String> receiver.
- Falsifier: any changed final landing set or verdict, a candidate visible to
  same-round analysis, a formerly refused alias/borrow case admitted, or no
  reduction in full analyses on the fixed module input.
- Result: rejected.  Giving each independent local `Array<String>` receiver
  its own first-store seed changed the module sequence after round 5.  The
  baseline reached `round=9 selected_stores=0 selected_copies=0`; the
  candidate still selected 194 copies in round 9.  Receiver independence is
  therefore not enough to preserve the planner's semantic order.
- A second candidate let the indexed-String owner judge later stores while
  deferring their publication to the existing finalizer.  It also failed the
  exact-sequence falsifier: baseline round 6 was `33/329`, while the candidate
  produced `44/38` and then `0/41` in round 7.  Delayed visibility alone does
  not make those candidates equivalent to the next analysis generation.
- Conclusion: store-candidate order is semantic evidence, not scheduling
  trivia.  Do not batch these stores without an equal final landing set and
  verdict.  The next bounded step is timing the existing post-pass/finalizer
  operations without changing their publication order.
- Timing showed that the store finalizer is not the blocking operation.  On
  the fixed declaration-JSON module its work was 0.014 s in the first
  analysis and 0.003 s in the second, while the second analysis spent
  11.863 s in `semantic_param_publication_prepare_flow_summaries`.
- A third candidate tried to solve every demanded formal of a routine as one
  routine-wide batch.  It was also rejected.  The second analysis became
  slower (`prepare_flow` 16.252 s instead of 11.863 s) and the exact landing
  sequence changed: the baseline rounds began `73/92`, `138/89`, `1/17`,
  `10/0`, `6/20`, `3/479`; the batch began `73/82`, `138/89`, `1/1`,
  `10/0`, `6/20`, `3/479` (stores/copies).  A routine is not the demand
  boundary owned by the current solver: batching its formals changed which
  facts became visible together.  Do not revive this candidate without an
  equal landing set and verdict.
- The next bounded measurement must stay inside one ordinary formal demand
  and identify its repeated owned operation.  It must not change demand
  roots, publication order, or analysis-generation visibility.
- That measurement found 231767 backing-binding lookups (4549161 linear
  probes) and 197015 backing-node lookups (22852186 linear probes) during the
  slow second preparation pass.  Open-addressed projections reduced the
  probes to roughly one per lookup and preserved rounds 0 and 1 exactly, but
  did not reduce the blocked cost: `prepare_flow` was 13.169 s instead of
  12.432 s on the adjacent baseline run.  The projection was therefore
  rejected and removed.  Comparison count alone was not the bottleneck; the
  measured time remains in the origin-specific control-root walk.
- A whole-run `gprof` measurement corrected that last attribution.  On
  `ast_collection_exclusive_value_owner.pgy`, eight landing analyses made
  227671 `function_param_flow_backing_rows_capture` calls.  Those captures
  asked 7775867 routine rows memos, recorded 505186 full routine checks, and
  called the sealed routine relation-header check 11757367 times.  The
  remaining repeated operation is the rows-currency walk: its routine memo
  avoids most full frame checks, but each closure visit still replays the
  sealed function header before using that memo.
- The bounded fix keeps the relation-header verdict in the existing
  `FunctionParamFlowProgramPointIndex`, whose typed publication inputs are
  sealed for the lifetime of the flow store.  It adds no new semantic owner
  and changes no demand or publication order.  In
  `PGY_VERIFY_FLOW_CURRENCY_MEMO` mode every reuse still performs the original
  header check and fails the store on a different answer.
- On the same isolated current-source tree, the fixed module fell from
  69.48 s and 408608 KiB to 58.70 s and 408580 KiB.  MIR JSON was byte-equal
  and every printed flow-store statistic was equal.  The complete
  `function_param_flow_summary_smoke.sh` passed both normally and with
  `PGY_VERIFY_FLOW_CURRENCY_MEMO=1 PGY_VERIFY_FLOW_WORKLIST=1`, using a Linux
  binary in a path without a stale `.exe` sibling.
- The aggregate Make target is not green yet for an independent reason:
  `readonly_map_query_fact_phase_unit` reports the same 98 failures with and
  without this change on the preserved dirty V1 tree.  This optimization does
  not claim that prerequisite or the V1 integration shard green.
- The `33/329` round-six value above is evidence from the earlier experiment,
  not a baseline for the present dirty source.  A checksum comparison showed
  the isolated semantic sources equal to the current checkout; that source's
  later declaration-module run reached round six as `44/38`.  Future
  candidates must compare against an adjacent build of the same current
  source, not revive the historical round sequence as an oracle.
- The header-verdict cache did not unblock the fixed canonical input.  Both a
  cold and a warm run hit the unchanged 30-minute ceiling with no MIR or
  diagnostic output.  A summary-only run reached round 17 before the same
  ceiling.  The observed generations were `102/652`, `178/112`, `84/32`,
  `2/0`, `11/0`, `2/0`, `7/0`, `10/0`, `3/0`, `19/0`, `154/437`, `0/1`,
  `51/180`, `0/1005`, `5/0`, `65/93`, `0/1`, `0/1` (stores/copies).
- Initial tail hypothesis: exact independent constructor-field copy landings
  are recorded while the same boundary emits a borrowed-escape error.  The
  tail reproduced consecutive copy sites at lines 103 and 142 of
  `SelfMirAssignmentTargetGraphContractReady`, so the bounded candidate was:
  when and only when
  `semantic_loan_copy_landing_select_typed_carrier` records an independent
  whole-value copy for that exact constructor field, do not also emit the
  stale pre-copy escape diagnostic.  Continue checking the routine so its
  remaining exact sites can be planned in the same sealed generation.
  Uncopyable fields, missing identities and failed landing allocation retain
  the ordinary fail-closed diagnostic.  The last consumer remains
  `semantic_analyze_landing_fixed_point`; the falsifiers are any lost
  constructor negative, a changed final landing set/runtime result, or failure
  to bring the unchanged canonical input inside the existing 30-minute
  ceiling.
- Result: rejected and removed.  On the import-composed assignment-target
  contract module, baseline and candidate had the same three generations
  (`73/92`, `90/54`, `0/0`) and the same output.  The focused
  `ref_constructor_copyable_field_moved_to_own_landed_valid` fixture also
  stayed `0/1`, `0/0` with byte-equal MIR.  The later-site serialization is
  therefore not owned by this diagnostic branch; suppressing it added no
  progress and was removed.
- The next falsifier isolates the actual seam: one owned `Array<String>` is
  placed in three nominal fields and read again.  The adjacent baseline needs
  four analyses (`0/1`, `0/1`, `0/1`, `0/0`) because the complete move checker
  records constructor moves only after the earlier finalizers, while the
  landing walk owns no such state.  The intended owner remains the nominal
  result snapshot plus its exact owned-element receipt; the last consumer is
  the default aggregate landing walk.  The bounded candidate lets that walk
  record the same constructor move and treats a copy selected earlier in the
  walk as fresh storage.  It must select the same three source identities in
  one analysis, preserve all uncopyable constructor refusals, and leave final
  MIR/runtime output unchanged.  Missing snapshots, non-owned elements and
  unsupported Clone types remain fail-closed; no constructor-shape exception
  is permitted.
- On the exact 2026-10-29 dirty source at `main` `60fd1e26d`, the fixed
  canonical input still exceeded the unchanged integration budget.  It
  reached landing round 27 and then exited `124` at `30:00.13`, with no MIR
  output and a 2313732 KiB maximum resident set.  The last completed round
  selected 15 typed stores and no copies (`506/956` cumulative before the
  merge).  Fourteen of those store receipts were `OWNED_PUSH` rows whose
  `routine_id` had been lost (`0`) even though the sealed publication row for
  the exact call still owned its caller.  The existing contiguous-store
  consumer therefore could not see their routine and each later store waited
  for another whole-program generation.
- The next bounded fix does not change a demand root, store kind, or
  publication generation.  `formal_string_store_landing_owner.c` must recover
  a missing routine only from `publication_result_current(ctx, call)` when
  that exact row is current and its caller identity is internally consistent.
  A supplied routine remains authoritative; a missing or inconsistent
  publication row remains ownerless and is not batched.  The last consumer is
  still `semantic_analyze_landing_fixed_point`.  Falsifiers are a changed
  focused positive/negative verdict, a different final landing set or MIR,
  acceptance without the current publication identity, or failure to bring
  the unchanged canonical input inside 30 minutes.
- Result: rejected and removed.  Rounds 0 through 12 were byte-for-byte the
  same sequence.  Round 13 moved four stores into round 14 (`32/2`, then
  `27/1`, versus `36/2`, then `23/1`), after which rounds 15 and 16 and every
  cumulative count were again identical.  At `18:52.77`, round 16 still had
  the same `413/946` cumulative set and `1/1` selection as the baseline.
  Caller identity was useful debug provenance but not the scheduling owner;
  it did not reduce a whole-program generation.  The semantic change was
  removed.  The path-bearing debug receipt remains so the next measurement
  can attribute a selected call without another blind full run.
- The blocked-gate profile on the same source and input places the current
  repeated cost inside semantic pass 2, not in the flow finalizer.  One first
  generation spent 31.652 s in `pass2_full_check`; 94,982 binary expressions
  accounted for 14.601 s of exclusive expression-dispatch time.  A second
  phase measurement attributed 11.134 s of that time to
  `type_check_role_operator_overload`, versus 0.125 s to named function
  overload lookup.  The role path calls
  `semantic_host_index_find_next_decl_of_type` once for nearly every binary
  expression, and that function rescans the complete host-declaration array
  even when no role targets the operand type.
- Bounded optimization objective: the existing semantic host index owns a
  generation-local, non-owning `(role for-type name, declaration order)`
  projection built with the rest of that index.  The role lookup consumer may
  traverse only the matching range; it must preserve source order and the
  included-role/operator checks.  There is no cross-generation reuse and no
  new semantic cache owner.  The forbidden fallback is a complete host-index
  rescan after this projection was successfully built.  Falsifiers are any
  changed operator-overload verdict or selected declaration, any changed
  focused ownership landing sequence, or no measured reduction on the fixed
  canonical generation.
- Result of that bounded optimization: role lookup fell from 11.134 s to
  0.093 s in the first generation; pass 2 fell from 33.993 s to 21.170 s,
  and the complete role/operator behavior gate passed 56 C/LLVM executions
  plus 26 negative diagnostics.  The exact canonical run preserved every
  selection through round 27, then completed round 28 with one new store at
  `direct_mir_scalar_program_llvm_direct_call_expression_owner.pgy:96`.
  It still timed out with no MIR at `30:00.09` (`rc=124`, 2313776 KiB), so
  this is a measured unblocker but not canonical closure.
- The next exclusive-time profile, after the role index, attributes 4.420 s
  per first generation to unknown `&&`/`||` operands (35,383 visits).  Those
  operands preserve conditional resource semantics by taking two
  `ResourceConsumeSnapshot`s, merging and restoring them.  The existing
  snapshot owner currently grows ten parallel arrays geometrically, causing
  every snapshot to allocate/copy/free each capacity rung even though its
  tracked-symbol count is fixed during the synchronous capture.  The bounded
  change is to count the same memoized tracked set first, allocate each owned
  array once at the exact count, then capture it; a changed tracked count or
  allocation failure remains fail-closed.  No branch rule, identity, merge,
  restoration or snapshot consumer changes.  Falsifiers are a changed flow
  positive/negative verdict, `PGY_SEMPERF_VERIFY_CLASSIFY_MEMO` divergence,
  a changed canonical selection sequence, or no reduction on that input.
- Result: rejected and removed.  The second scope walk cost more than the
  avoided allocation ladders: `&&`/`||` exclusive time rose from 4.420 s to
  10.102 s and pass 2 rose to 39.732 s.  Exact-size pre-counting is therefore
  not the blocking-operation fix; the one-pass snapshot implementation remains.
- The next bounded candidate stays inside the merge owner.  Before/after
  snapshots of one conditional normally carry the same live `Symbol *` rows
  in the same order, but `merge_resource_states` still searches the complete
  source snapshot for every destination row.  It will first accept an exact
  same-position identity (stable universe index when present, otherwise the
  same live Symbol pointer) and retain the existing complete search whenever
  positions differ.  Merge rules and the slow path are unchanged.  A reordered
  branch snapshot, duplicate/missing identity, changed verdict or unchanged
  canonical cost falsifies the candidate.
- Result: rejected and removed.  Four short-circuit C/LLVM parity cases passed,
  but under the adjacent timing run pass 2 changed from 29.308 s to 30.008 s
  and `&&`/`||` exclusive time from 4.420 s to 4.536 s.  Positional merge
  lookup was not the blocking operation.
- The next measurement identified the repeated owned operation rather than
  another allocation strategy.  In one first-generation analysis, 115647
  resource snapshots visited 1166831790 scope symbols but retained only
  3452327 tracked rows.  Snapshot growth itself cost 0.564 s and universe
  binding 3.119 s.  The dominant work is therefore repeated traversal of the
  program-global declaration scope from inside function-local control flow.
- Bounded scope objective: `snapshot_resource_states_from_scope` remains the
  resource-state owner and captures every live scope through the enclosing
  ordinary function, but does not inspect its parent program declarations.
  Lambda function scopes are transparent to this boundary because their
  bodies still resolve checked captures in the enclosing function; nested
  lambdas therefore continue until the first non-lambda function scope.
  Top-level implicit-Main checking has no function scope and retains the
  existing complete walk.  The last consumers remain the branch, loop,
  transaction, conditional and parallel snapshot joins.
- Forbidden fallback: no declaration-kind/name whitelist, no missing-universe
  permission, no changed merge/restore rule, and no exclusion of a checked
  lambda capture or implicit-Main value.  Falsifiers are a snapshot containing
  a parent-global tracked value while inside an ordinary function, a missing
  enclosing-function value inside a lambda, a missing implicit-Main value, any
  changed focused flow verdict, or no reduction on the fixed canonical first
  generation.
- Result: the function boundary reduced 115647 snapshots from 1166831790
  visited symbols to 1313887 while retaining 676775 tracked rows.  Conditional
  snapshot time fell from about 8.8 s to 0.819 s and first-generation pass 2
  from 37.359 s to 17.443 s.  The dedicated ordinary-function/lambda/implicit-
  Main unit passed; six C/LLVM short-circuit and lambda cases passed; the full
  operator gate passed 56 executions and 26 negative diagnostics.  The broader
  semantic unit binary remained independently red at 3231 passed / 15 failed
  on the preserved dirty tree, so it is not recorded as a green prerequisite.
- Exact canonical evidence used product binary SHA-256
  `bb4f272fac96beee2b06ce383e60309a46a002de4cd9ef2510471699a86600ae6`
  and the unchanged command `bin/pgy --native-pipeline --mir-json
  --error-format=json src/self_hosted/compiler/driver_bootstrap_main.pgy` in
  `/home/c/pgy-validation/v1-batch-20261028-2dE4h5`.  It preserved rounds 0
  through 28, reached round 34, then exited 124 at 30:00.14 with no MIR and a
  2331992 KiB maximum resident set.  This is six generations of additional
  progress, not canonical admission or V1 closure.  The receipt is
  `/root/codex-v1-temp/canonical-scopebound-full.err`.
- The next bounded seam is not a routine-wide batch.  Four consecutive formal
  String stores in
  `DirectMirScalarProgramLlvmDirectCallExpressionAt` landed one per generation
  at lines 95, 96, 101 and 102.  The existing finalizer may extend only over
  current program-point claims; a later store whose value snapshot was not
  completed after the first refusal remains unprovable.  The objective is to
  let the existing fact producer finish those current claims without selecting
  or admitting the stores in the same generation.  The finalizer keeps sole
  selection authority and every replay reruns ordinary ownership checks.
  Falsifiers are any changed final landing set or verdict, crossing the
  existing non-store statement boundary, admitting a stale/aliased value, or
  merely moving selections between rounds without reducing the exact input's
  completed-generation count.

## Objective card

- Objective: compute the copy landing set of the unchanged canonical input
  from function summaries and landing candidates alone, apply it once,
  verify once, and fail when verification selects a new landing.
- Priority: correctness of the landed program, then a bounded and
  measured cost, then the number of copies.
- Fact owners today: the flow summary store (summaries, demand episodes),
  the move, local copy, two-part and receipts owners (candidates, via
  `loan_copy_landing_record` and the String store landing).
- Last consumer: `driver_semantic_analyze_landed` (`src/compiler/driver_app.c`),
  which reloads the source and re-runs the whole analysis per round.
- Forbidden: a reduced canonical input, a larger time or memory allowance,
  a copy where no rule selected one, and counting a passing shadow run as
  admission.

## Facts measured before the experiment

| Fact | Value | Record |
|---|---|---|
| census landing analyses (census mode) | 10, rounds 2-8 add 1-3 copies each | [census](../truth/evidence/2026-10-24_native_census_after_main_merge_flow_repairs.md) |
| landed copies, census mode with receipts | 3382 (2321 without the receipts pass) | [receipts](../truth/evidence/2026-10-24_native_census_string_receipts_after_rows_repair.md) |
| product compile of the canonical input | 185 s, 3 analyses, 113 copies (it stops at earlier errors) | measured with `cc-selsite` |
| each round applies the whole cumulative set to the original source | yes: the last round applied 2321 copies at once and selected none | `driver_app.c` |

So steps 3 to 5 already hold for the final set; the open question is
step 2, computing that set without whole-program rounds.

A flow summary failure closes the store for the whole program; the census
count then measures the cascade, not the distance. Three such failures were
repaired on 2026-10-24; two remain in late demands
(`CollectionRuntimeNominalRecordArrayBlock`, `JsonEmitFieldRaw`).

## Steps

1. Run the planner in shadow mode on the unchanged canonical input.
2. Fix summaries and landing candidates to a fixed point, re-walking only
   the routines whose candidates or callee summaries changed, recursion
   inside its strongly connected component.
3. Apply the landing set once.
4. Verify the whole program once.
5. A landing the verification selects is a failure of the planner.
6. Record candidates, fact promotions, queue work, iterations per SCC and
   memory.

## Measurement 1: who selects a candidate after the first analysis

Census mode with the receipts pass at `f22d406d9`, each selection printed
with the file and line that selected it (`scratchpad/hm/planner_measure1.sh`).
A candidate counts once, in the analysis that first selects it.

| Analysis | New candidates | Selecting owner |
|---|---|---|
| 0 | 3201 | two-part and local copies (`default_aggregate_copy_owner.c:309`) 999, String receipts 645, String field lineage 262, ref call loans (`type_checker_ownership_param_summary.c:129`) 242, moves 232, String borrows 173 |
| 1 | 169 | String definition recipes (`indexed_string_definition_recipe.c:628`) 100, ref call loans 54, moves 8 |
| 2 to 8 | 12 in all, 1 to 3 each | move owner loan copies (`default_aggregate_move_owner.c:172-173`) in `ApplyPostfixFact` and `DirectMirScalarCfgGraphPlanFromAdmitted` |
| 9 | 0 | (the verification analysis) |

95 % of the set is known after one analysis. The rest is of two kinds:

- second-order candidates that only exist once earlier copies are in the
  program (definition recipes, ref call loans): the copy changes the
  String definition or the loan the next analysis sees;
- a chain inside one routine: the move owner refuses one loan of `base`
  per analysis in `ApplyPostfixFact`, because the next loan only fails
  once the previous one is copied. Compiling that module alone selects
  one copy and stops; the chain needs its callers' summaries from the
  whole program.

## Step 2 design

1. Candidates are facts, not diagnostics: each owner that selects a copy
   records it in one candidate table (site, owner, reason), and keeps
   walking instead of stopping at its first refusal.
2. Within one analysis, an owner reads its own routine's candidates as
   landed: the move owner treats a copied argument as no move of its
   path, and the String definition and loan owners treat a copied value
   as a fresh definition. A routine whose candidates grew is walked again;
   that is the per-routine fixed point.
3. Summaries change only through a routine's own facts, so a routine is
   walked again when a callee summary it read changed, inside its SCC.
4. Cost is bounded by the candidate namespace (one source node, one
   copy), as today's driver bounds rounds.
5. One reload applies the table; one verification analysis must select
   nothing new.

What to build first, smallest falsifiable piece: the move owner's in-walk
reading of its own loan copy candidates, measured on the census input by
the number of landing analyses (today 10) and the selected set (it must
equal today's 3382 minus nothing and plus nothing).

## Measurement 2: piece 1 as "copy every escape"

The sparse loan walk recorded every expression that carried the loan out,
and the move owner copied all of them when a copy could stand at each.
Census mode with receipts, plan1 = `f22d406d9`, plan2 = plan1 plus the
piece (`scratchpad/hm/planner_measure2.sh`).

| Fact | plan1 | plan2 |
|---|---|---|
| landing analyses after the first | 9 | 3 |
| census wall time | 2734 s | 1047 s (peak 3.98 GB) |
| selected copies | 3382 | 3390: none missing, 8 extra |
| final refusals (outside direct_mir) | 116 (75) | 116 (75) |
| flow summary and aggregate storage smokes | pass | pass |

The falsifier (an equal set) fails. The 8 extra copies sit in three
routines: `SelfMirIntentCleanupInvalidationsAppend` (5),
`SelfMirRoutineAttachLastExpressionGraph` (2, return values) and
`CodegenIntentExecutionPlanDefinitionBlock` (1). The walk reads a
binding's origins without flow order, so once the loan reaches `build`,
every read of `build` carries it, including one before the loan
(`intent_resource_lifetime_owner.pgy:84`). plan1 copied the first carrier
only, and its next analysis found the loan staying.

## Piece 1 revised: one walk per copy, inside one analysis

The walk repeats with each first carrier read as a copy already made (its
value reads its source as a Clone would and holds no origin), up to 16
carriers; the move owner copies the chain it found. When a step ends where
no copy can stand, or the owner changed, the loan is copied where it is
made and the chain is dropped. A second form also reads a store this
analysis already selected to land (the walk itself selects inout-output
stores, `semantic_formal_store_landing_select_held`) as a copy, made by the
store owner. On the two new fixtures:

| Fixture | plan1 | chain and stores |
|---|---|---|
| loan pushed into two inout Arrays | Clone at the first push plus two owned pushes, 1 landing analysis | two owned pushes, 1 |
| loan pushed, then given to an `own` formal | Clone at the push and at the getter, 2 analyses | Clone at the getter, 1 |

plan1 copies the same String twice there (`ArrayPushOwnedString(out,
Clone(first))`). The flow summary smoke, run with every failure listed
instead of the first, changes on one fixture only
(`by_value_getter_lend_kept_landed_valid`: the owned push alone, no
Clone); the two new fixtures fail on plan1.

## Measurements 3 and 5: the chain walk on the census input

| Fact | plan1 | plan3 (chain) | plan5 (chain, stores, destinations) |
|---|---|---|---|
| landing analyses after the first | 9 | 3 | 3 |
| census wall time | 2734 s | 1655 s (other runs beside it) | 1015 s |
| selected copies | 3382 | 3378 (26 missing, 22 extra) | 3376 (28 missing, 22 extra) |
| final refusals (outside direct_mir) | 116 (75) | 119 (78) | 119 (78) |

plan5 also reads a copied value as going nowhere its source goes (the
destination walk). Both add the same three refusals, all of `base` in
`ApplyPostfixFact` (`expr_postfix_owner.pgy:199, 251, 276`). The chain
diagnostics (`PGY_CENSUS_CHAIN`) show why: for the loans made at 159 and
228 the walks find ten copies each, after which the loan stays but its
owner still changes, so the move owner falls back to copying the getter
call, which no copy can stand at, and selects nothing. plan1 copies the
same carriers one per analysis, and its last analysis finds the owner
unchanged: another loan's copy in that routine (such as `base` at 160:48)
is in place by then, which a walk of one loan cannot read. Measurement 6
keeps the chain in that case when the getter cannot be copied.

The other gates on the plain tree: flow summary and aggregate storage pass;
`string_result_control_ownership` fails on plan1's tree as well (its C
fresh factories do not compile there).

## Measurement 6: the chain kept when the owner still changes

When the walks find the loan staying but its owner still changing, the
move owner copies the getter call if a copy can stand there, and otherwise
makes the chain's copies; the next analysis judges the owner with them.

| Fact | plan1 | plan6 |
|---|---|---|
| landing analyses after the first | 9 | 3 (the last selects nothing) |
| census wall time | 2734 s | 1156 s (peak 3.98 GB) |
| selected copies | 3382 | 3390: 14 missing, 22 extra |
| final refusals (outside direct_mir) | 116 (75) | 116 (75), the same kinds |

The falsifier of piece 1 (an equal set) still fails, by a net 8 copies:

- 5 of the 22 copy a getter call where plan1 copied one carrier of the
  same loan (`SemanticAstExpressionVerdictFromGraphCore`,
  `SemanticAstIterationTypeFactsFromArtifact`,
  `SemanticAstBodyTypeBundleFromAdmittedAnalysisObservedWithIdentityPolicy`,
  `CodegenIntentExecutionPlanDefinitionBlock`): one copy for one.
- 17 are later links of chains (`EmitStmtList` 7, where the chain reached
  its cap of 16; `SelfMirRoutineExpressionRuntimeAbiAttach` 6;
  `SelfMirIntentCleanupInvalidationsAppend` and `SelfMirIntentRoutineBuild`
  2 each) that plan1's next analysis did not find: a copy read as made is
  not yet the Clone the next analysis sees.
- 9 of plan1's copies are not needed (5 in
  `DirectMirScalarCfgGraphPlanFromAdmitted`).

So the planner question is answered for this piece: one analysis plus one
verification replaces nine landings with the same verdict, at a small
copy cost. What it does not answer is a copy set that is minimal or equal
to the round driver's; that needs the candidates of every loan in a
routine read together (step 2, point 2), which this piece does not do.

## Open decision for the user

Giving up completeness means refusing some sharing the canonical input
really has (for example the nine `valid` uses after
`SemanticAstExpressionSurfaceFactsFromTypedArtifact` returns a value
lending it and the caller writes through it). V1 asks for the unchanged
canonical program; admitting it then needs either a proof that does not
exist yet or an explicit `Clone` in its source.

## 2026-10-29: store-kind owner reuse at the program-point boundary

- **Objective:** remove the duplicate store-kind decision in the bounded
  program-point selector. `store_landing_kind_for_call` remains the single
  owner of `VALUE_COPY` versus `OWNED_PUSH`; the generic record path must not
  force every exact program-point receipt back to `VALUE_COPY`.
- **Priority:** preserve the exact store identity and safety refusal first,
  reuse the current typed owner second, remove the redundant replay third,
  and reduce canonical wall time only as a consequence.
- **Fact owner / last consumer:** the formal String store landing owner owns
  the typed landing kind. `semantic_analyze_landing_fixed_point` is the last
  consumer that merges that kind into the replay plan.
- **Forbidden fallback:** no syntax-only `ArrayPushOwnedString` guess, no
  weakening of receiver admission, and no second hard-coded kind decision in
  the program-point recorder.
- **Verification / falsifier:** the complete function-parameter flow gate must
  keep its positive and negative verdicts; populated or mixed-ownership
  receivers must not gain an owned-store route. On the fixed canonical input,
  the exact `DirectMirScalarProgramLlvmExpressionAt` store at line 106 must be
  selected once with its final kind rather than first as `VALUE_COPY` and
  again as `OWNED_PUSH`. A changed final MIR, runtime result, or negative
  verdict falsifies the change.

## 2026-10-29: caller-private inout output closure

- **Objective:** let the declared-result backing frame distinguish a bounded
  write to an `inout Array` supplied by an exact caller-private edge from an
  unclassified mutable output. `MirDestructureTemporaryAt` currently reaches
  `MirDestructureTakenNamesInto` with a fresh local `names` Array, but the
  routine-wide row check collapses that proven edge back into
  `mutable_output_required` before the backing frame can inspect the result.
- **Priority:** preserve the mutation and result identities first, keep the
  contextual permission caller-specific second, reject every unclassified
  mutation third, and only then remove the false stale relation.
- **Fact owner / last consumer:** the program-point index owns whether the
  routine contains only contextual Array outputs or an unbounded mutable
  output. `function_param_flow_no_store_graph_capture` is the last consumer;
  it may use the contextual fact only on the exact `private_outputs` edge.
- **Required change set:** classify a current `ArrayPush`/`ArraySet` whose
  receiver is this routine's `inout Array` as contextual output; continue the
  body walk so any other unsupported mutation still sets the unbounded flag;
  reject contextual output in every global no-output/closed-selector path;
  allow it only while traversing a caller-private output edge; keep such a
  closure out of the global admitted no-store set.
- **Forbidden fallback:** no function-name, source-line, or `names` exception;
  no blanket permission for `mutable_output_required`; no source `Clone`; no
  assumption that an `inout` actual is private without the existing exact
  local/forwarded edge proof.
- **Verification / falsifier:** `src/self_hosted/mir_lower/main.pgy` must no
  longer fail at `EmitStructDecls:509`; a contextual-output routine queried as
  globally no-output must remain refused; a non-local or aliased inout actual,
  an unsupported mutation, or a returned backing dependency must remain
  fail-closed. The focused flow positive/negative gate and the unchanged
  canonical input are the integration gates.

## 2026-10-29: loop-local collection retirement from retention facts

- **Objective:** issue the existing iteration-exit cleanup for a loop-local
  `Array<String>` when every use is an exact synchronous observation. The
  reached canonical case mutates the array, compares one indexed String, and
  joins the array into a fresh String before the loop backedge.
- **Priority:** preserve exact call/declaration identity first, consume the
  retention registry instead of re-deriving it second, admit only bounded
  indexed scalar observations third, and issue the cleanup edge last.
- **Fact owner / last consumer:** `builtin_argument_retention_registry.def`
  owns builtin and stdlib argument retention by identity, ordinal, and arity.
  The MIR expression graph owns the exact index and comparison topology.
  `mir_loop_collection_cleanup.c` is the last consumer that decides whether
  the loop edge may retire the local storage.
- **Required change set:** register `StringJoin` and `Join` as two-argument
  borrowed-for-call operations; remove the MIR consumer's duplicate
  return-type allowlist; accept one exact indexed String used only by equality
  or inequality; keep the existing escape, defer, and mutating-carrier
  refusals; regenerate the self-host projection.
- **Forbidden fallback:** no routine name, source line, binding number, or
  canonical-only exception; no broad permission for arbitrary indexed values,
  user-declared calls, calls absent from the registry, or retained results.
- **Verification / falsifier:** the minimal empty-birth loop and the actual
  `EmitDeclZoneAuthorities` route must contain a cleanup edge and pass MIR
  validation. C/LLVM execution must agree for `StringJoin` and `Join`; a
  user-declared mutating `Join`, indexed escape, defer, or retained carrier
  must still fail without publishing an artifact.

## 2026-10-29: branch-local Array<String> element premise

- **Objective:** keep the collection owner's exact all-elements-owned premise
  local to the CFG paths that reach a call. A return-only branch may transfer
  an empty local Array, but that terminal transfer must not revoke the same
  binding on the surviving path.
- **Priority:** preserve fact identity first, exclude terminal paths at the
  existing flow join second, merge every surviving path fail-closed third, and
  issue the inout receipt only from the restored current premise.
- **Fact owner / last consumer:** `array_storage_element_lifetime_owner` owns
  the return-element premise. The resource-flow snapshot owns its path-local
  copy while checking alternatives. `collection_owned_element_requirement`
  is the last consumer at the exact inout actual.
- **Required change set:** capture the premise beside the existing Array
  storage path state; validate function, binding and dependency identity on
  restore; AND ownership across fallthrough paths; clear a divergent
  dependency; include the premise in loop convergence and snapshot copies.
- **Forbidden fallback:** no source-name or line exception, no blanket empty
  literal permission, no reinterpretation of UNKNOWN as owned, no source
  `Clone`, and no scan of the callee or caller AST at finalization.
- **Verification / falsifier:** an empty local returned only by a terminal
  branch must remain admissible at a later owned-mutating inout call. A
  fallthrough branch that shallow-stores a borrowed String must still revoke
  the premise and fail; an explicit owned String store must remain admitted.
  The isolated nominal-array owner and unchanged canonical input are the
  integration gates.

## 2026-10-30: direct aggregate-member element premise

- **Objective:** let a moving default `Array<String>` actual consume the
  existing owned-element proof for an exact immutable aggregate member path.
  The reached canonical call passes `arena.topology.node_texts` directly to
  `MirExpressionGraphSequenceFromExtendedRows`; the generic Array receipt has
  no binding identity for that member expression even though the aggregate
  provenance graph already owns its definitions and returns.
- **Priority:** preserve the member/root identities first, reuse the sealed
  aggregate provenance graph second, reject borrowed or ambiguous definitions
  third, and admit the move only after the whole path proves ownership.
- **Fact owner / last consumer:**
  `collection_owned_element_member_requirement_owner` owns the aggregate
  field-path proof. `semantic_collection_owned_element_requirements_finalize`
  is the last consumer at the exact moving default actual.
- **Required change set:** capture a direct current local/formal member path at
  the call; store its function, root, field and formal identities in the
  existing actual snapshot; replay the published path at finalization; ask the
  aggregate proof graph to validate every local definition, return and caller;
  use that result only when the ordinary Array element receipt is absent.
- **Forbidden fallback:** no field-name, source-line or canonical-call
  exception; no `UNKNOWN`-as-owned rule; no new local carrier or source
  `Clone`; no trust in a member type without the captured path and producer
  graph; no duplicate aggregate provenance store.
- **Verification / falsifier:** a direct member built solely from owned String
  results must move into an owned-mutating default callee on C and LLVM. The
  same shape with a borrowed String element, a mutable root, a changed field
  path or an unproved caller must remain refused. The full function-parameter
  flow gate and the unchanged canonical input are the integration gates.

## 2026-10-30: loop-replayed aggregate member carrier premise

- **Objective:** preserve the exact owned-element premise of an
  `Array<String>` local moved from an aggregate member across every loop
  replay and an `inout` mutation. The first iteration and every backedge must
  independently prove the same current source; a proof from the first visit
  is not reusable merely because the call syntax is identical.
- **Priority:** exact source and binding identity first, fail-closed
  currentness second, recurrence proof at the loop join third, and canonical
  admission last.
- **Fact owner / last consumer:** `PgyCollectionOwnershipFact` owns the moved
  member's exact source path and source birth. The resource-flow loop snapshot
  owns its path-local state while the body is replayed.
  `semantic_collection_owned_element_requirements_finalize` is the last
  consumer at the exact `inout Array<String>` actual.
- **Required change set:** capture every fact component needed to revalidate
  the moved member source, restore it only to the same live binding and source
  generation, merge divergent loop paths fail-closed, and preserve the
  premise through an `inout` mutation that neither replaces nor borrows the
  element storage.
- **Forbidden fallback:** no duplicate-call snapshot suppression, no stale
  first-iteration result, no `UNKNOWN`-as-owned rule, no name or source-line
  exception, no source `Clone` or manual field carrier, and no second
  provenance store.
- **Verification / falsifier:** an owned member carrier appended on two loop
  iterations must remain current on both C and LLVM and produce the same
  result. A borrowed, mutable, rebound, or source-generation-mismatched
  carrier must remain refused. The full function-parameter flow gate and the
  unchanged canonical input are the integration gates.

## 2026-10-30: earliest shallow store on an owned member carrier

- **Objective:** land the first shallow `String` push that would invalidate an
  exact moved-member element premise before a later store tries to consume
  that premise. The replayed call may preserve the premise only when the
  existing typed store plan copies that exact value.
- **Priority:** exact call and member-path identity, earliest invalidating
  store, typed `VALUE_COPY` replay, then later owned stores and deep cleanup.
- **Fact owner / last consumer:** `indexed_string_borrow_owner` owns the exact
  String store that needs an independent copy;
  `formal_string_store_landing_owner` owns the typed plan; the collection
  effect owner consumes that plan when deciding whether the moved-member
  premise remains current.
- **Required change set:** select the existing value-copy plan at the first
  unplanned private String push into a current aggregate-member carrier;
  replay it before recording a shallow effect; preserve the premise only for
  that exact planned copy. Keep `ArraySet` and every unproved member path on
  the ordinary revocation path.
- **Forbidden fallback:** no restoration after an arbitrary shallow mutation,
  no all-`ArrayPush` exemption, no source-line or routine-name exception, no
  second mutation store, and no promotion of a value-only copy into an owned
  receiver cleanup policy.
- **Verification / falsifier:** three consecutive pushes into a moved owned
  member must select the earliest required copy and execute with C/LLVM parity.
  The adjacent loop with a borrowed `ArraySet` must still fail at its inout
  consumer. The full flow gate and canonical input remain the integration
  gates.

## 2026-10-30: exact member-copy candidate lookup budget

- **Revision / executable / input:** local `main` at `60fd1e26d` plus the
  preserved dirty V1 tree; product binary SHA-256
  `fd2c24a7df5b2da99a3a66a13e4927cf5bdad1b47f9cb74033c71c65c661720e`;
  unchanged canonical input SHA-256
  `8e5e115bbe25063f57b03a58f44cbcf16e8d86e104de99a6efc708a3fa565a12`.
- **Blocked gate / observed cost:** the exact canonical command produced no
  MIR or diagnostic before the unchanged 30-minute limit (`rc=124`). Peak RSS
  was 2418012 KiB; user/system CPU were 988.69/34.38 seconds. The complete
  function-parameter flow gate passed on the same binary, but exceeded its
  five-minute focused budget.
- **Reached owner / last consumer:**
  `formal_string_store_landing_owner` owns the exact pending candidate;
  `collection_ownership_fact_result` is its one effect-recording consumer.
  The sealed landing finalizer remains the only authority that may select or
  refuse the candidate.
- **Measurement before change:** the new pending-candidate query linearly
  scans the program-wide candidate array. Instrument only that owned query on
  the fixed canonical input and record lookup, comparison, hit and miss
  counts. A timeout alone is not evidence that this scan is the bottleneck.
- **Permitted change if measured:** replace only the repeated membership scan
  with an exact one-use producer/consumer handoff inside the same semantic
  generation. The handoff grants no owned effect; the candidate array and
  finalizer keep the semantic fact and replay obligation.
- **Forbidden fallback / falsifier:** no input reduction, timeout increase,
  skipped replay, candidate-as-owned inference, cross-generation reuse or
  relaxed negative. The change is rejected if the measured scan is not a
  material repeated operation, if any focused verdict/landing count changes,
  or if the unchanged canonical gate does not get back inside 30 minutes.
- **Result:** rejected without an optimization. A SHA-checked WSL-local source
  snapshot removed p9 input traffic and spent 579.69 user seconds in a
  10-minute bounded run. By then the query had made 731 lookups and only
  32000 candidate comparisons (303 hits, 427 completed misses at the last
  report, with 59 candidates). The count stayed unchanged across long compute
  phases. This scan is not the repeated operation blocking canonical closure;
  the temporary counters were removed and no cache or one-use handoff was
  added. The run still produced no MIR or diagnostic before `rc=124`, so the
  next measurement must attribute the remaining semantic compute rather than
  optimize this candidate membership check.

## 2026-10-30: pending dynamic owned-String result

- **Objective:** let one pending semantic generation carry a user-call result
  that its exact owned-result receipt will prove independent after flow
  preparation. Today only receipts needing static-return normalization may be
  deferred. A receipt whose sole source is an independent dynamic result such
  as `TextBuilderFinish` sets the final body-summary bit, but every caller sees
  it only in the next whole-program generation.
- **Priority:** exact call/callee/receipt identities first, published source
  shapes second, recursive post-flow ownership proof third, and owned push
  effect publication last.
- **Fact owner / last consumer:** `PublishedOwnedStringResult` owns the
  routine's result sources and syntax. The existing
  `ArrayStoragePendingOwnedStringResult` carries no permission and records the
  exact caller store. `pending_owned_string_result_current` is the last
  consumer and must still prove `semantic_collection_owned_string_call_result`
  after flow preparation before publishing the effect.
- **Required change set:** admit a current non-normalizing owned-result receipt
  to the existing pending-result path; do not publish a body summary or an
  owned push from the candidate query itself. Keep recursive receipt-cycle
  rejection and exact source/currentness checks in the finalizer.
- **Forbidden fallback:** no trust in return type, body-summary bit, callee
  name, source line or candidate status alone; no borrowed/static literal
  promotion; no missing receipt or source; no skipped post-flow replay and no
  source-level `Clone`.
- **Verification / falsifier:** the minimal `TextBuilderFinish` result fixture
  must keep byte-equal final MIR and C/LLVM output while removing its two
  extra landing stores. Existing borrowed, retaining, recursive and
  uncopyable-result negatives must remain refused. The declaration-JSON
  module must preserve its final MIR while eliminating the one-store bridge at
  line 314 and the resulting JSON call-depth rounds. The unchanged canonical
  input and full function-parameter flow gate remain integration gates.

### Result: rejected and rolled back

- The candidate-only change preserved the minimal fixture's final MIR exactly
  (SHA-256
  `33a9bd9277da3c338da11fe0f6068db1018c070641fc8c9fb72050ca2f00356c`),
  but it did not remove either store. It serialized the two stores across two
  landing rounds instead of selecting both in the first round.
- The declaration-JSON module retained the same round-3 bridge at
  `declaration_json_projection_owner.pgy:314` and the same round counts through
  that point (`24/69`, `31/32`, `173/121`, `1/1`). The proposed candidate did
  not shorten the reached call-depth chain.
- The broadened pending receipt therefore failed its named performance
  falsifier and was removed. The temporary fixture and instrumentation were
  removed as well. No cache, wider receipt authority, or larger budget was
  adopted. The remaining blocked operation is still whole-program semantic
  replay between landing generations, not candidate lookup or receipt shape.

## 2026-10-30: moved-member declared-read prefix

- **Objective:** preserve an exact moved `Array<String>` member premise across
  an intervening declared read only when a later owned-element mutation uses
  that premise and the sealed callee summary proves the intervening call is
  read-only.
- **Priority:** exact member-source identity, exact call/formal identity,
  sealed read-only summary, then the later `inout` or member-push consumer.
- **Fact owner / last consumer:**
  `CollectionOwnedElementRequirementStore` owns the call-actual snapshots and
  moved-member mutation requirements. Its finalizer is the last consumer;
  generic array-storage pending rows remain unchanged.
- **Required change set:** record a provisional moved-member read receipt at
  call entry; keep only the member premise provisional; mark earlier matching
  receipts required when an `inout` replacement or member push consumes the
  premise; validate required receipts after parameter-flow publication.
- **Forbidden fallback:** no generic pending-call row for escaped member
  carriers, no trust in parameter mode alone, no post-call fact replay, no
  source-line/callee exception, and no new rejection for a read whose premise
  is never used later.
- **Verification / falsifier:** the production expression-admission module
  must pass its read-then-`inout` chain. A retaining, writing, releasing or
  escaping intervening call must still refuse the later mutation. Existing
  member/inout positives and negatives, the full flow gate and unchanged
  canonical input remain the integration gates.

## 2026-10-30: empty-local String array at an owned `inout` boundary

- **Objective:** when an exact empty local `Array<String>` reaches a declared
  `inout` callee that requires owned elements, connect that boundary demand to
  the existing receiver-wide String-store landing plan. The plan must copy or
  transfer every reached shallow String push before replay; the source program
  must not gain manual `Clone` or deep-drop ceremony.
- **Priority:** preserve the callee/formal requirement and local binding
  identity first, reuse the existing owned-store selector second, keep
  borrowed or populated arrays refused third, and admit the formatter only as
  a consequence.
- **Fact owner / last consumer:** the formal String store landing owner owns
  `VALUE_COPY` versus `OWNED_PUSH` for exact `ArrayPush` calls. The owned-element
  requirement finalizer is the last consumer of the callee's `inout`
  requirement and may request that existing plan only for a published empty
  local receiver in the same routine.
- **Required change set:** expose one owner-directed selector from the landing
  owner that finds an exact shallow push for the empty local receiver and
  delegates the receiver-wide rewrite to the current selector; invoke it when
  an `inout` actual lacks an owned element receipt, then replay the unchanged
  program through the normal fixed point.
- **Forbidden fallback:** no permission from parameter mode, callee name,
  source line or empty type alone; no shallow element admission at a deep
  cleanup boundary; no manual formatter rewrite; no promotion of a nonempty,
  aliased, escaped or otherwise unproved receiver.
- **Verification / falsifier:** `src/self_hosted/fmt/layout_owner.pgy` must
  compile through the focused native pipeline. A borrowed/nonempty String
  array passed to an owned-element `inout` writer must remain
  `PGY_SEM_BORROW_ESCAPE`. The complete function-parameter flow gate and then
  the unchanged canonical input are the integration gates.

## 2026-10-30: MIR-C request identity before request transfer

- **Objective:** move one `DriverRung2MirCMachineRequest` into the MIR-C
  producer while preserving an independent, immutable protocol identity for
  the caller's receipt validation and rejection diagnostic.
- **Priority:** exact observation and machine identity first, one transfer of
  the full machine request second, fail-closed receipt validation third, and
  no source-level copy of the nested machine declaration.
- **Fact owner / last consumer:** `driver_mir_c_protocol_owner` owns the
  request identity `(observation mode, machine mode, manifest id,
  fingerprint)`. `DriverRung2MirCPayloadReceiptReadyFor` and
  `DriverRung2MirCPayloadAdmissionDiagnostic` are its last consumers after the
  full request has moved into `DriverRung2MirCProducePayloadAdmitted`.
- **Required change set:** snapshot the two request axes before transfer; copy
  only the two retained manifest strings into that snapshot; make receipt and
  diagnostic observers consume the snapshot; remove their reconstruction
  from the moved machine request. Apply the same contract to artifact and
  stdout callers.
- **Forbidden fallback:** no whole-request `Clone`, broad `ref`, weakened move
  refusal, duplicated request read after transfer, source-line exception, or
  validation derived from rendered payload text.
- **Verification / falsifier:** the two production callers must compile
  through the native pipeline without `PGY_SEM_MOVE_FROM_RELEASED`; mismatched
  request identity must still reject the receipt or rejection payload. Then
  rerun the unchanged canonical input before bootstrap or installed-driver
  claims.

## 2026-10-30: same-generation owned-String result closure

- **Objective:** close the exact owned-String result and local-Array store
  dependency graph inside one sealed publication generation, then let the
  existing landing driver apply that typed set once. The unchanged canonical
  input must stop paying for one whole-program semantic replay per call depth.
- **Measured blocker:** with copy landings fixed at 912 after round 2, rounds
  3 through 10 still added 1, 14, 14, 7, 11, 12, 19 and 42 owned-String
  stores. The path sequence progressed from declaration JSON through
  expression JSON, instruction JSON, block JSON and routine JSON. Ten minutes
  ended at 519 cumulative stores with no MIR. This is a generation-visibility
  chain, not candidate lookup cost or source-specific syntax.
- **Priority:** exact call and routine identity, sealed result receipts,
  recursive cycle rejection, typed store selection, then fewer semantic
  generations.
- **Fact owner / last consumer:** `PublishedOwnedStringResult` owns each
  routine's exact result sources. The collection ownership finalizer owns the
  sealed recursive proof. `formal_string_store_landing_owner` consumes only
  the resulting exact store candidates; `semantic_analyze_landing_fixed_point`
  remains the last landing-plan consumer.
- **Forbidden fallback:** no source-level `Clone`, function-name or source-line
  exception, assumed ownership from a String return type, stale pre-copy
  verdict, reduced canonical input, global cache, larger timeout, or weakened
  borrowed/retaining/recursive refusal.
- **Verification / falsifier:** the existing owned-result long-chain positive
  and borrowed/cycle negatives must retain their verdicts and runtime output;
  the full function-parameter flow gate must pass; the declaration-JSON and
  unchanged canonical inputs must preserve final MIR while completing fewer
  whole-program generations. A receipt graph with a missing source, stale
  identity or cycle must fail closed.

## 2026-11-01: exact indexed-String receipt-chain batching

- **Objective:** let the existing indexed-String receipt owners inspect one
  sealed generation in owner order after an earlier owner has selected an
  exact next-generation landing.  The reloaded AST remains the only current
  ownership proof; this step only batches exact boundary selections which the
  old fixed point discovered in later rounds.
- **Priority:** preserve the final landing identity set first, preserve every
  missing/stale/cycle refusal second, reduce whole-program analyses third, and
  meet the existing 120-second focused gate last.
- **Fact owner / last consumer:** nonowner, field, permanent, definition,
  shallow-retention and private-push receipt owners keep their existing facts
  and selectors. `indexed_string_borrow_receipts` owns only their evaluation
  order. `semantic_analyze_landing_fixed_point` remains the last consumer and
  must reload and revalidate every selected landing.
- **Forbidden fallback:** no selected raw node is treated as source zero or as
  a current Clone; no global projection, cache, source exception, reduced
  input, weaker refusal or larger timeout is permitted.  A stage may be
  followed only when it actually emitted a deferred landing diagnostic;
  ordinary errors and silent failures stop the chain.
- **Verification / falsifier:** on the fixed map-payload owner input the final
  plan must remain exactly 172 stores and 651 copies, the executable hash and
  runtime result must match the adjacent baseline, all borrowed/retaining/
  stale/cycle negatives must keep their verdicts, and the official C and LLVM
  commands must complete inside their unchanged 120-second limits.  Any new
  or missing landing identity rejects the batching change.

### Result: rejected and removed

- Continuing every later receipt owner after any deferred landing reduced the
  fixed point to round 6, but changed the final plan from the adjacent
  baseline `172 stores / 651 copies` to `170 / 656`.
- The five extra copy identities were `9856`, `9862`, `9873`, `107002` and
  `127819`.  The last two reproduce the named-call and Option-result
  counterexamples from the earlier global selected-source projection.  Two
  baseline private stores were also not recovered; one extra store appeared
  early, for a net loss of two stores.
- Therefore a deferred diagnostic is not sufficient dependency evidence.
  Later receipt owners need the exact wrapper edge and result-input recipe
  that would become current, rather than permission to run from a failed
  predecessor.  The source candidate was removed.  The unchanged 120-second
  gate still completed only through round 4, with the baseline sequence
  preserved through that point.

## 2026-11-05: sealed body-call ordinal projection

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree; `direct_mir_owned_collection_execution_probe.pgy` on the
  unchanged nine-round landing sequence ending at 273 stores / 1762 copies.
- **Blocked gate / observed cost:** the official C probe exceeds its unchanged
  120-second limit.  A `-pg` run measured 831166 calls to
  `semantic_param_publication_body_actual_current`; its two linear searches of
  the already sealed `body_calls` owner consumed about 13 seconds of sampled
  self time.  The selected body edge itself does not change during that sealed
  publication generation.
- **Objective / priority:** retain exact call, caller and callee identity;
  project the exact body-edge ordinal once at the publication seal; remove the
  two repeated searches; then reduce the unchanged gate cost.  Semantic
  identity and fail-closed divergence take priority over speed.
- **Fact owner / last consumer:** `FunctionParamPublicationStore.body_calls`
  remains the body-edge owner.  The corresponding `PublishedNode` holds only
  its sealed ordinal projection.  `semantic_param_publication_body_actual_current`
  is the last consumer.
- **Forbidden fallback:** no inferred edge from call shape, no missing-edge
  default, no cross-generation reuse, no changed landing order or count, no
  reduced input and no larger timeout.
- **Verification / falsifier:** normal mode reads the ordinal only after the
  store is sealed. `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` repeats the former linear
  search and fails closed on a missing, duplicate or different edge.  The
  publication unit, full flow gate in verification mode, exact 273/1762
  landing sequence, and original 120-second C/LLVM commands must retain their
  verdicts.

## 2026-11-05: routine-wide completed backing-row replay experiment

- **Revision / fixed input:** the same local `60fd1e26d` dirty V1 tree and
  `direct_mir_owned_collection_execution_probe.pgy`; the body-edge projection
  preserved the exact 273-store / 1762-copy result but the full C command still
  took 232.33 seconds against the unchanged 120-second limit.
- **Observed repeated owner operation:** a `-pg` run reached
  `function_param_flow_backing_rows_capture_target` 469461 times.  Completed
  output replay supplied 318617 of those calls, once per formal, although the
  rows owner already owns a routine-wide shape and callee receipt.
- **Objective / priority:** validate all completed backing frames of one
  routine through that existing rows owner instead of reopening a separate
  callee walk for each formal.  Preserve exact frame identity, effect facets,
  result paths and no-store checks first; reduce repeated traversal second.
- **Fact owner / last consumer:** `FunctionParamFlowProgramPointIndex` and its
  existing `rows_shape` / `rows_memo` remain the only owner.  Completed
  `function_param_flow_output_replay` is the last consumer.  No new cache or
  parallel authority is introduced.
- **Forbidden fallback:** no skipped incomplete frame, no assumed callee
  currency, no cross-generation receipt, no weaker pending or unsupported
  verdict, no changed landing identity, and no larger timeout.
- **Verification / falsifier:** the flow fact unit and its currency-verification
  mode must retain all prior verdicts; any routine whose unrelated incomplete
  frame makes a formerly valid formal fail rejects this experiment.  The fixed
  probe must remain exactly 273/1762, and the official C/LLVM commands must
  finish under 120 seconds.

### Result: rejected and removed

- The verification unit added five failures in the existing nested-inout
  outward-effect positives.  A completed formal may be current while another
  frame in the same routine is intentionally not an admitted premise, so a
  routine-wide check is strictly stronger than the formal check it replaced.
- The source change was removed before running the large probe.  Formal-local
  currency remains the contract; a later optimization must preserve that exact
  target instead of widening its proof obligation.

## 2026-11-05: exact-formal backing currency receipt

- **Revision / fixed input:** the same local `60fd1e26d` dirty V1 tree and
  unchanged nine-round `direct_mir_owned_collection_execution_probe.pgy`.
  The rejected routine-wide replay is absent from the source.
- **Blocked gate / observed repetition:** the official C command remains over
  its 120-second budget.  The measured backing owner was entered 469461 times
  and enqueued 68.27 million targets; completed output replay supplied 318617
  formal-local roots.  Many of those roots reach a formal whose exact backing
  chain already passed earlier in the same sealed demand generation.
- **Objective / priority:** retain the formal-local contract and reuse only a
  successful receipt for the same routine identity, same formal ordinal and
  same demand epoch.  Semantic identity, fail-closed replay and the exact
  landing set take priority over reducing traversal.
- **Fact owner / last consumer:** the existing completed
  `FunctionParamFlowSummaryEntry` owns the exact routine/formal identity and
  stores only the epoch of its last successful backing replay.  The existing
  `function_param_flow_backing_rows_capture_target` remains the sole validator;
  `function_param_flow_output_replay` is the last consumer.
- **Forbidden fallback:** no routine-wide receipt, no receipt for a declared
  result or `ROWS_CAPTURE_ALL`, no reuse while solving, no cross-demand reuse,
  no reuse of incomplete or unsupported entries, no skipped failed traversal,
  no changed input, timeout or landing identity.
- **Verification / falsifier:** normal mode may reuse the exact formal receipt.
  `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` replays the complete old formal traversal
  on every reuse and fails the store closed on divergence.  Entry mutation
  sites clear the receipt beside the existing output receipt.  The focused
  fact unit, verification-mode flow gate, exact 273/1762 landing sequence and
  original 120-second C/LLVM commands must retain their verdicts.

## 2026-11-05: linear C Clone text assembly

- **Revision / fixed input:** the same local `60fd1e26d` dirty V1 tree and
  unchanged `direct_mir_owned_collection_execution_probe.pgy`.  The emitted C
  artifact is 35,139,865 bytes over 438,726 lines; its longest line is 72,787
  bytes and consists of one recursively deep aggregate Clone.
- **Blocked gate / observed repetition:** the unchanged full command took
  176.50 seconds, of which the backend owned 56.67 seconds.  The C Clone owner
  currently appends each copied field with `strdup_fmt("%s...", body, ...)`,
  copying the whole accumulated aggregate once again for every field.
- **Objective / priority:** preserve the exact Clone expression and deep-copy
  contract while assembling that expression with one owner-local growing
  buffer.  Byte identity of the emitted C takes priority over speed.
- **Fact owner / last consumer:** `transpiler_clone_value_emit.c` remains the
  sole C deep-Clone owner.  Its struct Clone emitter owns only text assembly;
  the ordinary C expression emitter remains the last consumer.
- **Forbidden fallback:** no shallow copy, no omitted field, no depth change,
  no new semantic cache, no changed compiler flags, input, timeout or landing
  identity, and no backend-specific admission rule.
- **Verification / falsifier:** the pre-change and post-change `--emit-c`
  artifacts must be byte-identical.  Existing Clone success and refusal tests,
  exact 273/1762 landing sequence, and the original 120-second C/LLVM commands
  must retain their verdicts.  Any byte difference rejects this optimization.

### Result: rejected and removed

- The post-change C artifact was byte-identical (`986a3762...`) to the
  pre-change 35,139,865-byte artifact, so the experiment preserved semantics.
- Wall time nevertheless increased from 152.60 to 156.50 seconds on the same
  ext4 input and executable path.  Aggregate text concatenation was therefore
  not the measured owner of the backend cost on this rung.
- The source change was removed.  The generated-C size and backend traversal
  remain open observations; neither justifies changing the timeout or output.

## 2026-11-05: inline stable-identity read at the AST owner boundary

- **Revision / fixed input:** the same local `60fd1e26d` dirty V1 tree and
  unchanged `direct_mir_owned_collection_execution_probe.pgy`.
- **Blocked gate / observed repetition:** the profiled semantic run called
  `ast_node_stable_id` 3,899,751,415 times.  The function owns no lookup or
  mutation: it returns zero for null and otherwise reads `ASTNode.stable_id`.
  Its out-of-line body alone held 7.01 sampled seconds; call overhead is paid
  by the owners that repeatedly validate sealed identities.
- **Objective / priority:** keep `ASTNode.stable_id` as the sole identity fact
  and expose the same null-safe read inline only where the complete AST layout
  is already visible.  Preserve the external accessor for API-only consumers
  and ABI compatibility.  Identity semantics take priority over speed.
- **Fact owner / last consumer:** `ASTNode.stable_id` in `ast.h` remains the
  owner.  `ast_node_stable_id` remains the public spelling and external ABI;
  existing semantic and compiler consumers remain unchanged.
- **Forbidden fallback:** no second identity field, no cached identity, no
  skipped stable-ID comparison, no layout change, no changed input, timeout or
  landing identity, and no consumer-specific answer.
- **Verification / falsifier:** compile both an `ast.h` consumer and an
  `ast_api.h`-only consumer, run parser/identity units, preserve the exact
  273/1762 landing sequence, and run the original 120-second C/LLVM commands.
  Any differing identity verdict or missing external symbol rejects the change.

### Result: rejected and removed

- Both header consumers compiled and the emitted C stayed byte-identical at
  `986a3762...`, but the same ext4 `--emit-c` run increased from 152.60 to
  159.56 seconds.
- The profiled call count was therefore instrumentation evidence, not an
  observed closure-blocking cost in the normal optimized executable.  The
  inline source change was removed; the public accessor remains unchanged.

## 2026-11-05: declaration-owned C deep-Clone helpers

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree; the unchanged nine-round input ends at 273 stores / 1762
  copies and emits 35,139,865 bytes of C.
- **Blocked gate / observed cost:** the CI-equivalent release compiler takes
  155.19 seconds for the official C command against its unchanged 120-second
  limit.  Its measured phases are 83.65 seconds semantic, 30.94 seconds C
  generation, 16.52 seconds compiling the emitted C, and about 3 seconds
  linking.  The emitted program expands the same declaration's recursive
  deep-Clone expression at every use, including one 72,787-byte line.
- **Objective / priority:** preserve the existing declaration and field
  metadata as the only Clone contract, but emit one file-local deep-Clone
  function per reached nominal declaration and call it at each use.  Exact
  value semantics, one evaluation of the argument, declaration-order field
  copies, and backend parity take priority over reducing C text and compile
  time.
- **Fact owner / last consumer:** `transpiler_clone_value_emit.c` remains the
  sole C deep-Clone owner.  The active MIR declaration field view supplies
  every field identity and type.  The ordinary expression emitter remains
  the last consumer; the helper registry owns only file-local C publication
  identity and never answers a semantic question.
- **Forbidden fallback:** no shallow copy, skipped field, AST rescan, generic
  instance guess, new program-shape route, changed landing set, input or
  timeout, and no helper call before the declaration's complete layout.
- **Verification / falsifier:** focused nested struct/String/Array Clone
  behavior must retain C/LLVM output and cleanup behavior; malformed or
  missing MIR field metadata must still fail closed.  The full emitted C must
  shrink while the exact 273/1762 landing sequence remains unchanged, and the
  original 120-second C/LLVM commands must pass with their positive and
  malformed controls.

### Result: rejected and removed

- A nested nominal `Array<Int>` Clone retained C/LLVM output (`9`, `7`), and
  the fixed full input retained the exact nine-round 273/1762 landing result.
- The emitted C shrank only from 35,139,865 to 34,852,110 bytes (0.8%).  The
  emit-only command still took 174.57 seconds on the measured run, including
  34.60 seconds in the backend, and compiling that artifact took 18.84
  seconds.  It therefore could not satisfy the unchanged 120-second command.
- The helper registry, publication buffer and calls were removed.  The active
  blocker remains the repeated owner operation inside the 80-second semantic
  phase; smaller C text is not accepted as V1 progress by itself.

## 2026-11-05: exact-formal receipt before queue admission

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree; the unchanged nine-round input ends at 273 stores / 1762
  copies.
- **Blocked gate / observed repetition:** the official C command remains over
  its unchanged 120-second limit.  The latest semantic profile attributes
  about 20 seconds to returned-backing work.  The backing owner still calls
  `rows_capture_enqueue` tens of millions of times, and a target whose exact
  routine/formal receipt is already current is discarded only after it has
  been admitted to and removed from the queue.
- **Objective / priority:** consult the existing exact-formal receipt before
  queue admission, while keeping the existing dequeue check as the sole
  validator for targets that were not already current.  Exact target identity
  and fail-closed replay take priority over queue reduction.
- **Fact owner / last consumer:** the completed
  `FunctionParamFlowSummaryEntry` and its `backing_rows_current_epoch` remain
  the only receipt owner.  `function_param_flow_backing_rows_capture_target`
  remains the last consumer; the queue owns no semantic fact.
- **Forbidden fallback:** no routine-wide receipt, no result/all-rows reuse,
  no reuse while solving or capturing, no cross-demand receipt, no changed
  landing, input or timeout, and no skipped verification replay.
- **Verification / falsifier:** normal mode may omit only a queue edge whose
  exact routine/formal receipt already equals the current demand.  With
  `PGY_VERIFY_FLOW_CURRENCY_MEMO=1`, every such edge is queued and the former
  full traversal runs.  The focused fact unit, exact 273/1762 sequence and
  official 120-second C/LLVM commands must retain their verdicts.

### Result: rejected and removed

- The fixed input retained the exact 273/1762 landing result, but the official
  C command still timed out at 120 seconds.  A complete measured run took
  171.78 seconds: semantic 98.98 seconds and backend 51.51 seconds.
- The 665 timed flow bodies still attributed 20.073 seconds to backing work,
  versus 20.001 seconds in the adjacent baseline.  Pre-queue receipt lookup
  therefore did not remove the repeated operation that owns the cost.
- The source shortcut was removed.  The existing dequeue-time exact-formal
  receipt remains the only accepted reuse path.

## 2026-11-05: sealed call-formal projection reuse

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree; the unchanged fixed input ends at 273 stores / 1762 copies.
- **Blocked gate / observed repetition:** the official C command remains over
  its unchanged 120-second budget.  The current profile enters
  `semantic_host_index_selected_call_formal` 74,092,056 times after the typed
  publication generation has already sealed the exact call, callee and formal
  identities.  Those replays repeat name lookup, nested-call lookup and
  stable-ID checks for an immutable generation.
- **Objective / priority:** reuse only the argument-to-formal row already
  replayed by the publication owner at seal time.  Semantic identity and
  fail-closed currency take priority over fewer calls.
- **Fact owner / last consumer:** `PublishedNode.arguments` remains the exact
  call-formal owner, and `FunctionParamPublicationStore.inputs_ready` plus the
  sealed call/routine receipts bound its lifetime.  The existing
  `semantic_param_publication_formal_for_actual` query remains the last
  consumer and publishes no second answer.
- **Forbidden fallback:** no unsealed or cross-generation reuse, no inferred
  ordinal, no skipped callee/routine receipt, no changed landing, input or
  timeout, and no new cache or compatibility read.
- **Verification / falsifier:** normal mode may read the sealed formal row.
  `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` must replay the complete former host-index
  derivation.  A differing ordinal or formal identity fails closed.  The
  publication fact unit, verification-mode flow gate, exact 273/1762 landing
  sequence and original 120-second C/LLVM commands must retain their verdicts.

### Current result

- The current full input preserved the exact nine-round 273-store / 1762-copy
  result.  Normal and verification mode reached the same first pre-existing
  positive-fixture refusal; the focused 48-row and 49-row units passed.
- A completed C run measured 165.312 seconds: semantic 84.443 seconds and
  backend 56.885 seconds.  The unchanged official command still timed out at
  120 seconds, so this projection reuse is not closure by itself.
- The adjacent profiled semantic/MIR run reduced
  `semantic_host_index_selected_call_formal` from 74,092,056 calls to
  23,082,542.  Its next measured repetitions are backing-row capture
  (15.47 seconds inclusive) and default aggregate move traversal
  (17.55 seconds inclusive); neither may be weakened to make the gate green.

## 2026-11-05: single-pass short C-buffer formatting

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree and the sealed call-formal candidate above.  The fixed input
  still ends at 273 stores / 1762 copies.
- **Blocked gate / observed repetition:** the C backend takes 56.885 seconds
  inside a 165.312-second official command.  `codebuf_write` currently invokes
  `vsnprintf` once to measure and again to emit every formatted fragment even
  when the already-owned buffer has enough capacity.
- **Objective / priority:** write a short formatted fragment once into the
  existing `CodeBuf` tail and repeat it only when growth is actually required.
  Exact C bytes and explicit allocation failure take priority over speed.
- **Fact owner / last consumer:** `CodeBuf` remains the sole emitted-byte and
  capacity owner.  `codebuf_write` remains the last formatting consumer; no
  semantic or backend dispatch fact is introduced.
- **Forbidden fallback:** no changed generated program, skipped formatter
  error, smaller input, larger timeout, alternate compiler flag or backend
  route, and no second output buffer authority.
- **Verification / falsifier:** formatted-buffer focused cases must preserve
  bytes across literal, short, growth and truncation boundaries.  The complete
  probe artifact must remain byte-equal, and the original 120-second C/LLVM
  commands retain their positive and negative verdicts.

### Result: rejected and removed

- The default four-case C/LLVM determinism gate passed, but the unchanged
  official C command still timed out at 120 seconds.
- The adjacent complete run took 166.982 seconds with a 57.321-second backend,
  versus 165.312 and 56.885 seconds before the change.  The difference is
  measurement noise in the wrong direction, not evidence that the duplicate
  formatting pass blocks closure.
- The source change was removed.  `CodeBuf` formatting is not the next V1
  optimization target.

## 2026-11-05: one collection-effect proof per immutable MIR generation

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V1 tree and the sealed call-formal candidate above.  The unchanged
  fixed input ends at 273 stores / 1762 copies.
- **Blocked gate / observed repetition:** the original C command still exceeds
  120 seconds.  A full emit-C profile enters
  `mir_validate_collection_effect_evidence` 9,905 times: once for the whole
  program, 4,952 times from per-routine ownership-transition validation and
  4,952 times from per-routine backend emission validation.  Those calls
  repeat one sealed snapshot proof and consume about 15 seconds in the
  profiled run, including 98,985,528 routine-inventory reads.
- **Objective / priority:** validate the complete collection-effect snapshot
  once for one immutable MIR generation, issue a receipt bound to that exact
  program/snapshot/routine inventory, and require the receipt at the local
  transition and emission consumers.  Safety identity and fail-closed
  verification take priority over call reduction.
- **Fact owner / last consumer:** the retained semantic
  `collection_effect_snapshot` remains the fact owner.
  `mir_validate_collection_effect_evidence` remains the only proof producer;
  MIR transition validation and the C/LLVM emission contracts are the last
  consumers.  The receipt carries proof currency and owns no second semantic
  answer.
- **Forbidden fallback:** no removed global validation, no receipt detached
  from its exact MIR pointers/count, no mutable-generation reuse, no skipped
  local transition or topology checks, no changed landing/input/timeout, and
  no backend-local reconstruction of collection facts.
- **Verification / falsifier:** malformed or cross-program receipts fail
  closed.  `PGY_VERIFY_MIR_COLLECTION_EVIDENCE_REUSE=1` replays the former
  per-routine evidence proof at every receipt use and fails on divergence.
  Focused malformed-evidence/transition/emission tests, the exact 273/1762
  sequence and the original 120-second C/LLVM commands must retain their
  verdicts.

## 2026-11-05: C declaration-header lookup projection

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree; the unchanged fixed input and 120-second C command.
- **Observed blocker:** the current whole-run profile enters
  `mir_find_decl_header_of_type` 2,220,036 times and spends 7.62 sampled
  self-seconds rescanning the validated declaration-header inventory.  The
  dominant callers are C-emitter declaration and host-header lookups.
- **Objective / owner:** MIR declaration headers remain the sole authority.
  One C-emission context may retain a non-owning name projection of those exact
  rows and reuse it only while `ctx->mir` is unchanged.
- **Last consumer / forbidden fallback:**
  `transpiler_active_decl_header_of_type` is the last consumer.  No AST answer,
  guessed declaration, cross-program row, changed input, weaker validation or
  larger timeout may replace a missing MIR header.
- **Verification / falsifier:** duplicate headers remain rejected by MIR
  validation; program changes clear the projection; missing names remain
  missing.  `PGY_VERIFY_C_DECL_HEADER_LOOKUP_CACHE=1` replays the former typed
  linear lookup and fails closed on a different pointer.  C bytes, the exact
  landing sequence and C/LLVM behavior gates must remain unchanged.

### Result: rejected and removed

- The focused C transpiler suite passed all 1,020 cases normally and with the
  former lookup replay enabled.  The fixed input also preserved the exact
  nine-round 273-store / 1762-copy landing result.
- The unchanged 120-second command still timed out.  A complete adjacent run
  took 163.571 seconds, including a 65.693-second C backend, compared with the
  preceding 150.560-second run and its 41.717-second backend.  The projection
  therefore did not remove the operation blocking V.001 and was removed.

## 2026-11-05: positive-only typed declaration projection

- **Observed cause:** line-level profiling confirmed about eight sampled
  seconds in the typed declaration-header linear scan.  The rejected cache
  also stored every missing spelling in a fixed 16K table; after saturation a
  miss replayed 16K probes, explaining its backend regression.
- **Objective / owner:** MIR declaration headers remain the sole authority.
  Build one positive-only `(AST kind, name) -> exact header pointer`
  projection from the immutable validated inventory for one C emission.
- **Last consumer / forbidden fallback:**
  `transpiler_active_decl_header_of_type` remains the last consumer.  Missing
  keys return missing immediately; there is no negative-row storage, AST
  fallback, guessed declaration, changed input, weaker validation or larger
  timeout.
- **Verification / falsifier:** duplicate header authority remains the MIR
  validator's error.  A changed inventory pointer/count rebuilds the
  projection.  `PGY_VERIFY_C_DECL_HEADER_LOOKUP_PROJECTION=1` replays every
  former typed linear lookup and a different pointer fails closed.  The 1,020
  C emitter cases, exact 273/1762 landing sequence, emitted bytes and original
  120-second C/LLVM commands must retain their verdicts.

## 2026-11-05: sealed claim declaration-binding projection

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree.  The fixed input, nine-round 273-store / 1762-copy result,
  and original 120-second C/LLVM commands do not change.
- **Blocked gate / observed repetition:** after the positive-only MIR header
  projection, the unchanged C command completes at the timeout boundary
  (121.064 seconds reported by the compiler timer).  Line profiling shows that
  each backing-row capture target scans the same sealed typed-claim vector to
  count declarations, scans it again to populate a temporary table, allocates
  that table, and later frees it.
- **Objective / priority:** derive the declaration-binding membership table
  once from one checked claim generation and reuse it only for that generation.
  Exact membership, explicit allocation failure and fail-closed currency take
  priority over reduced scans.
- **Fact owner / last consumer:** `FunctionParamFlowIndexClaims.rows` remains
  the only declaration fact owner.  Its checked generation owns a non-semantic
  membership projection; backing-row capture is the last consumer.
- **Forbidden fallback:** no unsealed, unchecked or cross-generation reuse;
  no use of the program-point local list as a substitute; no guessed binding,
  changed input, weaker claim check, smaller corpus or larger timeout.  A
  projection allocation or currency failure is a flow-store failure.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` replays the
  former complete claim scan for every membership query and fails on a
  different verdict.  Focused flow units, the full flow-summary gate, exact
  273/1762 landing sequence, and original 120-second C/LLVM commands must keep
  their verdicts.

### Result: rejected and removed

- The product build, all 1,020 C-emitter cases, and the complete flow-summary
  corpus passed in normal and forced-replay modes.  The exact nine-round
  273-store / 1762-copy semantic result was unchanged.
- The unchanged 120-second C command still timed out with status 124 and about
  1.38 GiB maximum RSS.  Its adjacent unlimited run completed in 164.223
  seconds (semantic 112.683, backend 35.495).  Runtime variation does not turn
  this into evidence that the projection unblocks the fixed gate.
- The source change was removed.  Declaration-binding membership is not the
  next V.001 optimization target.

## 2026-11-05: type-resolution graph identity projection consumption

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree.  The exact nine-round 273-store / 1762-copy input and the
  original 120-second C/LLVM commands remain fixed.
- **Blocked gate / observed repetition:** phase timing attributes roughly
  3–4 seconds per landing generation to the full second type-check pass.  The
  current profile enters `semantic_stage_should_defer_to_graph` 158,490 times;
  its consumer rescans every graph node although `TypeResolutionGraph` already
  owns an exact `(kind, site, label)` identity projection.
- **Objective / priority:** consume the graph owner's existing identity
  projection to select the exact type-reference node, then retain the existing
  reason-edge test.  Exact graph identity and fail-closed verification take
  priority over fewer node scans.
- **Fact owner / last consumer:** `TypeResolutionGraph.nodes` and `edges`
  remain the semantic owners; `node_identity_slots` remains their derived
  identity projection.  `semantic_stage_should_defer_to_graph` is the last
  consumer and publishes no second verdict.
- **Forbidden fallback:** no guessed node, name-only match, cross-graph row,
  changed edge reason, changed landing/input/timeout, or skipped graph error.
  If the optional projection is unavailable, the graph owner itself performs
  the former exact row lookup rather than inventing an answer.
- **Verification / falsifier:**
  `PGY_VERIFY_TYPE_RESOLUTION_STAGE_INDEX=1` replays the former complete
  node-and-edge scan for every query and emits a semantic error on divergence.
  Type-resolution DAG gates, the full semantic/transpiler suites, exact
  273/1762 landing sequence, and original 120-second C/LLVM commands must keep
  their verdicts.

### Node projection result and bounded continuation

- The node projection preserved the exact 273/1762 landing sequence and its
  forced replay reported no divergence.  The unchanged command improved from
  an adjacent 123.13-second timeout to 121.98 seconds, but still exited 124.
- The same last consumer still scans every graph edge after finding the exact
  node.  Extend this owner-local projection only with positive
  `(source-node, reason)` rows, invalidate it on graph mutation, and replay the
  complete former scan under the same verification switch.  No other semantic
  or performance track is opened.

### Edge projection result

- The positive `(source-node, reason)` projection preserved the exact
  273-store / 1762-copy sequence.  Forced replay completed with no projection
  divergence; the full semantic executable retained its 3227 passes and the
  same 19 failures already present in the shared dirty tree.
- The unchanged 120-second C command still exited 124 while the generated C
  translation unit was being compiled.  An adjacent complete run took
  131.484 seconds: semantic 91.937 seconds and backend 26.935 seconds, at
  1.38 GiB maximum RSS.  The graph projection is a measured reduction, not
  V.001 closure by itself.

## 2026-11-05: routine-owned move-path arena

- **Revision / fixed input:** the same local `60fd1e26d` dirty V.001 tree,
  fixed 273/1762 input, 120-second command and current graph/header
  projections above.
- **Blocked gate / observed repetition:** a current `-pg` MIR run attributes
  17.77 seconds to the ten default-aggregate move walks and reaches
  `move_walk_state_add` 696209 times.  Every path spelling currently gets its
  own `malloc`/`free`, including branch-state copies, although all path rows
  die together at the end of one routine.  `gprof` does not charge libc
  allocation time to this owner, so only the unchanged wall-clock gate may
  establish a benefit.
- **Objective / priority:** keep the exact move rows and comparisons, but put
  their copied path spellings in one existing `PgyArena` owned by the current
  routine walk and release it only after every branch state is retired.
  Lifetime correctness and identical move/copy decisions take priority over
  allocation count.
- **Fact owner / last consumer:** `MoveState` remains the sole ordered move and
  copy fact owner.  `walk_node` and its branch joins remain the last consumers;
  the arena owns bytes only and publishes no fact or lookup answer.
- **Forbidden fallback:** no cross-routine path reuse, no pointer-identity
  comparison, no changed traversal, landing, summary or timeout, and no
  retained arena after the routine state is freed.  Allocation failure remains
  fail-closed.
- **Verification / falsifier:** the focused default-aggregate positive and
  negative cases, the semantic suite, exact 273/1762 sequence and original
  120-second C/LLVM commands must keep their verdicts.  ASan must cover branch
  copies and state removal so a path cannot outlive its routine arena.

### Arena result: retained, but not V.001 closure

- Production `MoveState` rows now borrow path spellings from a routine-owned
  arena.  Unit-created states retain the old heap ownership, so the shared
  graph unit keeps its public construction contract.  Every row-removal path
  distinguishes those two byte owners; the state row array itself remains
  heap-owned in both cases.
- The graph unit passed 2670 checks normally and under ASan/UBSan with leak
  detection.  The aggregate-member inout bridge produced byte-identical
  22146981-byte MIR under normal and sanitized product binaries, with no
  sanitizer diagnostic.  The full flow-summary corpus passed normally and
  with `PGY_VERIFY_FLOW_CURRENCY_MEMO=1`.  The semantic executable remained
  at its existing dirty-tree baseline of 3227 passes / 19 failures.
- The exact fixed input retained all nine generations and the final
  273-store / 1762-copy set.  The unchanged 120-second C command still exited
  124 while the system C compiler was compiling the generated translation
  unit.  Its adjacent unlimited run completed in 126.450 seconds: semantic
  86.259 seconds, backend 26.647 seconds, and 1381128 KiB maximum RSS.
  Against the adjacent pre-arena result above, total time fell by about five
  seconds and semantic time by about 5.7 seconds, so the bounded lifetime
  change is retained.  It does not make the fixed gate green.
- The next bounded measurement must use this exact arena-enabled product and
  the same input.  It may optimize only a newly measured repeated owned
  operation; a warm file cache or a timeout-only pass is not sufficient
  closure evidence.

## 2026-11-05: sealed formal lookup projection reuse

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree and the retained move-path arena.  The exact nine-round
  273-store / 1762-copy input and the original 120-second C/LLVM commands stay
  fixed.
- **Blocked gate / observed repetition:** the unchanged C command still exits
  124.  A current non-PIE profile enters
  `function_param_flow_backing_rows_capture_target` 469383 times and charges
  it 9.65 seconds of self time.  Every reached target currently allocates and
  rebuilds the same formal stable-id, mode and hash-slot table even though the
  program-point index already owns the sealed routine identity.  Direct gdb
  observation also disproved the profile's unrelated cycle-format attribution,
  so that path is not an optimization target.
- **Objective / priority:** let the existing program-point index own one
  immutable formal lookup projection for its routine and reuse that exact view
  in backing-row capture.  The per-fact `seen` state remains local to each
  validation; semantic identity and forced full replay take priority over
  allocation reduction.
- **Fact owner / last consumer:** the function declaration's typed parameters
  remain the only formal identity and mode authority.  Their program-point
  index owns only the derived lookup layout; backing-row capture remains the
  last consumer and publishes no new verdict.
- **Forbidden fallback:** no declaration-binding cache (the measured rejected
  experiment above stays removed), no cross-index or cross-routine reuse, no
  inferred mode, no second semantic owner, and no changed input, landing,
  timeout or safety rule.  Missing, duplicate or changed parameter identities
  fail closed.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` replays every
  formal stable id, mode and slot against the live sealed declaration and
  fails the flow store on divergence.  Focused flow units, the full summary
  gate in normal and verification modes, exact 273/1762 landing sequence and
  the original 120-second C/LLVM commands must keep their verdicts.  If the
  unchanged gate does not improve materially, remove the projection.

### Result: rejected and removed

- The product built, and the full flow-summary corpus passed in normal and
  forced-replay modes.  The fixed input preserved all nine rounds and the
  final 273-store / 1762-copy result.
- The unchanged 120-second C command still exited 124 while compiling the
  emitted C.  Its adjacent unlimited run took 135.333 seconds by the compiler
  timer (semantic 91.200, backend 29.790) and 135.39 seconds wall clock at
  1381260 KiB maximum RSS.  That is not a material improvement over the
  retained arena baseline and does not unblock V.001.
- The formal projection and its index storage were removed.  The declaration
  projection remains removed as well; neither is the next optimization target.

## 2026-11-05: non-Windows emitted-C compiler preference

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree and retained move-path arena.  The nine-round input,
  273-store / 1762-copy result, generated program, 120-second command and
  positive/negative controls remain unchanged.
- **Blocked gate / measured operation:** the default Linux route selected GCC
  and exited 124 while compiling the generated C, at 1380720 KiB maximum RSS.
  The already-supported `PGY_CC=clang` route completed the same timeout command
  with status 0; its artifact returned `true` for the live control.  The
  toolchain owner's header already declares `PGY_CC -> clang -> gcc -> cc`,
  while its non-Windows candidate array implements `gcc -> clang -> cc`.
  Clang also emits 1714 non-fatal `-Wunused-value` warnings for generated
  assignment expressions whose result is intentionally discarded.
- **Objective / priority:** make the Linux implementation follow its existing
  toolchain contract, preserving `PGY_CC` and `CC` as explicit authorities and
  GCC/cc as fallbacks.  Suppress only the generated unused-expression warning
  already suppressed on Windows.  Backend behavior and fallback availability
  take priority over the measured compile-time margin.
- **Fact owner / last consumer:** `pgy_select_c_compiler` remains the sole C
  toolchain-selection owner; `compile_c_to_binary` and the LLVM/runtime link
  paths remain its consumers.  This changes no language, MIR, ownership or
  emitted-C fact.
- **Forbidden fallback:** no removed GCC support, no hidden required
  environment variable, no reduced input, increased timeout, weaker semantic
  check, warning-to-error relaxation, backend-specific program route or
  generated-program rewrite.
- **Verification / falsifier:** with no compiler override, the unchanged C
  command must finish under its existing timeout and its artifact must pass
  the live control.  An explicit `PGY_CC=gcc` build must remain available.
  LLVM and installed-driver C/LLVM checks must pass before this can contribute
  to V.001 closure; the exact landing sequence and safety gates must not move.

### Current C result and remaining LLVM blocker

- With no `PGY_CC` or `CC` override, the unchanged 120-second C command now
  exits 0 and its artifact returns `true` for the live control.  An explicit
  GCC build of `arith_grand_total` remains available and is runtime-equal to
  the default Clang build.
- The unchanged LLVM command still exits 124.  Its adjacent unlimited run
  completes in 129.602 seconds: semantic 88.341 seconds and backend 27.085
  seconds, at 1380828 KiB maximum RSS.  The artifact returns `true`.  V.001
  therefore remains open; compiler preference is retained as the C-gate fix,
  while the next bounded reduction must be in the shared semantic cost.

## 2026-11-05: current exact-formal root receipt fast path

- **Revision / fixed input:** the same local `60fd1e26d` dirty V.001 tree,
  retained move-path arena, exact nine-round input and original 120-second C
  and LLVM commands.
- **Blocked gate / measured operation:** LLVM takes 129.602 seconds, including
  88.341 seconds in semantic analysis.  The current profile enters
  `function_param_flow_backing_rows_capture_target` 469383 times; completed
  output replay is its dominant caller.  A root formal whose exact
  routine/formal receipt already equals the current demand epoch still
  allocates a queue, rediscovers the same completed entry at dequeue, skips
  traversal, rediscovers it again during receipt publication, then frees the
  queue.
- **Objective / priority:** consult the existing exact-formal receipt once
  before allocating the root queue.  Only a complete formal-local receipt for
  the same demand epoch may return immediately.  Full traversal and receipt
  publication remain unchanged for every other target.
- **Fact owner / last consumer:** `FunctionParamFlowSummaryEntry` and its
  `backing_rows_current_epoch` remain the only receipt owner;
  `function_param_flow_backing_rows_capture_target` remains the validator and
  last consumer.  No new table or semantic answer is introduced.
- **Forbidden fallback:** no routine-wide, declared-result, incomplete,
  solving, capture-callback, cross-demand or verification-mode shortcut; no
  changed input, landing, timeout, safety rule or queue behavior for a target
  without that exact current receipt.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` must retain
  the full former replay.  The focused fact unit and full flow-summary corpus
  must pass normally and in verification mode; the fixed program must remain
  exactly 273 stores / 1762 copies and keep C/LLVM runtime output.  The change
  is rejected if the unchanged LLVM command does not improve materially.

### Result: rejected and removed

- The 48-check fact unit and the full flow-summary corpus passed in normal and
  forced-replay modes.  The fixed input also retained the exact nine-round
  273-store / 1762-copy result.
- The unchanged LLVM command still exited 124.  A subsequent unlimited run was
  slower under system load rather than showing a stable semantic reduction.
  Since the required gate did not improve, the root shortcut was removed; the
  existing dequeue-time exact-formal receipt remains the only accepted path.

## 2026-11-05: function-local ResourceFlowUniverse seal boundary

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree, retained move-path arena, exact nine-round input and the
  unchanged 120-second C/LLVM commands.
- **Blocked gate / observed repetition:** the current profile enters
  `resource_flow_universe_capture_function_facts` 44568 times and attributes
  6.18 seconds to that call tree.  It reaches `type_is_resource_handle`
  227740500 times because every named function seal walks past its own
  `SCOPE_FUNCTION` into the program-global scope.  The same owner's branch
  snapshot already stops at the first non-lambda function frame.
- **Objective / priority:** seal only the current named function frame while
  retaining declaration identities already issued for destroyed inner blocks
  and lambda scopes.  Function-local fact identity and the existing implicit
  Main behavior take priority over scan reduction.
- **Fact owner / last consumer:** `ResourceFlowUniverse` remains the sole
  stable-index owner for one function analysis.  The HIR routine fact snapshot
  remains its last consumer; the scope boundary creates no new fact or cache.
- **Forbidden fallback:** no skipped current function symbols, no loss of
  inner-block or lambda identities, no global declaration copied into a named
  routine row, no new lookup table, and no changed input, landing sequence,
  timeout or safety rule.  Implicit Main keeps its root-scope behavior.
- **Verification / falsifier:** a resource-like global adjacent to a named
  function must not appear in that function's captured facts, while its
  function and lambda rows remain.  The resource-flow lifetime unit, full
  semantic/flow gates, exact 273/1762 landing sequence, and unchanged LLVM
  command must preserve their verdicts.  Remove the boundary change if the
  exact gate does not improve materially.

## 2026-11-05: sealed declaration projection revisit after scope repair

- **Revision / fixed input:** the same local `60fd1e26d` dirty V.001 tree
  after the function-local ResourceFlowUniverse boundary repair.  The input,
  nine-round 273/1762 result and 120-second LLVM command remain unchanged.
- **Blocked gate / observed repetition:** the exact LLVM command still exits
  124.  A fresh line profile attributes more than three seconds to rebuilding
  `RowsCaptureDeclarations`: each backing check scans the same already checked
  claim generation twice, allocates a temporary ID table, then frees it.  This
  is the largest measured source line group left inside the 11.88-second
  `function_param_flow_backing_rows_capture_target` self cost.
- **Objective / priority:** let the existing checked claim generation own one
  lazy declaration-ID projection and reuse it only while its generation is
  unchanged.  The claim rows remain the sole authority; exact currentness and
  full replay take priority over removing the repeated scans.
- **Fact owner / last consumer:** `FunctionParamFlowIndexClaims.rows` and its
  checked generation remain the owner.  Backing-row currency validation is the
  last consumer; the projection stores membership only and publishes no new
  ownership verdict.
- **Forbidden fallback:** no unsealed or cross-generation read, no guessed
  declaration, no source-shape exception, no changed landing/input/timeout or
  weaker refusal.  Allocation or generation mismatch fails the affected rows
  check rather than answering from stale storage.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` replays the
  former full claim scan for every projected lookup and fails closed on a
  mismatch.  Focused flow units, the complete flow-summary gate in normal and
  replay modes, exact 273/1762 landing sequence and the unchanged 120-second
  LLVM command must pass.  If that command remains red, remove this revisit.
- **Observed candidate result:** the 48-check rows-memo unit, the complete
  flow-summary gate and its currency/worklist replay mode all passed.  The
  unchanged fixed input retained all nine rounds and the exact final
  273-store/1762-copy result on C and LLVM.  Both `timeout 120` commands
  returned status 0 and both artifacts printed `true`; C reported 117.614
  seconds total and LLVM reported 126.045 seconds total.  The LLVM command's
  outer elapsed time was 126.13 seconds despite the status-0 timeout verdict,
  so record that timing difference rather than treating it as spare budget.

## 2026-11-06: one-capture backing-target membership projection

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree.  The fixed input is
  `collection_constructor_field_input_probe.pgy`; its current fixed point is
  17 rounds, 175 stores and 652 copies.  The committed 120-second C and LLVM
  commands, safety rules and input stay unchanged.
- **Blocked gate / observed repetition:** an unlimited C run takes 133.711
  seconds.  A current gprof build enters
  `function_param_flow_backing_rows_capture_target` 797804 times.  Its local
  queue receives 132309960 enqueue requests but visits only 23591404 distinct
  targets; the existing duplicate check linearly scans the queue on every
  request.  This is the measured repeated owned operation.  Loan-copy ID
  membership was separately rejected as a target after only 6911110 total
  comparisons.
- **Objective / priority:** keep queue order and every rows-currency replay
  unchanged, but let this one synchronous capture own an exact, lazy target
  membership projection.  Semantic identity and fail-closed verification take
  priority over elapsed time.
- **Fact owner / last consumer:** the `RowsCaptureTarget` queue remains the
  sole target identity owner.  `rows_capture_enqueue` is its only membership
  consumer; the projection stores queue ordinals and publishes no semantic
  answer.
- **Forbidden fallback:** no cross-call, cross-demand, cross-generation or
  solver-result cache; no skipped target, reordered queue, changed landing,
  larger timeout, smaller input or weaker safety rule.  Missing projection
  storage fails the affected rows check.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` replays the
  former linear membership decision for every indexed lookup and fails closed
  on divergence.  Focused flow gates in normal and replay modes, the exact
  17-round/175-store/652-copy trace, C/LLVM runtime parity and the original
  120-second commands must pass.  Remove the projection if the unchanged gate
  is not materially faster.

### Result: rejected and removed

- The focused rows unit passed all 48 checks and the candidate compiler built.
  On the unchanged C query, however, the projection took 134.46 seconds at
  898328 KiB maximum RSS, versus the adjacent 133.711-second baseline.  Most
  queues are small enough that lazy hash construction offsets the removed
  linear comparisons.
- The target projection and all temporary profiling counters were removed.
  No target membership cache, timeout change or semantic exception remains.

## 2026-11-06: exact-entry formal receipt consumption

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree.  The input, 17-round/175-store/652-copy result, 120-second
  commands and every safety rule stay unchanged.
- **Blocked gate / observed repetition:** the current O2 C query takes 133.711
  seconds.  A gprof run enters backing-row capture 797804 times; 522805 of
  those calls come from `function_param_flow_output_current`.  Completed
  formal receipts are already owned by the exact summary entry, but that
  consumer discards its entry index and makes the backing owner rediscover it,
  allocate a traversal queue and then observe the same demand-epoch receipt.
- **Objective / priority:** let the output consumer pass its already validated
  exact entry identity to the existing backing-row owner.  A current receipt
  may answer before queue allocation; a missing/stale receipt follows the
  unchanged full replay.  Semantic identity and replay verification take
  priority over elapsed time.
- **Fact owner / last consumer:** `FunctionParamFlowSummaryEntry` remains the
  owner of `backing_rows_current_epoch`.  The backing-row owner validates and
  consumes it; `function_param_flow_output_current` is the last caller that
  already holds the exact entry identity.
- **Forbidden fallback:** no new cache, cross-demand reuse, skipped full replay,
  guessed entry, changed input, timeout, landing or refusal.  Verification mode
  must bypass the shortcut and replay the full chain.
- **Verification / falsifier:** the 48-check rows-currency unit in normal and
  `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` modes, the exact query trace and C/LLVM
  parity must retain their verdicts.  Remove this path if the unchanged O2 C
  query does not gain material margin under 120 seconds.

### Result: rejected and removed

- The normal rows-currency unit passed all 48 checks.  Its direct verification
  invocation retains one pre-existing deliberate behind-owner mutation failure;
  the candidate itself bypassed the shortcut in verification mode.
- The unchanged O2 C query still timed out at 120 seconds and completed without
  the timeout in 145.47 seconds at 898724 KiB maximum RSS, worse than the
  adjacent 133.711-second baseline.  The exact-entry receipt path was removed;
  no new API or shortcut remains.

## 2026-11-06: dense publication-address read path

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree.  The fixed query, 17-round landing result, safety rules and
  120-second commands remain unchanged.
- **Blocked gate / observed repetition:** the O2 gprof run calls
  `publication_node` 1200995947 times and `publication_node_by_id` 356381223
  times.  Together they spend 10.17 sampled seconds, before the nested stable-ID
  accessor cost.  Both functions read the publication owner's already dense
  stable-identity array; only a missing row with `insert=true` needs growth or
  publication mutation.
- **Objective / priority:** keep the same publication owner and array, but make
  its read/current-row projection an inline owner seam.  Existing rows and all
  `insert=false` reads return directly; one named slow path alone grows or
  publishes a missing row.  Identity validation and fail-closed missing rows
  take priority over elapsed time.
- **Fact owner / last consumer:** `FunctionParamPublicationStore.nodes` remains
  the sole row owner.  `publication_node` and `publication_node_by_id` remain
  its private consumers; `publication_node_insert` is the only mutating path.
- **Forbidden fallback:** no second table, cached semantic answer, unchecked
  identity, cross-generation reuse, changed input, timeout or refusal.  A row
  whose stored identity or AST pointer differs still fails closed.
- **Verification / falsifier:** publication and rows-currency units, the full
  function-flow smoke in normal and replay modes, exact landing trace and
  C/LLVM query parity must be unchanged.  Remove the split if the unchanged O2
  C query does not gain material margin below 120 seconds.

### Result: rejected and removed

- The 48-check rows unit passed, but inspection showed that
  `publication_node` is also the current-identity replay boundary through
  `ast_node_stable_id`, not merely dense-array address arithmetic.  Directly
  reading `node->stable_id` would bypass that owner seam, so the candidate did
  not proceed to the fixed performance query.
- The exact current-tree publication gate reports 11 stale-row faults both
  before and after removal.  They are a separate dirty-tree regression, not
  evidence attributable to this candidate.  The inline path and its mutating
  split were removed; the single publication owner and original replay
  boundary remain.

## 2026-11-06: stable-identity accessor purity contract

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree, built as exact-overlay executable
  `7bffb6388df268376f494b29f3d71d35aaf6296d830da9aba3735aa88bd8a84d`.
  The input remains `collection_constructor_field_input_probe.pgy` and the
  committed C command keeps its 120-second limit.
- **Blocked gate / observed repetition:** after restoring the rejected
  experiments and repairing publication currency, the unchanged command exits
  124 at 120.06 seconds and 896280 KiB RSS.  The current profile attributes
  5.16 sampled seconds and 1882122417 calls to `ast_node_stable_id`; backing
  row validation is its largest measured caller family.
- **Objective / priority:** tell the C optimizer the existing stable-identity
  accessor is side-effect free while retaining that accessor as the only read
  seam.  Stable identity and stale-row failure take priority over call
  elimination.
- **Fact owner / last consumer:** `ASTNode.stable_id` remains owned by AST
  identity assignment and is still consumed through `ast_node_stable_id`.
  Publication and backing-row currency checks remain the last semantic
  consumers; this adds no identity projection or cache.
- **Forbidden fallback:** no direct field read at semantic call sites, inline
  duplicate accessor, assumed non-null node, skipped replay, changed input,
  larger timeout or weakened refusal.  The declaration may state only the
  accessor's actual read-only behavior.
- **Verification / falsifier:** stale node and generation mutations in the
  publication unit must still fail closed; the 13674-check publication gate,
  48-check currency gate and 49-check worklist gate must pass.  The original
  120-second C and LLVM commands must gain material margin and retain their
  exact runtime verdict.  Remove the attribute if they do not.

### Result: rejected and removed

- The 13674-check publication gate, 48-check currency gate and 49-check
  worklist gate passed, including their stale-identity mutations.
- The unchanged C query, pinned to a separate CPU while another single-core
  V.001 measurement was active, still exited 124 at 120.03 seconds and
  898504 KiB RSS.  That is no material improvement over the adjacent
  120.06-second, 896280-KiB baseline.
- The purity attribute was removed.  Stable identity remains the same
  out-of-line owner seam, with no direct field read or cached projection.

## 2026-11-06: complete-AST stable-identity inline projection

- **Revision / fixed input:** the same exact-overlay source and executable
  baseline as the rejected purity candidate.  A subsequent idle replay still
  exits 124 at 120.06 seconds and 895932 KiB RSS, so the committed budget has
  no usable margin.
- **Blocked gate / observed repetition:** the profile still assigns 5.16
  sampled seconds to 1882122417 calls of the trivial stable-identity accessor.
  The declaration-only purity contract did not remove that cross-translation-
  unit call boundary.
- **Objective / priority:** retain the public `ast_node_stable_id` symbol and
  source spelling, but let compiler-internal translation units that have the
  complete `ASTNode` definition consume one inline projection.  The external
  ABI wrapper delegates to that same implementation.  Identity currentness
  and one implementation take priority over inlining.
- **Fact owner / last consumer:** AST identity assignment still owns
  `ASTNode.stable_id`; `ast_node_stable_id` remains the named read seam.
  Publication and backing-row currency checks remain its last semantic
  consumers.  No consumer reads the field directly and no semantic fact is
  cached.
- **Forbidden fallback:** no second accessor body, double evaluation of a call
  expression, removed external symbol, direct semantic field access, changed
  input, larger timeout, skipped replay or weaker refusal.  Translation units
  that only know the opaque AST type keep the external function.
- **Verification / falsifier:** build every native translation unit, retain the
  external symbol, pass the publication/currency/worklist mutation gates and
  the fixed C/LLVM query with material margin.  A stable-ID mutation between
  reads must still invalidate the old row.  Remove the projection if any
  consumer fails to compile or the unchanged query does not improve.

### Result: rejected and removed

- The complete native compiler rebuilt, the external symbol remained, and the
  13674-check publication, 48-check currency and 49-check worklist gates all
  passed.
- The unchanged C query still exited 124 at 120.06 seconds and 898196 KiB RSS.
  Source-level inlining did not produce measurable margin on the O2 path.
- The internal projection and implementation guard were removed.  The public
  declaration and out-of-line implementation are restored exactly; no
  semantic consumer reads `stable_id` directly.

## 2026-11-06: whole-rows receipt subsumes a narrow rows query

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree and the validated MIR-row RSS repair.  The fixed
  `collection_constructor_field_input_probe.pgy`, 120-second commands and all
  ownership decisions stay unchanged.
- **Blocked gate / observed repetition:** the idle C command still exits 124
  at 120.05 seconds and 895888 KiB RSS.  A completed instrumented run records
  only about 1100 whole-row receipts but 662604 to 779699 valid reuses per
  analysis instance.  Exact-formal queries still reopen the narrower walk even
  when the existing settled whole-row receipt is current.
- **Objective / priority:** let the existing whole-row receipt answer the
  logically weaker formal/result query while retaining its callee closure and
  external-state replay.  Semantic currentness and fail-closed divergence take
  priority over elapsed time.
- **Fact owner / last consumer:** `FunctionParamFlowRowsMemo` remains the sole
  receipt owner.  `function_param_flow_backing_rows_capture_target` is its
  existing consumer and remains responsible for every reached callee.
- **Forbidden fallback:** no new receipt, cross-store/generation reuse, skipped
  callee or external-state check, changed input, timeout, landing, refusal or
  direct semantic answer.  A missing strong receipt follows the unchanged
  narrow full walk.
- **Verification / falsifier:** verification mode promotes every narrow reuse
  to a complete whole-row replay and compares its reached set.  The 48-check
  currency unit, 49-check worklist unit, complete flow gate, exact landing
  trace, C/LLVM runtime parity and both original 120-second commands must pass.
  Remove this use if the fixed command lacks material margin.

### Result: rejected and removed

- The focused rows, worklist, publication, allocation and module-snapshot
  units passed, but the unchanged C query then failed in 41.51 seconds with
  15005 ownership refusals instead of producing an artifact.
- A whole-row receipt is stronger as a verdict, but replaying its complete
  callee set for a narrow formal query couples unrelated paths and makes the
  consumer more conservative.  The narrow/full distinction is therefore part
  of this owner's observable admission contract.
- The reuse extension was removed.  Narrow queries again walk only their
  exact formal/result closure; no timeout or refusal was changed.

## 2026-11-06: one call-actual projection for overlap and move consumers

- **Revision / fixed input:** the restored `60fd1e26d` dirty V.001 tree with
  the MIR-row RSS repair.  The fixed constructor query, 17-round landing
  result, safety rules and 120-second C/LLVM commands remain unchanged.
- **Blocked gate / observed repetition:** the current profile attributes
  10.35 seconds to applying call moves and 2.90 seconds to overlapping-moving-
  argument checks.  Both consumers independently derive the same access path,
  aggregate value and exact body-call callee/formal row; overlap also derives
  every other actual again inside its pairwise loop.
- **Objective / priority:** project each call actual once from the sealed
  publication owner, then let the existing overlap and move rules consume that
  same call-local row.  Diagnostic identity and the two independent decisions
  take priority over fewer publication reads.
- **Fact owner / last consumer:** typed publication remains the sole value and
  body-call owner.  The projection is routine-walk scratch retired with
  `MoveWalk`; overlap and move application remain its only consumers.
- **Forbidden fallback:** no cached verdict, cross-call or cross-generation
  reuse, changed traversal order, omitted argument, weaker overlap/move rule,
  changed input, timeout or landing.  A missing current body-call row retains
  the same fail-closed diagnostic selected by the active pass.
- **Verification / falsifier:** focused move/overlap positives and negatives,
  the full flow-summary gates, exact landing trace, source C/LLVM parity and
  both original 120-second commands must preserve their results.  Remove the
  projection if the fixed query has no material margin.

### Result: rejected and removed

- The native compiler built, but the existing copy-graph unit itself remains
  unbuildable because its old `move_walk_state_add` call does not match the
  current owner signature; that pre-existing test problem is not a green
  receipt for this candidate.
- The unchanged C query still exited 124 at 120.05 seconds and 895860 KiB RSS.
  User CPU changed only from 112.12 to 111.85 seconds, which is not material
  margin and does not open the gate.
- The shared projection, buffer and combined dispatch were removed.  The two
  consumers again derive their own rows; no semantic or timeout change remains.

## 2026-11-06: intermediate landing observation before final publication

- **Revision / fixed input:** local `main` at `60fd1e26d` plus the preserved
  dirty V.001 tree and the validated MIR-row RSS repair.  The fixed input is
  `collection_constructor_field_input_probe.pgy`; the current complete run
  takes 17 landing rounds and finishes at 175 stores and 655 copies.  The
  committed 120-second C and LLVM commands, safety rules and input stay
  unchanged.
- **Blocked gate / observed repetition:** every growing landing round runs the
  full semantic publication tail even though the fixed-point owner consumes
  only the selected store/copy identities and immediately destroys the result.
  The final lifecycle, region, durable flow snapshot, collection-birth and
  collection-effect artifacts are required only from the stable round returned
  to the driver.  The measured round trace alternates store and copy growth,
  so weakening copy reset or retaining an unproved copy is forbidden.
- **Objective / priority:** let intermediate rounds stop after type checking and
  landing finalization, then reload the unchanged source and run the existing
  complete semantic analysis once after the landing sets stop growing.  Exact
  landing identity, final diagnostics and final published artifacts take
  priority over elapsed time.
- **Fact owner / last consumer:** the landing fixed-point owner remains the sole
  owner of cumulative store/copy plans.  Type checking and the formal-store
  finalizer remain their producers.  The complete semantic result returned to
  the driver remains the sole owner of lifecycle, region, flow and collection
  snapshots.
- **Forbidden fallback:** no retained copy across store growth, no skipped
  type/ownership check, no reuse of an intermediate AST or semantic result as
  the final artifact, no changed input, timeout, landing or refusal.  A final
  full analysis that selects a landing absent from the intermediate observation
  fails closed.
- **Verification / falsifier:** `PGY_VERIFY_LANDING_OBSERVATION=1` runs the
  former complete analysis beside every intermediate round and requires
  byte-for-byte equal selected landing identities/kinds.  Focused positive and
  negative landing gates, the exact 17-round final plans, C/LLVM runtime parity,
  bootstrap fixed point and the original 120-second commands must pass.  Remove
  the observation path if the fixed query lacks material margin.

### Result: rejected and removed

- The complete compiler rebuilt, but the unchanged C query still exited 124 at
  120.07 seconds and 893496 KiB RSS.  User CPU changed only from the adjacent
  112.12-second baseline to 111.99 seconds, which is not material margin.
- Durable semantic snapshot publication is not the blocking repeated operation;
  the repeated type/flow analysis that selects the next landings owns nearly all
  of the round cost.  The intermediate observation API and replay path were
  removed.  Every landing round again runs the original complete analysis.

## 2026-11-07: native compiler build-policy comparison

- **Revision / fixed input:** the restored exact-overlay source at local
  `main` `60fd1e26d`, whose ordinary O2 compiler hash is
  `7bffb6388df268376f494b29f3d71d35aaf6296d830da9aba3735aa88bd8a84d`.
  The input, C backend, 120-second command and ownership rules are unchanged.
- **Blocked gate / measured operation:** the ordinary GCC O2 binary exits 124
  at 120.05 seconds (112.12 seconds user CPU, 895888 KiB RSS).  This comparison
  asks whether native code generation policy, rather than another semantic
  owner change, can open a reproducible CI margin.
- **Objective / priority:** compare GCC O3/LTO and Clang O2 without changing
  source, input, timeout or diagnostics.  Runtime identity and a useful CI
  margin take priority over a small local speedup.
- **Fact owner / last consumer:** semantic and backend owners remain unchanged;
  only the private native compiler executable differs.  The committed build
  remains the last consumer if neither comparison clears the fixed gate.
- **Forbidden fallback:** no larger timeout, reduced input, skipped landing,
  changed safety rule or toolchain-only closure claim.  A candidate that merely
  approaches 120 seconds is rejected because it has no CI variance budget.
- **Verification / falsifier:** the unchanged query must exit 0, produce an
  executable and print the same observation.  Timeout or missing artifact
  rejects the build-policy candidate before broader gates.

### Result: rejected; no build-policy change

- GCC O3/LTO hash `390fe0b5...c7cf1` still exited 124 at 120.03 seconds
  (107.82 seconds user CPU, 898332 KiB RSS).  The roughly 4.3-second CPU saving
  did not open the gate.
- Clang 18 O2 hash `5a0b5886...33d8` also exited 124 at 120.03 seconds
  (112.54 seconds user CPU, 898768 KiB RSS).  It was not faster than the GCC O2
  baseline on this input.
- Both builds remain scratch-only.  The Makefile, compiler source and ordinary
  O2 executable are unchanged.

## 2026-11-07: current 17-round whole-run profile refresh

- **Revision / fixed input:** the same exact-overlay source, compiled with GCC
  O2 plus `-pg`; input and C command are unchanged, with a 300-second profiling
  ceiling only to collect evidence rather than to redefine acceptance.
- **Observed result:** the run completed in 239.94 seconds at 899908 KiB RSS,
  emitted the artifact and printed `expected source and observation mode`.
  The current profile enters backing-row capture 795358 times and enqueue
  169597741 times.  Backing-row capture owns 18.78 sampled self-seconds and its
  call tree remains the largest measured semantic cost.  The now-indexed
  `semantic_param_publication_body_actual_current` is only 0.02 self-seconds;
  the older five-round profile is no longer a valid basis for changing it.
- **Next falsifier:** attribute the current backing-row self cost to source
  lines before changing another receipt or projection.  Exact-formal and
  whole-row shortcut variants already failed either performance or admission
  and must not be repeated without new evidence.

## 2026-11-07: pending-relation owner receipt candidate

- **Revision / fixed input:** local `main` `60fd1e26d` plus the preserved dirty
  V.001 tree and validated MIR-row RSS repair.  The fixed constructor query,
  17-round landing result, ownership rules and 120-second C/LLVM commands are
  unchanged.
- **Blocked gate / measured repetition:** source-line profiling of the current
  complete run attributes about 3.8 sampled seconds to replaying each pending
  relation's membership by scanning every node in its backing fact.  The same
  relation-to-node ownership condition is reconstructed on every currency
  replay even though fact nodes are append-only and pending relations are
  discarded when a non-recursive frame is rebuilt.
- **Objective / priority:** record, when a pending relation is issued, the
  exact fact-local ordinal of the already-existing node that owns it.  Currency
  replay checks that ordinal and the unchanged direct-call or intent-action
  relation in constant time.  Semantic identity and fail-closed membership
  take priority over elapsed time.
- **Fact owner / last consumer:** `FunctionParamFlowBackingFact` remains the
  owner of its append-only node rows and pending relations.
  `function_param_flow_backing_rows_capture_target` remains the last currency
  consumer.  The relation receipt is retired with the relation; it is not a
  second graph or a cross-epoch cache.
- **Forbidden fallback:** do not create a missing owner node, infer ownership
  from syntax alone, retain a relation across a frame rebuild, weaken the
  intent-step check, change the input/timeout/landing result, or accept a
  relation that the former full node scan rejected.  Missing or stale owner
  identity fails closed.
- **Verification / falsifier:** verification mode replays the former full node
  scan and requires it to select the same ordinal.  Pending-relation positive
  and recursive negative gates, the 48-check currency unit, 49-check worklist
  unit, complete flow gate, exact 17-round landing trace, C/LLVM runtime parity
  and both original 120-second commands must preserve their results.  Remove
  the receipt if it changes admission or does not open material margin.

### Result: rejected and removed

- The candidate compiler built.  Landing allocation (107), module snapshot
  (43), rows currency (48), closure worklist (49) and publication fact phase
  (13674) all passed without a changed verdict.
- The unchanged C query still exited 124 at 120.06 seconds.  User CPU was
  112.28 seconds and peak RSS was 898164 KiB, versus the adjacent exact-source
  baseline of 112.12 seconds and 895888 KiB.  Removing the membership scan did
  not produce measurable margin; the sampled line cost was not the wall-time
  blocker in this build.
- The owner ordinal, altered relation signature and O(1) currency read were
  removed.  Pending-relation membership again uses the original full fact-node
  replay, and no semantic or build-policy change remains from this candidate.

## 2026-11-07: demand-local rows-memo closure receipt candidate

- **Revision / fixed input:** the restored exact-overlay source at local
  `main` `60fd1e26d`, ordinary O2 hash `7bffb638...a84d`, plus the preserved
  dirty V.001 tree and MIR-row RSS repair.  The fixed constructor query,
  17-round landing result, ownership rules and 120-second C/LLVM commands stay
  unchanged.
- **Blocked gate / measured repetition:** low-cost root telemetry over the
  complete query records about 18k--23k whole-row roots, 26k--46k exact-formal
  roots and 2k--5k declared-result roots per large analysis instance.  The
  existing whole-row memo is already reused about 662k--780k times per
  instance, but every reuse expands its callee closure through the queue again.
- **Objective / priority:** let the existing rows memo remember that its full
  transitive closure passed in the current demand and fixed-point pass.  Reuse
  that receipt only while `store->changed` remains false.  Exact semantics and
  prompt invalidation take priority over avoiding queue work.
- **Fact owner / last consumer:** `FunctionParamFlowRowsMemo` remains the only
  receipt owner.  `function_param_flow_backing_rows_capture_target` remains the
  last consumer and records a closure receipt only after its complete queue
  succeeds.  No exact-formal or alternate graph owner is introduced.
- **Forbidden fallback:** no reuse across demand or fixed-point pass, after any
  flow fact changed, after a failed or partial queue, with an unpassed memo, or
  under verification mode.  Do not change the input, timeout, landing result,
  diagnostic or ownership rule.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` always expands
  and replays the former full queue.  Unit mutations of demand, pass and
  `store->changed`, the 48-check currency unit, 49-check worklist unit, complete
  flow gate, exact landing trace, C/LLVM runtime parity and both original
  120-second commands must preserve their results.  Remove the receipt if the
  fixed query lacks material margin.

### Result: rejected and removed

- The candidate compiler built and the landing allocation (107), module
  snapshot (43), rows currency (48), closure worklist (49) and publication fact
  phase (13674) units all passed.
- The unchanged C query still exited 124 at 120.03 seconds.  User CPU was
  112.07 seconds and peak RSS was 898292 KiB, statistically unchanged from the
  adjacent 112.12-second / 895888-KiB baseline.
- The demand/pass receipt fields, closure-note vector and shortcut were
  removed.  The large memo-reuse count is not itself evidence of the blocking
  operation; the original queue expansion remains until a profile identifies a
  materially dominant owned operation.

## 2026-11-07: exact-root currency receipt candidate

- **Revision / fixed input:** the restored exact-overlay source at local
  `main` `60fd1e26d`, ordinary O2 hash `7bffb638...a84d`, plus the preserved
  dirty V.001 tree and MIR-row RSS repair.  Input, ownership rules, landing
  result and both 120-second commands remain fixed.
- **Blocked gate / measured repetition:** exact-formal roots outnumber
  whole-row roots in every large analysis instance (about 26k--46k versus
  18k--23k).  Caching a whole memo closure did not help because the expensive
  root proof itself remained.  A repeated exact root asks the same settled
  backing fact again inside one demand/fixed-point pass.
- **Objective / priority:** let that backing fact retain only its last
  successful exact-root currency verdict for the same demand, pass, solving
  state, fact epoch and typed-claim generation while no flow fact changed.
  Identity and immediate invalidation take priority over reuse.
- **Fact owner / last consumer:** the existing `FunctionParamFlowBackingFact`
  owns the receipt beside the exact formal/result rows it describes.
  `function_param_flow_backing_rows_capture_target` is its only producer and
  consumer.  No alternate dependency graph or whole-row authority is added.
- **Forbidden fallback:** no receipt for all-row or callback captures; no reuse
  across demand, pass, solving transition, fact epoch, claim generation,
  building/pending state or `store->changed`; no changed input, timeout,
  diagnostic, landing or safety rule.
- **Verification / falsifier:** `PGY_VERIFY_FLOW_CURRENCY_MEMO=1` always runs
  the original full queue and fails closed if a current exact-root receipt
  diverges.  Focused receipt invalidation mutations, currency/worklist units,
  complete flow gate, exact landing trace, C/LLVM parity and both original
  120-second commands must pass.  Remove the receipt without replacement if it
  does not open material margin.

### Result: rejected and removed

- The rows/worklist/landing/module units passed, but the 13674-check readonly
  publication gate reported 69 stale-identity mutation failures.  The receipt
  answered current after a transitive dependency identity had changed.
- Demand epoch, fixed-point pass, fact epoch, claim generation and
  `store->changed` do not form a complete currency proof for an exact backing
  root.  Reusing the whole result under those fields would weaken the existing
  fail-closed identity contract.
- The receipt fields and shortcut were removed before the large performance
  probe.  Do not revive an exact-root whole-result memo until the backing owner
  exposes a complete dependency identity that passes the stale-identity
  mutations; repeated root count alone is not sufficient evidence.

## 2026-11-07: move-walk formal-rule reuse candidate

- **Revision / fixed input:** restored exact-overlay O2 compiler hash
  `7bffb638...a84d` at local `main` `60fd1e26d`, the preserved dirty V.001
  tree and the unchanged constructor query.  The 120-second C/LLVM commands,
  17-round landing result and all ownership rules stay fixed.
- **Blocked gate / measured repetition:** a timing-only baseline completed 14
  rounds before exit 124.  Those rounds spent 35.118 seconds in flow
  preparation, 27.775 seconds in pass 2 and 20.470 seconds in aggregate
  landing.  The adjacent full profile attributes 32.76 seconds to 19 default
  aggregate move walks; 321142 repeated formal-move queries account for 6.27
  seconds, including 113290 complete-summary currency replays.
- **Objective / priority:** within one move walk, compute only the move verdict
  for an exact `(callee identity, formal ordinal)` once per settled flow
  generation and reuse only that result.  Lending, outward effects and handoff
  retain their original independent query order.  Semantic identity and
  immediate invalidation take priority over elapsed time.
- **Fact owner / last consumer:** the function-parameter flow summary remains
  the sole fact owner.  `semantic_default_aggregate_formal_moves` derives the
  verdict; `default_aggregate_call_move_owner.c` is its last consumer in the
  ordered walk.  The transient walk projection owns no cross-walk fact.
- **Forbidden fallback:** no reuse across flow-store pointer, demand epoch,
  entry count or fixed-point-pass changes; no reuse while solving; no skipped
  first currency proof; no changed landing order, input, timeout, diagnostic
  or ownership condition; no general cache architecture.
- **Verification / falsifier:** verification mode replays the original verdict on
  every reuse and fails closed on any difference.  The readonly publication,
  currency/worklist, landing and module units, complete flow gate, exact
  landing trace and C/LLVM parity must pass.  The original 120-second commands
  must gain material margin; otherwise remove the projection.
- **Known baseline red outside this candidate:** with the exact baseline hash
  restored, `array_result_owned_element_tuple_direct_landed_valid` fails MIR
  validation at `owned-evaluated-expression`.  The same failure occurred with
  the broader discarded prototype, so it is not evidence for or against this
  move-only reuse.  It remains an independent V.001 gate obligation.

### Result: rejected and removed

- Verification mode passed the 13674-check readonly publication unit, the
  48-check currency unit, the 49-check worklist unit, the 107-check landing
  allocation unit and the 43-check module snapshot unit.
- The unchanged C query still exited 124 at 120.05 seconds.  User CPU was
  112.04 seconds and peak RSS was 895856 KiB, indistinguishable from the
  adjacent 112.12-second / 895888-KiB baseline.
- The transient projection was removed.  The formal-move repetition in the
  whole profile is primarily repetition across landing generations, not a
  dominant duplicate inside one ordered move walk.  Another local cache would
  repeat this failed objective.

## 2026-11-07: V.001 evaluated-expression validation closure

- **Base and fixed input:** local `main` `60fd1e26d` plus the preserved dirty
  V.001 tree; restored native compiler SHA-256
  `7bffb6388df268376f494b29f3d71d35aaf6296d830da9aba3735aa88bd8a84d`.
  Raw/View and storage isolation remain the next milestone.
- **Observed complete set:** independently invoking all 608 registered positive
  flow fixtures found eight MIR validation failures in five evaluated shapes:
  tuple literal, channel send, event operation, lambda construction and
  spawn.  Five additional defer fixtures fail earlier in semantic analysis
  and are a separate obligation.  The eight MIR failures all stop at
  `owned-evaluated-expression`; the graph serializer does not represent those
  source shapes.
- **Objective / priority:** validate collection-release calls in the exact
  expressions an instruction evaluates without making persisted expression-
  graph completeness a prerequisite.  Semantic call identity and collection
  effect facts win over spelling; execution timing wins over AST containment
  (a lambda body is not evaluated when the lambda value is constructed).
- **Fact owner / last consumer:** the semantic call's collection-effect tuple
  owns release identity.  `mir_instruction_evaluation_roots` owns the evaluated
  roots.  `mir_collection_validate_owned_local_transitions` is the last
  consumer before MIR publication.
- **Forbidden fallback:** no unsupported expression accepted as opaque, no
  name-only builtin decision, no weakening of direct drop receipts, no new
  expression-wire vocabulary, no Raw/View or broader ownership precision.
- **Change set:** walk only evaluated expression children, reject a tracked
  owned release below the instruction root, keep the root release for the
  existing instruction-receipt validator, and fail closed on an unknown
  evaluated node kind.
- **Gates / falsifiers:** the eight named positives must reach MIR; all 608
  registered positives are re-enumerated; direct drop, double-drop and
  use-after-drop negatives remain refused; the full focused flow gate and
  collection-transition unit run before the unchanged constructor query is
  measured again.

## 2026-11-07: V.001 deferred-release fact projection closure

- **Base and fixed set:** the same local `main` and exact native compiler as
  the evaluated-expression closure above.  The preservation owner now accepts
  the five fixed positives only when a provisional deep-drop receipt still
  names the unique declared `Array<String>` result birth and that birth seals
  as the same owned result.  All five then reach HIR and fail at
  `terminal-disposition`: the semantic fact is retired, while defer execution
  has not yet been projected as a transition retirement.
- **Objective / priority:** preserve one fact chain from the semantic release
  receipt to HIR's exact pending binding, then to MIR retirement rows for the
  return blocks on which the defer registration must execute.  Registration
  is not a drop operation.  Runtime C and LLVM emission retain the existing
  LIFO defer topology and must consume the same registration/exit relation.
- **Fact owner / last consumer:** the semantic call's collection-effect tuple
  owns release identity.  HIR may postpone only the exact retired binding;
  MIR owns concrete terminal retirement rows using
  `mir_defer_registration_exit_relation`.  The ordered transition validator is
  the last admission consumer before C/LLVM use that same relation.
- **Forbidden fallback:** do not ignore calls after a `defer` declaration, do
  not exempt all calls inside cleanup bodies, do not infer safety from a
  callee name, and do not add Raw/View, storage-isolation or new syntax work.
  A missing, stale or ambiguous defer identity fails closed.
- **Complete change set:** keep the exact provisional-result preservation
  check; replace HIR's all-owned-literals deferral switch with an explicit
  pending-binding set; collect only a direct deferred deep drop with a current
  semantic receipt; issue a terminal MIR retirement only where the existing
  registration/exit relation is `1`; fail closed on conditional topology,
  duplicate retirement or stale identity.  Do not execute a defer body while
  walking its registration block.
- **Gates / falsifiers:** all five positives must reach MIR.  The existing
  direct double-drop, move-after-defer, retaining-call, constructor-return,
  LIFO drop-before-read, read-after-drop and mutable-reassign negatives must
  remain refused.  Re-enumerate all 608 positives, run the focused flow gate,
  then resume the unchanged V.001 canonical/bootstrap/installed-driver chain.

## 2026-11-07: V.001 dependent-result shallow-transition refusal

- **Observed gate:** after the defer slice, the full flow gate first stopped
  because an owned call result followed by ordinary `ArrayPush` or `ArraySet`
  reached MIR and was refused without a source location.  At the later deep
  drop its exact fact is `UNKNOWN`, still names the result source, and has
  `return_elements_owned == false`; the shallow transition has withdrawn the
  only whole-array deep-release premise.
- **Objective / owner / consumer:** the existing collection ownership fact
  owns that withdrawn premise.  `semantic_collection_admit_owned_string_drop`
  must refuse the dependent unknown result before publishing a drop receipt;
  MIR must not reconstruct the refusal as an orphan-receipt fallback.
- **Forbidden fallback:** no name-based permission, no promotion of UNKNOWN to
  OWNED, no removal of the shallow receipt, and no reduced negative location
  check.  General UNKNOWN without this exact dependent-source evidence is not
  changed by this slice.
- **Gate / falsifier:** both registered shallow-push and shallow-set/deep-drop
  negatives fail at their source receiver, while all 608 positives and the
  explicit defer positive/negative matrix retain their prior results.

## 2026-11-07: V.001 empty-local owned-store chain closure

- **Observed gate:** the focused flow gate reaches an empty local
  `Array<String>` whose first literal push and later member-path set are both
  shallow.  The member path is correctly copied at its earlier constructor
  boundary, but the set still lands as `Clone + ArraySet`; the receiver then
  has no uniform owned-element receipt for its synthesized cleanup.
- **Objective / priority:** keep the constructor copy, make the first push and
  later set one receiver-lifecycle decision, and let the existing owned store
  operations own their copies.  Exact call and binding identity precede copy
  count or replay-round count.
- **Fact owner / last consumer:** the formal String store landing owner owns
  `VALUE_COPY`, `OWNED_PUSH`, and `OWNED_SET` for each exact call.  The sealed
  collection-effect rows own the receiver-wide mutation chain.  The Array
  builtin checker is the last consumer that rewrites those plans to
  `ArrayPushOwnedString` and `ArraySetOwnedString`.
- **Forbidden fallback:** no extra source `Clone`, no empty-type-only
  promotion, no admission of an unselected shallow set, and no manual drop.
  A populated, aliased, ambiguous, or incompletely checked receiver remains
  refused.
- **Gate / falsifier:** the released local member-path fixture reaches MIR
  with one constructor `Clone`, one owned push, and one owned set, then runs
  equally on C and LLVM.  The direct released-local alias fixture remains
  refused; the full focused flow gate and 608-positive census follow.

## 2026-11-07: V.001 owned-String formal literal transfer

- **Observed gate:** the exact bootstrap reaches
  `CodegenTypeEnvStateOwned`, where `[local_rows]` transfers one `own String`
  formal into a local `Array<String>` and immediately deep-drops that array.
  Semantic admission allowed the operation through the old general-UNKNOWN
  boundary, but issued no collection-effect receipt, so MIR correctly failed
  at `owned-evaluated-expression`.
- **Objective / priority:** record that one exact value transfer at the array
  literal, consume the source formal once, and classify the destination with
  the existing owned-literal fact.  Existing deep-drop admission then owns
  retirement and its immutable call receipt; HIR/MIR remain consumers rather
  than reconstructing permission.
- **Fact owner / last consumer:** the array element-lifetime owner validates
  the typed String value, stable formal identity, `own` mode, non-borrowed
  payload and one-time consumption, then records an immutable receipt on the
  existing typed value publication row.  Cleanup replay validates that receipt
  instead of reopening the retired function scope.  The collection fact/effect
  producer consumes that premise, and the MIR owned-local transition validator
  is the last admission consumer.
- **Forbidden fallback:** no general UNKNOWN drop receipt, callee-name or
  routine-name exception, reused `own String`, implicit copy, source edit, or
  Raw/View and storage-isolation work.  A second literal store or later read
  of the transferred formal must fail at that use.
- **Gate / falsifier:** an exact owned-formal singleton reaches MIR and runs
  equally on C and LLVM; a second store of the same formal is rejected with
  the stable move-after-consumption diagnostic.  Then the assignment probe,
  focused flow gate, 608-positive census and unchanged bootstrap resume.

## 2026-11-07: V.001 disjoint member carrier read

- **Observed gate:** the exact bootstrap's nominal-array root control stops in
  `SelfMirRoutineAttachLastExpressionGraph`.  `build.cfg` is carried through a
  returning update, after which the same synchronous expression reads only the
  disjoint `build.local_names` and `build.local_types` fields.  Native admits
  the program; self-host records the active `cfg` field move as an opaque
  whole-root read hazard and refuses the sibling read at
  `unproved_formal_indexed_read_entry`.
- **Objective / priority:** keep the exact moved field unavailable until its
  restoration while permitting a synchronous readonly call over a distinct
  field of the same aggregate.  Exact root and field-path identity outrank the
  current whole-root shortcut; no type name or routine name grants permission.
- **Fact owner / last consumer:** the ordered member-transition owner already
  owns the first local root-hazard site; forwarded hazards remain opaque when
  closure crosses a function boundary.  Aggregate lineage and exclusive-value
  owners remain responsible for restoration, whole-root and overlapping path
  uses. `SemanticAstCollectionMemberIndexedReadReady` is the last consumer of
  that temporal fact before body-bundle publication.
- **Forbidden fallback:** no blanket member-read permission, no weakening of
  moved-field, whole-root, prefix, deferred, mutating or retaining-call
  refusals, no source carrier workaround, and no Raw/View or storage-isolation
  work.  A missing or ambiguous path remains refused.
- **Change set / order:** retain the root-wide hazard, but record its real local
  syntax site instead of replacing it with opaque site `0`; closure across a
  function boundary still publishes `0`. Add the nested disjoint-carrier
  positive and a missing-restoration negative to the focused member gate;
  preserve the same-field move, forwarded-move, whole-root/value-formal,
  mutating and deferred negatives; then rerun the nominal-array root control
  and the full bootstrap.
- **Gate / falsifier:** the nested carrier positive and the real MIR root
  control must pass on both C and LLVM observers.  The existing
  `member_indexed_read_moved_negative`, forwarded-move, root-boundary and
  mutator negatives must remain refused before the bootstrap resumes.
