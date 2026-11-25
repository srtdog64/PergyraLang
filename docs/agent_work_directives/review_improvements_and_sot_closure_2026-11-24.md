# Review improvements and SoT closure (2026-11-24)

Status: ACTIVE. Base revision: main `0a9053745`. Owner: Claude lane (single
lane; no parallel implementation tracks).

The user asked to act on every valid point of an external review and then
close SoT rows ("전부 개선하고 SoT 닫자"). Each point was checked against the
repository before it was accepted; the checks are in the table. Keywords stay:
no word is removed, and the docs split them into a 1st tier (core) and a 2nd
tier (for when the domain grows).

## Objective card

- Objective: every accepted review point has an executable result or a
  recorded decision; required CI green on the commit that carries each step;
  then SoT rows closed with their gates.
- Priority: memory safety, one fact owner, fail-closed admission, backend
  parity, fixed point, exact-SHA CI, then the review's DX points.
- Forbidden: removing language words, larger time or memory limits, reduced
  inputs, skipped backends, weakened negatives, per-program-shape routes,
  closing a SoT row by dropping an obligation without a recorded decision.

## Points and order

| # | Review point | Checked fact | Step |
|---|---|---|---|
| 1 | CI near its limits | `build-linux` cancelled twice at 30 min on a 1.3x slower runner; contracts 17-26 of 30 min | `0a9053745` (release debug step reuses the `test-all` build) and a third Linux shard `build-linux-self-host-gates` (the contracts job reached 30.3 min on a 1.8x slower runner in `6a6f612b5`'s run). |
| 2 | Memory safety (found while measuring) | Default C ran 6 `inout_call_string_loan` programs native refuses, reading freed memory | Done in the review batch; three branch-separated `_valid` programs are refused too (loan analysis follows syntax order). |
| 3 | Too many words up front | 6 type-declaration words, 145 registry words | Done in the review batch: `language_word_tiers.tsv` + rendered page, checked by the registry gate; README declaration section. |
| 4 | Ownership inference is hard to predict | Compiler Clones are visible only in MIR JSON | Done in the review batch: `pgy --report-copies` (position, expression, place). The reason is not reported yet. |
| 5 | Native and self-host verdicts differ | Listed in `docs/truth/state/blocker.md` | Keep closing divergences (V1 ownership goal). |
| 6 | `direct_mir` size and the MIR to AST text round trip | 819 files, 75k lines; DRV-2 MIR JSON 372 MB | V1 goal 2. Route census at `58220e986` ([record](../truth/evidence/2026-11-25_record_field_array_abi.md)): the three-routine route (about 10.7k lines) runs one corpus program, a generic method returning `Array<Point>`; composite intent runs one, legacy and nested intent none. Order: record Array fields on the general route (Int, Bool and String done; the field release, S2, open), `Array<record>` values and non-scalar generic instances, then delete the three-routine route; then the intent plan; then the round trip. |
| 7 | 12.1 GiB DRV-2 peak | Margin came from removing waste; the driver still releases almost nothing | Measure the CI runner's peak first; then the 2026-11-16 last-use release decision. |
| 8 | Two front ends drift | True | Recorded: [decision](../truth/decisions/2026-11-24_native_pipeline_becomes_frozen_oracle_after_v1.md). |
| 9 | `intent/zone/world` barely dogfooded | `PgyCompilerWorld` is route-level `REACHABLE` | After V1: one real application; record as a plan, not now. |
| 10 | SoT rows | 19 open; none closes in one short step | `target.capability_profile` closed ([evidence](../truth/evidence/2026-11-24_target_capability_profile_closed.md), [scope decision](../truth/decisions/2026-11-24_target_capability_profile_closure_scope.md)); next row by the same survey. |

## Gates

Each step names its own falsifier in its evidence record. Every code step
runs the canonical DRV-2 producer, the 38 tool verdicts, the full-corpus sweep
(default C and LLVM against native) and the ownership gates the Makefile or
push steps reach, against the previous driver, before it is pushed.

## Point 6 plan: `Array<struct>` on the general route, then the three-routine route

Observed at `0a0b43d7e` plus the `Array<String>` field slice:

- The three-routine route runs one corpus program, a generic method
  returning `Array<Point>` (`Point` a struct). The general route refuses it
  at `routine-instance` (a generic instance's return ABI must be scalar,
  `Option<Int>` or `Array<Int>`), and refuses the non-generic form too: it
  has no `Array<struct>` value at all (`left-edge` on the literal).
- 45 programs native admits mention `Array<S>` for a struct `S` and are
  refused on default LLVM; 4 more for a class.
- The general route implements Arrays once per element type: `array_int`
  28 files, `array_bool` 18, `array_string` 51. A fourth copy for structs
  would be the per-shape duplication V1 goal 2 removes.

Slices, in order; each lands with its gate and CI green:

1. Element ABI owner: one fact per Array element type (scalar, `String`,
   struct) giving the element's C and LLVM types, size, alignment and
   release recipe, read from the existing Int/Bool/String ABI and the
   logical record layout. No behaviour change; the existing families read
   it where they spell an element type today.
2. `Array<struct>` locals in one routine through that owner: literal,
   index read (an element copy), member read of the element, `ArrayLength`,
   release at exit (the record's own release recipe per element). C and
   LLVM, runtime-equal to native, ASan clean.
3. `Array<struct>` parameters and results, with the hand-off rule's
   borrow/move fact for the caller's release.
4. Generic routine instances whose substituted parameter or result is an
   `Array<T>` or a struct, from the same element ABI owner.
5. Delete the three-routine route, its families and its seven `one_mir_*`
   gates; its program runs on the general route (falsifier: the route's
   dispatch is gone and the program still runs equal on C and LLVM).
