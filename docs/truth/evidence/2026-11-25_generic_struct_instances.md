# Generic instances with struct and Array actuals on the general route (2026-11-25)

Source: main `2a5aeec99` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers) and `/root/ts3` (gates). Drivers (test profile,
`-O0`): base `driver_ra4` (the `2a5aeec99` source), change `driver_gi3`
(with the scratch route log of `route_log3.py`). Slice 2 of the
[point 6 plan](../../agent_work_directives/review_improvements_and_sot_closure_2026-11-24.md).

## Before

- A generic routine instance was admitted only when each substituted
  parameter and the result were scalar, `Option<Int>`, `Array<Int>` or an
  identity cell. A struct actual (`Wrap<T>` with `T = Point`) or an Array
  of one was refused at `routine-instance`, and the program went on to the
  three-routine route (`generic_member_record_array_return_flow.pgy`, the
  one corpus program that route ran) or was refused.
- A generic method call was refused at `direct-call` even when its instance
  was admitted: the call names the template (`ArrayWrapper_Wrap`), the
  instance's symbol is specialized (`ArrayWrapper_Wrap_Int`), and the member
  call admission compared the two spellings. The generic member fixtures
  with `Array<Int>` and `Option<Int>` results were refused there.

## Change

- `direct_mir_routine_instance_signature_owner.pgy`: a substituted
  parameter naming a declared value struct takes that declaration's MIR ABI
  row (the row a non-generic parameter of the struct carries); an Array of
  a declared struct carries no ABI row, as a non-generic `Array<struct>`
  parameter does. A struct or `Array<struct>` result is admitted. A struct
  whose declaration row is not admitted gives no substitution.
- `direct_mir_routine_instance_inventory_owner.pgy` carries the program's
  declared struct names (sealed in its digest);
  `direct_mir_routine_instance_type_owner.pgy` admits a struct or
  `Array<struct>` instruction substitution with no physical ABI row, as the
  same instruction of a non-generic routine has.
- `direct_mir_member_call_admission_owner.pgy`: a call to a generic
  method's instance is confirmed by its template name specialized with the
  instance's actual tuple.
- Gate `direct_mir_generic_struct_instance_owner.sh`.

## Results

- Sweep of 2,746 programs:
  - Default LLVM: 883 to 887 equal to native, 0 wrong, nothing else moved.
    The four new programs are generic member fixtures
    (`generic_member_array_return_flow`, `generic_member_constructed_return_flow`,
    `generic_member_inferred_flow`, `generic_vessel_member_inferred_flow`).
  - `generic_member_record_array_return_flow.pgy` stays equal; the route
    census now shows it on the general route.
  - Default C (MIR to AST round trip): unchanged, 1,418 equal.
- Route census (the scratch route log on default LLVM): the three-routine
  route now runs no corpus program. It receives 35 and refuses all of them
  (29 classified `declaration-free`, 6 `double-nominal`).
- Direct C under ASan for the five programs above: no error, equal to
  native.
- Gate: PASS with `driver_gi3`; `driver_ra4` refuses the first fixture
  (`direct MIR constructed member structural shape is invalid`, the
  three-routine route).
- With `driver_gi4` (the same source without the route log): the 19
  CI-run gates that reach direct MIR and the two new gates PASS. Codegen
  bootstrap: gen2 == gen3 (125,935 lines). The front end is unchanged.

## Not covered

- The three-routine route still exists (slice 3 deletes it).
