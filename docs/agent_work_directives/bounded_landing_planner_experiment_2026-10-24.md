# Bounded landing planner experiment (2026-10-24)

Status: piece 1 landed as the chain walk (measurement 6): 4 analyses
instead of 10, same verdict, net 8 more copies. Owner: Claude lane. Raised by the
user on 2026-10-24 after the native checker reached 116 census refusals:
the current landing loop is not practical, and one bounded experiment
should decide whether a planner can replace it. Shape-specific admission
rules are paused until the experiment reports.

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
