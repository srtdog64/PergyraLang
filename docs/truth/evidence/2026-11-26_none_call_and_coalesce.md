# `None()` and `??` on the general route (2026-11-26)

Source: main `bffa6821d` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (drivers, sweep) and `/root/ts3` (gates). Drivers (test
profile, `-O0`): base `driver_om2` (the `bffa6821d` source), change
`driver_co2`.

## Before

The `return_abi` cluster of default LLVM (29 programs) were all Option
programs that write the absence as a call, `return None();`. The return ABI
check knew only the spelling `None`, and the expression admission did not
map the call (the front end classifies it as a direct call to `None` with no
declaration, `call_target_syntax_id` 0), so `let x: Option<Int> = None();`
stopped at `graph-root-mapping` as well. Behind it, the general route had no
`??` at all (`expression-kind`).

## Change

- `direct_mir_scalar_program_builtin_call_owner.pgy`: a zero-argument call
  to `None` with no declaration is the absence of the expected Option (the
  kind `None` gets).
- `direct_mir_scalar_program_option_absence_return_abi_owner.pgy`: the
  return ABI check accepts the call spelling.
- `direct_mir_scalar_program_coalesce_expression_kind_owner.pgy` (kinds 162
  `CoalesceOptionInt` and 163 `CoalesceOptionBool`, classification, node
  readiness) and `direct_mir_scalar_program_coalesce_expression_owner.pgy`
  (C and LLVM): `option ?? fallback` over an `Option<Int>` or
  `Option<Bool>`. Native evaluates the option once and the fallback only
  when the option is absent. C calls a `static inline`
  `pgy_scalar_option_{int,bool}_coalesce(value, fallback)` helper from the
  Option preamble, LLVM selects between the payload and the fallback, so
  both evaluate the option once and the fallback eagerly; the fallback must
  therefore be pure (literals, locals, parameters, negation, `+`, `-`, `*`)
  and any other fallback is refused at node readiness. The record Clone
  admission marker moves to 164.
- `??` is dispatched with the match families (an Option selection); the C
  and LLVM match dispatchers take the right operand.
- Gate `direct_mir_option_coalesce_owner.sh` (self-host gates shard).

## Results

- Probes (`driver_co2`): the gate fixture prints `57 -2 9 11 2 true true`
  on C and LLVM, as native; `tests/cases/backend_compare/option_count_loop`
  prints `5 1 0 0` on both.
- Sweep of 2,746 programs, `driver_om2` -> `driver_co2` (equal / self
  refuses / other output / runs a native refusal): default LLVM
  952 / 1,076 / 0 / 25 -> 982 / 1,046 / 0 / 25; default C
  1,418 / 602 / 8 / 85, unchanged. The 30 newly equal programs run ASan
  clean and equal to native on direct C. Two `??` over `Option<String>`
  now stop at `expression-kind`.
- `direct_mir_option_coalesce_owner.sh` PASS on `driver_co2`; on
  `driver_om2` it fails at the fixture (`graph-root-mapping` at `None()`).
- Component contract, language word registry (inventory regenerated), SoT
  authority edge and adequacy and gate reachability (890 scripts) PASS.
- The 31 CI direct-MIR and self-host-gate scripts of the [Option match
  record](2026-11-26_option_record_match.md), the new gate and the two
  installed-pair gates PASS on `driver_co2` with `wslpath` hidden.
- Native oracle C emission of the driver source: rc 0 (418 s).
- Codegen bootstrap: gen2 == gen3 (125,935 lines).

## Not covered

- `??` over `Option<String>` or an Option of a record, and a fallback that
  calls a routine (`direct_mir_option_coalesce_call_refused.pgy`; native
  prints `fallback ran 9`).
- Native prints `Log(flag ?? true)` as `0` for a present `false`, while
  `let b: Bool = flag ?? true; Log(b)` prints `false`: native types the
  operand of `Log` differently from the bound value.
