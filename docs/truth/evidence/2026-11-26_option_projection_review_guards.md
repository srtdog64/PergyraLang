# Review guards on the Option projections (2026-11-26)

Source: main `d65096f11` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (driver, sweep) and `/root/ts3` (gates). Driver (test profile,
`-O0`): `driver_rv1`. Follows a review of
[Option<record> match](2026-11-26_option_record_match.md) and
[`None()` and `??`](2026-11-26_none_call_and_coalesce.md).

## Findings and change

- The `??` projections built their C call and LLVM IR from empty operands
  instead of refusing, unlike their neighbours. Both now claim the node and
  return an empty or invalid result for an empty operand, which the callers
  refuse.
- The LLVM Option<record> match did not check the payload's LLVM type
  before storing it. An empty type now refuses the node.
- `direct_mir_option_coalesce_owner.sh` had only a source-level refusal. It
  now also forges the MIR: the `Int` fallback of `MaybeIncr(9, 3) ?? 11`
  becomes a `Bool` literal with its graph digest resealed, and both backends
  refuse `Option<Int> ?? Bool` at `expression-kind`.
- A `case Some(p)` binding stores the payload before the test, also when the
  option is absent. That is unobservable while the payload record holds no
  storage, which the admission requires and the gate's `String` record case
  pins. It is left in place with a note: admitting a storage record must
  replace the store with the binding's ownership decision (borrow, move or
  copy), not just move it under the test.

## Results

- `direct_mir_option_coalesce_owner.sh` (with the forged case) and
  `direct_mir_option_record_match_owner.sh` PASS on `driver_rv1`; the forged
  case is refused at `expression-kind` on both backends.
- Sweep of 2,746 programs equals `driver_co2`'s on both routes (default
  LLVM 982 / 1,046 / 0 / 25, default C 1,418 / 602 / 8 / 85).
- Native oracle C emission of the driver source: rc 0 (452 s).
- Codegen bootstrap: gen2 == gen3 (125,935 lines).
