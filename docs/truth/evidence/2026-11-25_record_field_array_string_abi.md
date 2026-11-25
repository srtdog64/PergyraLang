# Record Array<String> fields and the refused Array<String> fact (2026-11-25)

Source: main `0a0b43d7e` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Drivers (test profile, `-O0`): base `driver_rf1` (the
`0a0b43d7e` source), split only `driver_rs1`, change `driver_rs2`, and the
scratch falsifier `driver_rs2ns` (the record fill without the split). Follows
[record Array fields](2026-11-25_record_field_array_abi.md).

## Before

- The `Array<String>` ABI producer returned one empty fact for "the program
  has no `Array<String>` row" and for every refused capture (a malformed
  row, an unadmitted carriage, rows that disagree). Route admission read both
  as "absent".
- A record `Array<String>` field with no routine row was refused at
  `seal-record-collection-abi`, as `Array<Int>` and `Array<Bool>` fields were
  before `0a0b43d7e`.

## Change

- `direct_mir_scalar_program_array_string_abi_owner.pgy`: a refused capture
  returns `DirectMirScalarProgramArrayStringAbiFactRefused()` (digest -1),
  which is neither ready nor canonical empty, so admission refuses it at
  `array-string-abi`. Only "no row" returns the canonical empty fact.
- `direct_mir_scalar_program_record_field_array_abi_owner.pgy`: the
  `Array<String>` fact is filled from its canonical row like the Int and Bool
  facts, only when the producer's fact is canonical empty.
- Gate `direct_mir_record_field_array_abi_owner.sh`: two more fixtures (an
  `Array<String>` field alone with a getter lending one element, an
  `Array<String>` field next to an `Array<String>` local) and a forged
  `Array<String>` row on that local.

## Results

- Sweep of 2,746 programs on default LLVM:
  - Split only (`driver_rs1`): no program moved.
  - Change (`driver_rs2`): 855 to 876 equal to native, 0 wrong, nothing
    else moved. All 21 newly equal programs have an `Array<String>` record
    field.
  - Their direct C output under ASan: 21 clean and equal to native. The 27
    programs `0a0b43d7e` admitted are clean too; the four native refuses have
    no native output to compare and no ASan error.
- Gate: PASS with `driver_rs2`. `driver_rs2ns` admits the forged
  `Array<String>` row on C and LLVM (the hidden fallback the split removes)
  and fails the gate there. `driver_rf1` fails at the `Array<String>` field
  fixture.
- The 19 CI-run gates that reach direct MIR (push steps) PASS with
  `driver_rs2`. Codegen bootstrap: gen2 == gen3 (125,935 lines). Component
  contract (line caps) PASS. The front end is unchanged, so the DRV-2 MIR
  comparison of the record above stands.
- An `Array<String>` value parameter given a literal argument is refused by
  the general route at `array-string-boundary` with or without this change,
  so the gate uses a local instead.

## Not covered, and a correction

- The [record above](2026-11-25_record_field_array_abi.md) and `0a0b43d7e`
  say the field leak comes from MIR having no drop row for the record. That
  is wrong. The general route releases a record local's Array fields at
  routine exit (owned-local release mode 4), and a record kept in its routine
  does not leak them. The leak is a record local handed to a by-value
  parameter: the emitted caller has no release for it, and the callee, whose
  default-mode parameter shares the caller's storage, releases nothing.
  Measured on direct C with LeakSanitizer: a record read in place leaks only
  the `ToString` text (24 bytes, native 10); the same record passed to
  `Weighted(holder, 2)` leaks its field too (60 bytes). The caller's release
  would need the callee's borrow-or-move fact, which the one hand-off rule
  owns ([blocker](../state/blocker.md), item 3).
