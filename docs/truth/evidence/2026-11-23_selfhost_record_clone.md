# Self-host direct MIR copies records field by field (2026-11-23)

Source: main `f027156e6` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Plan: [directive](../../agent_work_directives/selfhost_compiler_clone_carriage_2026-11-22.md),
slice 2 (S5c of the [one hand-off rule decision](../decisions/2026-11-16_one_handoff_rule_for_both_front_ends.md)).

## Before

- Slice 1 ([record](2026-11-22_selfhost_compiler_clone_carriage.md))
  copied String and Array landings and refused a landing on a record with
  `clone-site-type`. In the 1,398-program sweep, 8 programs stopped there.
- An explicit `Clone` of a record had no direct-MIR signature. Three probe
  programs were refused at `stage=builtin-call` on direct C and LLVM: an Int
  and String record, a record of two Arrays, and a nested three-field
  record.

## Change

- Owner: `direct_mir_scalar_program_logical_record_clone_owner.pgy` plans
  the copy from the record's layout fact:
  - a member read for each field;
  - a String or Array Clone (kinds 151 and 144) for String, `Array<Int>`
    and `Array<String>` fields;
  - the value itself for Bool, Int, Long, `Option<Int>` and payload-free
    enum fields;
  - the same plan for a nested record, up to 16 levels;
  - a constructor over the copied fields.

  Any other field type (a map, an `Array<Bool>`, an Option of a record, an
  identity cell) has no copy, and the whole record is refused.
- Signature: `Clone(record)` resolves to kind 160, which only the admission
  reads. It is past the last expression kind, so an expression set that
  still holds it is invalid.
- Admission: a record copy (an explicit Clone, or a listed landing of record
  type) is appended once, as its planned nodes.
  - The source must be a place (a local, a parameter, or a member chain over
    one). The C emitter repeats member reads as text, so a call result is
    refused (`record-clone-source`).
  - A landing's constructor carries the site literal
    (`compiler-clone:<lane>:<node>`), and the program check finds it there
    as in slice 1. Record constructor readiness accepts that literal on a
    constructor and nowhere else.
  - A landing on a record with no copy stays `clone-site-type`. An explicit
    Clone of one is `record-clone-field`.
- The self-host checker shaped the owner. A recursive appender, or one
  called from two sites, that takes `inout types` was refused on the DRV-2
  source as `unproved_inout_copy_entry`. So the plan holds only integers,
  and one call site appends the type names, each of them the caller's
  record type or one of the fact's field types.
- Gate: `selfhost_compiler_clone_carriage_owner.sh` adds:
  - 4 landed record fixtures, a nested record landing and an explicit
    three-field record Clone, each run as native on direct C and LLVM under
    ASan;
  - two refusals on both backends: a record with a map field
    (`clone-site-type`) and an explicit Clone of a record call result
    (`record-clone-source`).
- Caps: admission owner 589 -> 620, record expression readiness 122 -> 126.

## Results

Driver: release build of this tree. Previous driver: the `f027156e6` release
build.

- Extended gate: PASS. The previous driver fails it at the first record
  program (`clone-site-type`).
- Direct C and LLVM under ASan with leak detection off: 6 landed record
  fixtures, the nested landing and the three explicit record Clones print
  native's output with no memory error.
- LSan: the copies leak, as explicit Clones do.
  `default_aggregate_move_reuse_landed_valid` (two landings) leaks C 212
  and LLVM 116 bytes, which is what its explicit-Clone version leaks.
- Default-route LLVM sweep of the 1,398 programs:
  - equal 535 -> 542: 7 record landing fixtures in
    `function_param_flow_summary`; no program left equal; 0 wrong output;
  - native refuses 295; non-equal 568 -> 561;
  - first-error clusters 119 (11 = 50%, 33 = 80%).
- Candidate gates (307 MIR/concat related): 147 fail, and all 147 failed in
  slice 1's run. One gate changed, to PASS: the cleanup gate, which slice 1's
  run used before its update.
- The 31 scripts the CI push step runs directly: all PASS.
- The 75 scripts behind the CI push make steps: 67 PASS. The 8 that fail
  also failed in slice 1's run (a missing Rocq, a separately built
  fact-phase unit, the shared MIR lowering tool build and the three smokes
  that need it, `one_mir_struct_argument_projection.sh`,
  `identity_cell_receiver_execution.py`). The two gates slice 1 updated now
  PASS; slice 1's run used them before the update.
- SoT registry gate PASS (CLOSED=62 BRIDGE=17 ACTIVE=2); gate reachability:
  889 scripts, 0 undeclared, 0 stale.
- Codegen bootstrap: PASS, gen2 == gen3 (125,506 lines of C).
- `direct_mir/`: 818 -> 819 files, 75,096 -> 75,426 lines.

## Open

- The `clone` row of a landed initializer (`member_alias`) is still refused
  at the transition admission. The directive explains why restating the row
  is not enough.
- `default_aggregate_move_ref_param_invalid.pgy` moves a `ref` parameter
  into a callee that grows it.
  - Native refuses it.
  - The self-host front end lands a Clone at that call. Now that direct MIR
    copies records, direct C and LLVM run it and print `3`, ASan clean.
    Slice 1 refused it at `clone-site-type`.
  - So the two front ends give different verdicts, and the one hand-off
    rule decision asks for one rule.
- Map copies: slice 3.
- Scheduled `Platform full` run 45068771482 on `f027156e6` failed the same 5
  jobs as run 43914895554 on `3ad7444db` (macOS C-only, both driver parity
  shards, `platform-full-linux`, `platform-full-windows`).
