# General route walks hoisted iterables; returned-Array routes deleted (2026-11-20)

Source: main `bfc4280e3` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2` (driver) and `/root/ts3` (gates). Driver built from the changed
tree with the existing gen2 codegen and the release C profile: SHA-256
`a935645e8b27c2624dc8103c281e87e205ba22d683d698766f061f5f55fa5cc2`. The
comparison driver is the `6ea9e53a8` build of the
[one-routine claim record](2026-11-20_general_route_one_routine_claim.md).

## Before

- The general route refused a `for` whose iterable the front end hoists into
  a `let` (a call result, an array literal, a record field) at the
  `collection_range` stage. Slice 1 of the
  [directive](../../agent_work_directives/general_route_runtime_foreach_2026-11-19.md)
  had kept that out of scope.
- Two per-program-shape routes sat ahead of the general route in the
  multi-routine dispatcher:
  - the returned-Array foreach route, a `Main` that walks the result of one
    literal `Array<Int>` producer with Int arithmetic;
  - the `Array<Int>` return route, which claimed every program with a routine
    returning `Array<Int>` and stopped most of them with "return program
    envelope is invalid".
- A scratch driver that logged the route of each program showed, on the
  1,398-program default-LLVM sweep, that the first route took 2 programs
  (equal) and the second 3 (1 equal, 2 refused).

## Change

- The collection range admission no longer refuses a hoisted iterable. The
  loop's `let` is walked like any local.
- With LocalRef wire rows (a routine with several loops), that `let` carries
  `iteration-synthetic:<loop>:0`. The LocalRef plan admits it only when
  `MirIterationTypeFactsHoistedIterableRef` finds an iteration type fact of
  that routine, for that loop, that says the iterable is hoisted. Any other
  non-`declaration:` row on a `let` is still refused.
- The returned-Array foreach and `Array<Int>` return routes are deleted:
  6 owner files and 4 functions only they reached. The registry rows
  `projection.direct_mir_array_return_program_identity` and
  `projection.direct_mir_array_return_plan` (both CLOSED) and their spine
  facts in `SoTAuthority.v` go with them. The one remaining function of
  `direct_mir_array_return_graph_fact_owner.pgy` is read only by the literal
  foreach receipt, so its carrier row moves to that row.
- Test changes:
  - `one_mir_array_return_projection.sh` checked the deleted route's C and
    LLVM text. It now runs the same program through the general emitter
    (4/3 on C and LLVM under ASan, the same artifact with the routine rows
    swapped) and requires all 13 of its forged MIR inputs to be refused with
    no artifact.
  - The collection-iteration gate gained two programs, a nested loop over
    call results and loops over an array literal and a record field, and two
    forged MIR inputs: a hoist fact switched off, and a synthetic row naming
    a loop the routine does not have. Both are refused.
  - The bool-logic gate lost its line-cap check of a deleted owner.
  - `one_mir_returned_array_foreach_projection.sh`, the deleted foreach
    route's gate, is deleted with it; no make target or CI job ran it. Its
    program and refusals are now the collection-iteration gate's
    `hoisted_calls` and forged hoist facts. `one_mir_cfg_break_case.sh` no
    longer calls it.

## Results

| Check | Result |
|---|---|
| Collection iteration (14 programs, 4 forged MIR), `Array<Int>` return (13 forged), array parameter, builtin rows, mutable declaration, coverage, runtime helper, component contract, SoT edge | PASS |
| Collection-iteration gate on the `6ea9e53a8` driver | FAIL: default LLVM does not compile `hoisted_literal_and_field` |
| `one_mir_bool_logic_projection.sh` | FAIL locally on this driver and on the `6ea9e53a8` and `bfc4280e3` drivers alike: all three emit the same MIR, whose hash differs from the gate's pinned value in this tree |
| `one_mir_scalar_cfg_foreach_array_int_projection.sh` (no make target or CI job) | FAIL on this driver and on the `bfc4280e3` driver alike: "lost owned foreach diagnostic for wrong-iterable" |
| `sot_registry_gate.py` | 92 authorities, 210 carriers; CLOSED=68 BRIDGE=22 ACTIVE=2 |
| Default-LLVM sweep against the `6ea9e53a8` driver | 4 moved from refused to equal and nothing else moved: equal 522 -> 526 |

The 4 programs, all under `tests/cases/`: `backend_compare/for_in_array_literal_iterable`,
`backend_compare/for_in_member_iterable`, `backend_compare/function_returning_array`
and `function_param_flow_summary/recursive_ref_read_only_walk_valid`.
The two programs of the deleted foreach route stay equal on the general
route.
