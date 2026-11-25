# Populated Array<struct> literals on the general route (2026-11-25)

Source: main `f1c95ccd4` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers) and `/root/ts3` (gates). Drivers (test profile,
`-O0`): base `driver_rs2` (the `821aee530` source), literal only
`driver_ra2`, change `driver_ra4`. Slice 1 of the
[point 6 plan](../../agent_work_directives/review_improvements_and_sot_closure_2026-11-24.md).

## Before

- The general route admitted an `Array<struct>` local built from `[]` and
  `ArrayPush`, but refused a populated literal such as
  `[Point { x: 1 }, Point { x: 2 }]` at `left-edge`: its nested-literal
  readers knew only `String`, `Int` and `Bool` elements.
- The owned-local release plan had no mode for an `Array<Record>`, so no
  record array was ever released (`[]` plus two pushes leaked its 16-byte
  buffer; native frees it).

## Change

- `direct_mir_scalar_program_nested_array_literal_operand_owner.pgy`
  (renamed from the `Array<Int>` one; the `Array<Bool>` copy is deleted):
  one reader of a nested literal spine for any element type, and the choice
  of the literal kind: `Array<String>`, `Array<Int>`, `Array<Bool>`, or an
  array of an admitted logical record (new kind 160,
  `LogicalRecordArrayPopulatedLiteral`; the record Clone admission marker
  moves to 161, past the last kind).
- C and LLVM emit the literal through the record array's push helper (which
  aborts when allocation fails); LLVM's stack slot is fixed-size, so the
  entry-block owner hoists it out of loops. The helper and its declarations
  are emitted when a literal needs them.
- Release mode 9: an `Array<Record>` whose element record holds only `Int`,
  `Bool`, `Long` or `Float` fields frees its buffer, when the owned-local
  lifetime grants the release (an `[]` or populated literal kept in its
  routine). A record array with storage-owning elements still has no
  release; an index read of a record array borrows it.
- Gate `direct_mir_record_array_literal_owner.sh`.

## Results

- Sweep of 2,746 programs:
  - Default LLVM: 876 to 883 equal to native, 0 wrong. The seven new
    programs are populated `Array<struct>` literals (two codegen fixtures and
    five `function_param_flow_summary` programs).
  - Three `_invalid` programs native refuses now run on default LLVM; they
    already ran on default C with the same output.
  - Release mode 9 (`driver_ra4` against `driver_ra2`): no program changed
    status on either route.
  - Default C (MIR to AST round trip): unchanged, 1,418 equal.
- Every program default LLVM now runs that names an Array of a user type
  (19): direct C under ASan has no error; 16 print native's output and the
  other three are the native-refused programs above.
- Gate: PASS with `driver_ra4`. `driver_rs2` refuses the first fixture at
  `left-edge`; `driver_ra2` (the literal without mode 9) fails the release
  check, its push-helper buffers leaked.
- The 19 CI-run gates that reach direct MIR and the new gate PASS with
  `driver_ra4`. Codegen bootstrap: gen2 == gen3 (125,935 lines). Component
  contract, language word registry (the word inventory regenerated for the
  new fixtures), SoT authority edge and gate reachability PASS. The front end
  is unchanged.

## Not covered

- A record array handed to a by-value parameter, built by `ArrayPush`, or
  returned from a routine is not released (no ASan error; leaks the buffer).
- A `for` loop over an `Array<struct>` (`collection_range`), and the
  generic instance the three-routine program needs (slice 2).
