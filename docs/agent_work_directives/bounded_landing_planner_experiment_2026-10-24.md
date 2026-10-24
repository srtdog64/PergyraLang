# Bounded landing planner experiment (2026-10-24)

Status: PLAN, measurement 1 done (3382 candidates, 10 analyses). Owner: Claude lane. Raised by the
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

## Open decision for the user

Giving up completeness means refusing some sharing the canonical input
really has (for example the nine `valid` uses after
`SemanticAstExpressionSurfaceFactsFromTypedArtifact` returns a value
lending it and the caller writes through it). V1 asks for the unchanged
canonical program; admitting it then needs either a proof that does not
exist yet or an explicit `Clone` in its source.
