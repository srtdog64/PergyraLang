# One-routine scalar CFG route and its collection families deleted (2026-11-20)

Source: main `5be0704a0` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2` (driver) and `/root/ts3` (gates). Driver built from the changed
tree with the existing gen2 codegen and the release C profile: SHA-256
`d3445a4e6f604181ef4f46ce0c918a2f84fcb77ad076028b81fc13c9b7eac97a`.
The comparison drivers are the `6ea9e53a8` build of the
[one-routine claim record](2026-11-20_general_route_one_routine_claim.md)
(sweep) and the `bfc4280e3` build (gate reruns).

## Before

The direct-MIR dispatcher tried, after the general route, a one-routine scalar
CFG route. Its graph input built literal foreach receipts and three
collection plans when the routine was not the general route's:

- `projection.direct_mir_string_array_push` (BRIDGE): an empty
  `Array<String>` with straight-line pushes;
- `projection.direct_mir_collection_pop_effect` (BRIDGE): bounded `ArrayPop`;
- `projection.direct_mir_array_int_program` (BRIDGE): Array<Int> sum, max,
  reverse, static set and dynamic push;
- `projection.direct_mir_scalar_cfg_foreach_receipt` (CLOSED): literal
  foreach receipts.

The scalar CFG emitters, readiness checks and graph identity carried these
plans for every program, where the general route always passed them empty.
A scratch driver that logged the route of each program found no program of
the 1,398-program default-LLVM sweep reaching the route or any of the four
plans; `str_array_push`, `array_pop`, `array_sum`, `array_max`,
`array_reverse` and `array_push` all ran on the general route.

## Change

- The dispatcher no longer tries the one-routine scalar CFG route. Its
  projection, graph admission and route-claim owners are deleted.
- The scalar CFG C and LLVM emitters keep only the program path.
- The graph input builds only range receipts: every loop is a range, a
  `for v in xs` a cursor walk. The range owner loses its one-routine bound
  shapes.
- The graph plan, the LocalRef plan, the value type plan, the loop-flow and
  local-inventory checks and the routine admission lose the foreach, String
  array, Array<Int> program, pop and collection-plan fields and parameters.
  A plan row with a kind only those plans produced (`ArraySetString`,
  `ArrayPushString`, `ArrayPopString`, `ArrayPopInt`, `LogArrayLength`,
  `LogArrayIndexConcatString`) fails the typed readiness check.
- Every function the edits left unreachable from the driver mains, and every
  file left with none, is deleted: 188 source files, about 16,000 lines.
- Registry: the four rows above and their spine facts in `SoTAuthority.v`
  are deleted. `projection.direct_mir_scalar_cfg_graph_plan` is now issued by
  `DirectMirScalarCfgProgramGraphPlanAdmission` and
  `projection.direct_mir_scalar_cfg_program_extension` by
  `DirectMirScalarCfgProgramExtensionFromPreparation`; the deleted files
  leave every consumer list and their carrier rows go. Census
  `CLOSED=67 BRIDGE=19 ACTIVE=2`, 88 authorities, 195 carriers.
- Tests:
  - The 23 gate files of the deleted rows go (the String array push, pop,
    Array<Int> loop push/sum/max/reverse, foreach, mixed foreach and indexed
    String array projections with their mutation and artifact scripts). No
    make target or CI job ran them, and all of them failed on the
    `bfc4280e3` driver too, since the general route has claimed their
    one-routine programs from `6ea9e53a8`.
  - New `direct_mir_general_route_collection_programs_owner.sh`, a push core
    step: the six programs on native C/LLVM, default LLVM and the general
    emitter's C, and that C under ASan. It also passes on the `6ea9e53a8`
    driver: it keeps the programs covered, it does not falsify the deletion.
  - One owner probe built a plan with the deleted fields and was updated;
    `one_mir_cfg_break_case.sh` no longer calls the deleted gates.

No program, gate input or forged MIR of the gates reached the deleted route,
so there is no input it answered that now fails; the deletion is the removal.

## Results

| Check | Result |
|---|---|
| Collection iteration (14 programs, 4 forged MIR), collection programs (6), builtin rows, mutable declaration, coverage, runtime helper, `Array<String>` cleanup, component contract, SoT edge | PASS |
| `tests/self_hosted/parity/one_mir/*.sh` | Every failure also fails on the `bfc4280e3` driver; none of these gates is run by a make target or CI job |
| `tests/self_hosted/parity/direct_mir/*.sh` | 52 PASS; each of the rest also fails on the `bfc4280e3` driver |
| Default-LLVM sweep against the `6ea9e53a8` driver | 4 refused -> equal (the hoisted iterables of `5be0704a0`) and nothing else moved: equal 522 -> 526, as before this change |
