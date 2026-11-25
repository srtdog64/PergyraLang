# `match` over an Option of a record on the general route (2026-11-26)

Source: main `53c35e483` plus the [clone plan fix](2026-11-26_clone_plan_native_oracle_fix.md)
plus this change. Trees: WSL `Ubuntu-C-WSL` `/root/ts2` (drivers, sweep)
and `/root/ts3` (gates). Drivers (test profile, `-O0`): base `driver_cf1`
(the fix), change `driver_om2`.

## Before

The general route built `Some(Pair(x, y))`, `None`, `IsSome` and
`UnwrapOption` on an `Option` of a logical record, but refused a `match`
over one at `enum-match`: the match admission knew `Option<Int>`, `Int`,
payload-free and payload enums only. After the class field reads it was
the next default LLVM cluster (about 25 programs, mostly
`tests/cases/backend_compare/option_class_*`).

## Change

- `direct_mir_scalar_program_option_logical_record_match_owner.pgy`: a match
  case over a local or parameter of type `Option<R>`, `R` a logical record
  holding no storage, becomes one condition node (kind 161,
  `OptionLogicalRecordMatch`; the record Clone admission marker moves to
  162). `case Some(p)` stores the payload in the match-binding local `p`
  (its MIR binding type must be `R`) and is true when present; `case None`
  is true when absent. C reads the Option struct's `value` and `is_some`;
  LLVM extracts fields 1 and 0 of `{ i1, R }`.
- `direct_mir_scalar_program_option_logical_record_match_kind_owner.pgy`
  holds the kind and its node-shape readiness, apart from the admission, so
  the expression readiness owner imports it without an import cycle.
- `direct_mir_scalar_program_enum_exhaustive_match_owner.pgy`: a third guard
  shape for these conditions (variants Some and None, count 2, the same
  local or parameter), so a routine whose match returns in both arms needs
  no trailing return.
- Gate `direct_mir_option_record_match_owner.sh` (self-host gates shard):
  the fixture equal to native on C (ASan) and LLVM; a Some binding retyped
  to `Int` in forged MIR, and an Option of a record holding a `String`
  (`direct_mir_option_record_storage_match_refused.pgy`, native prints
  `4 -1`), refused by the general route.

## Results

- Probes (`driver_om2`): the fixture prints `12 0 300 36 -1` and
  `tests/cases/backend_compare/option_class_value/main.pgy` `12 0 0 300 0`
  on C and LLVM, as native.
- Sweep of 2,746 programs, `driver_cf1` -> `driver_om2` (equal / self
  refuses / other output / runs a native refusal): default LLVM
  937 / 1,091 / 0 / 25 -> 952 / 1,076 / 0 / 25; default C
  1,418 / 602 / 8 / 85, unchanged. The 15 newly equal programs run ASan
  clean and equal to native on direct C.
  `option_2level_class_nested` now stops at a terminal return instead.
- `direct_mir_option_record_match_owner.sh` PASS on `driver_om2`; on
  `driver_cf1` it fails at the fixture (`enum-match`). Both refusals come
  from the new owner (`enum-match`) on both backends.
- The 30 CI direct-MIR and self-host-gate scripts, the new gate and the two
  installed-pair gates PASS on `driver_om2` with `wslpath` hidden.
- Native oracle C emission of the driver source: rc 0 (450 s).
- Component contract, language word registry (inventory regenerated), SoT
  authority edge and adequacy and gate reachability (889 scripts) PASS.
- Codegen bootstrap: gen2 == gen3 (125,935 lines).

## Not covered

- An Option of a record holding storage (`String`, `Array` fields).
- A match over a member or call result rather than a local or parameter.
