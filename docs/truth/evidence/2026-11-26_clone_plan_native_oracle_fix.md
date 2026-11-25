# Clone plan refused by the native oracle, fixed (2026-11-26)

Source: main `53c35e483` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts3`. Fixes the [subject Clone change](2026-11-25_subject_clone.md)
(`fd2e097a7`).

## Failure

[CI 45640324474](https://github.com/srtdog64/PergyraLang/actions/runs/45640324474)
on `52396e0a7` failed `self-host-native-oracle-linux`: the native compiler,
emitting the self-host driver source to C (`driver_bootstrap.sh`, "oracle
driver C emission failed"), refused one function:

    [ERROR] 239:6 - Function parameter flow summary failed closed: required
    return backing relation is incomplete or stale

`build-linux-self-host-contracts` failed on the same function: two
`semantic-owner-probe` steps (push steps 3 and 4) compile owners of the
self-host compiler with the native compiler.

Line 239 of `direct_mir_scalar_program_logical_record_clone_owner.pgy` is
`DirectMirScalarProgramLogicalRecordClonePlanFor`. The subject Clone change
had made the recursive planner `...ClonePlanAppend` (all `inout` arrays)
take the copied type as a `String` and carry an `Array<String>` of step
types. The self-host front end, which built every local driver of that
change, admits it; the native one does not. The local checks of that change
did not run the native oracle emission.

## Change

- The plan carries each step's type as a record field index and a cell
  field index (`type_fields`, `cell_fields`), and the recursion takes the
  copied record row or cell row as `Int`s, as the record planner did before
  cells. `DirectMirScalarProgramCloneFieldTypeAt` reads a field type from a
  row.
- Reproduced locally (`bin/pgy src/self_hosted/compiler/driver/driver_bootstrap_main.pgy
  --native-pipeline --emit-c`, 127 s to the refusal). A first fix that kept
  the `String` parameter and only changed the step types to indexes was
  still refused at the same function (255:6); taking rows as `Int`s is what
  the native summary admits.

## Results

- Native oracle C emission of the driver source: rc 0 (482 s).
- Driver `driver_cf1` (this change on `53c35e483`): the subject Clone,
  compiler Clone carriage and receiver-field gates PASS; the sweep of 2,746
  programs equals `driver_vr1`'s on both routes (default LLVM
  937 / 1,091 / 0 / 25, default C 1,418 / 602 / 8 / 85).
- Codegen bootstrap: gen2 == gen3 (125,935 lines).
