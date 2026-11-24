# Self-host element writes land the copies native lands (2026-11-24)

Source: main `db571be36` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` and `/root/ts3`. Native reference: `bin/pgy` built from
`cc27d42c6` (no native C source changed since). Drivers: base
`driver_s5c2r` (sha256 `ed53eff011ea937e`, release, main), change
`driver_a3b` (sha256 `2de5aa6ca46fff9a`, release).

## Before

Every corpus program (2,746: `tests/cases`, `examples`, the codegen and
mir_lower fixtures) on the base driver's default C and LLVM routes, against
native LLVM:

| Route | Equal | Native refuses | Self-host refuses | Other output |
|---|---:|---:|---:|---:|
| default C | 1,417 | 718 | 598 | 13 |
| default LLVM | 831 | 718 | 1,196 | 1 |

The earlier "0 wrong output" measured only the 1,398 programs that declare
a `var`, and only on LLVM. Native C prints what native LLVM prints for all
14 programs, so each difference is the self-host's:

- Four ownership programs (C only): an element write through storage a copy
  shares, with no copy (`local_copy_source_element_set`,
  `by_value_lend_result_written_argument_copied`,
  `default_aggregate_move_field_element_write`, `..._field_set`). LLVM
  refused them only at `seal-record-collection-abi`.
- `foreach_string_borrow/push_then_set_valid` (C and LLVM): a walked String
  pushed into another Array is read after the walked Array's owned set, and
  prints freed memory where native prints `5`.
- Five C programs print a `Float` through an Int conversion (`3.14` as `3`,
  `-3.5` as `-3`) or round in single precision (`float_arith_chain`,
  `float_to_string_precision`, `float_unary_minus`, `long_unary_minus`,
  `mixed_param_types`).
- `projection_chain_abi` and `party_role_bind` (C): a zone `refresh` chain
  and a party role call print 0.
- `branch_defer_skipped` (C): a `defer` inside an untaken branch runs.

## Cause of the four

46 probe programs, each on native and on default C and LLVM, split it:

1. An indexed assignment was no write. The statement facts track no
   assignment, so `SemanticAstCollectionExclusiveStatementRootReady` returned
   before its indexed-assignment branch. The same write spelled `ArraySet`
   ran as native does.
2. A store through a by-value formal's member path did not move the formal.
   The summary recorded only whole-formal sets, and the write-through rule
   asked that no other statement name the formal, which fits an alias, not
   a direct path. Native counts any store through the formal's access paths.
   An `inout` formal's member-path write was not recorded as a write either.
3. An `ArraySet` through a copy's path left the copy unfollowed, so no copy
   site was recorded, and an in-place write never checked the sharing an
   unfollowed copy leaves.

## Change

- `ast_collection_exclusive_statement_mutation_owner.pgy`: an indexed
  assignment takes the in-place write branch. An in-place write through a
  local is refused when an unfollowed copy of it made before the write is
  read later, or when its definition copied another local's storage
  unfollowed and that local is read later
  (`SemanticAstCollectionExclusiveInPlaceWriteUnfollowed`). A definition that
  is a call or constructor result (a `Slice` view among them), a formal's
  storage and a focus part keep their earlier admission. Growth is
  unchanged.
- `ast_collection_exclusive_formal_write_through_owner.pgy`: one rule for
  every in-place write (`SemanticAstCollectionExclusiveInPlaceWriteAt`), which
  the mutating-formal summary (`ast_collection_exclusive_value_growth_owner.pgy`)
  reads for call-form writes, `ArraySet` statements and indexed assignments. A member
  path of a by-value formal moves the formal; of an `inout` formal it records
  the formal as written. A by-value Array handed whole to an `inout` writer
  still shares
  ([OPEN](../decisions/2026-10-23_open_by_value_array_element_write_through_inout.md)),
  so `by_value_cursor_set_through_recursive_parser_valid` is unchanged.
- `ast_collection_exclusive_value_copy_owner.pgy`: an `ArraySet` statement
  through the holder or a path of it is a followed use, as an indexed
  assignment is.
- Direct MIR route admission names the failed seal condition
  (`stage=seal-record-collection-abi` and 11 others) instead of `stage=seal`.
- Gate `default_route_native_runtime_parity_owner.sh`: 3 more programs that
  must run as native on both default routes, 7 more that may be refused.
  `collection_inout_effect_owner.sh` expects the index form of a local set
  after a constructor took it to be admitted, as the `ArraySet` form is.

A first version refused every in-place write through a local whose
definition shares unfollowed storage, as a growth is refused. It refused
two valid programs (`backend_compare/slice_write_surface`, a write through
a `Slice` view, and `struct_clone_option_payload_copied_valid`, whose sharer
is dead), so the refusal now needs a later read of the sharer.

## Results

- Probes (46 programs, 92 self-host legs): 55 legs equal to native, 37
  refused, 0 other output. The base driver ran 32 of the first 43 with
  other output on default C and 9 on default LLVM; the last 3 were written
  after that run, and the driver with only causes 1 and 2 fixed ran all
  three with other output. Native admits 3 that are refused: a copy handed
  to a call inside a `Print` and then written by index (2), and a growth
  through a constructor's holder (refused before too).
- Sweep, 2,746 programs: default C 1,417 -> 1,421 equal, other output
  13 -> 9 (the four above), no other program moved; default LLVM
  unchanged (831 equal, 1 other output).
- Canonical DRV-2 producer (`driver_rung2_main.pgy`): exit 0 on both
  drivers, 192 s -> 187 s, peak 4.68 GB both. The MIR gains four compiler
  Clone sites, all in `SemanticAstBodyTerminationFactsFromFacts`, and their
  call hand-off rows: `partial` takes Clone of `nonreturning`,
  `function_nodes` and `ignored_reachability`, and `fixed` of `reachable`,
  because `nonreturning[row] = true` writes while `partial` is still read.
  Native MIR of the same input has exactly these four Clones.
- Tool programs (`src/self_hosted/tools/*/main.pgy`, 38): every verdict
  equals the base driver's; 6 that import that owner gain the same four
  sites.

## Gates

Run on main + this change with `driver_a3b` installed, before two
line-cap repairs and one gate expectation (below):

- The 75 CI make-step scripts and the push-step scripts (122 in all): 112
  PASS. Of the 10 failures, 8 failed the same way in slice 2's run; the
  component contract and `one_mir_string_case_math_projection.sh` stopped
  at a line cap (`direct_mir_scalar_program_route_fact_owner.pgy` 119/117,
  `ast_collection_exclusive_value_growth_owner.pgy` 299/249).
  `default_route_native_runtime_parity_owner.sh` PASS: 8 programs equal on
  both default routes, 16 equal or refused.
- Ownership gates the Makefile or push steps reach (45), the component
  contract, the semantic owner probe and the default-route gate: 39 PASS.
  7 fail on main the same way
  ([list](2026-11-21_selfhost_element_release_loan_refusal.md#ownership-gates));
  the other two are the line cap and `collection_inout_effect_owner.sh`.
  That gate expected `field_ctor_local_index_negative` (a local handed to a
  constructor, then written by index) to be refused, while it already
  expected the `ArraySet` form, `field_ctor_local_set_negative`, to be
  admitted with a Clone, as native admits both. The index form is now
  admitted the same way, and the gate expects that.
- Codegen bootstrap: gen2 == gen3 (125,660 lines).

The repairs: the new in-place write rule and the alias field reader moved
from the growth owner to `ast_collection_exclusive_formal_write_through_owner.pgy`,
whose subject they are, and two signatures of the route fact owner fit on
one line. Final tree, rebuilt release driver `driver_a3c` (sha256
`6350360a6bec6e70`):

- Canonical DRV-2 MIR byte-equal to `driver_a3b`'s (`1723f1e9601f577f`),
  181 s, peak 4.68 GB; the 46 probes print exactly what they printed on
  `driver_a3b`; sweep of 2,746 programs equal to `driver_a3b`'s on both
  routes.
- PASS: component contract, `one_mir_string_case_math_projection.sh`,
  `default_route_native_runtime_parity_owner.sh`, semantic owner probe
  build, `collection_inout_effect_owner.sh`,
  `shared_storage_copy_landing_owner.sh`,
  `selfhost_compiler_clone_carriage_owner.sh`,
  `collection_focus_recovery_owner.sh`.
- `default_route_native_runtime_parity_owner.sh` on the base driver fails
  (exit 1): default C prints `55` for `copy_then_index_set`, native `15`.

## Not covered

- The installed-driver and driver fixed-point jobs run in CI.
- A copy the analysis cannot follow (handed to a call in a statement that
  defines nothing) and then written: refused here, copied by native.
- The other nine programs above that run unlike native.
