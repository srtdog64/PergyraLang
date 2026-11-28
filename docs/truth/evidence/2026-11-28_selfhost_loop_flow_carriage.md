# Self-host loop flags from the flow calculus; flow state marked not carried

Observed 2026-11-28 KST on `695a28193` plus this change. Trees: WSL
`Ubuntu-C-WSL` `/root/ts2` (bootstrap, sweep, census) and `/root/ts3`
(gates). Drivers (test profile, `-O0`): `driver_lf1` (SHA-256 prefix
`61fd8b67dcbc8714`) for the change, `driver_hd1` (`c2af12e497604436`) for
`695a28193`. The user chose this slice on 2026-11-28 when no open SoT row
could close quickly: stop the false claims first, leave the rows BRIDGE.

## What the self-host MIR claimed

A census compared every routine's loop and resource-flow rows of the native
oracle MIR (`--test-native-mir-json-oracle`) with the self-host MIR
(`--emit-mir-json-verified`) over the 2,746 sweep programs. On `driver_hd1`
704 programs differed:

- every self-host routine had `resource_flow_symbol_count` 0, where native
  tracks Array and other collection locals (890 routines);
- every self-host loop had flags 1 (FALLTHROUGH), effect 0 and no states. 41
  programs had a loop native marks 9 (it may return) or 0 (a `while true`
  whose only `break` is unreachable); others had native effect masks (NONDET
  4, IO 32) or loop states.

The only consumers, the general route's loop admission and range iteration
owner, required exactly that constant shape (flags 1, effect 0, no states),
so for self-host MIR they checked nothing.

## Change

- The body flow calculus gains `may_return` (native FLOW_RETURN: a reachable
  return, `Exit` or non-returning call, also through a loop that falls
  through). Its `returns` field keeps its must-return meaning; the body
  verdicts are unchanged (sweep below).
- `ast_loop_flow_flag_fact_owner.pgy` computes each loop's native result
  flags from the fixed termination facts (FALLTHROUGH unless a `while true`
  has no reachable break, or a literal range with iterations has a body that
  never reaches its next iteration; RETURN when the body may return). The
  body-type bundle carries the rows; the MIR producer reads them by loop node
  and fails verification on a loop without a row.
- The self-host producer writes `"flow_state_carried":false` on every
  routine; native writes `true`. mir_lower reads a routine without `true` as
  carrying no flow state and refuses a non-zero effect, a state row or a
  resource-flow row in it.
- Loop admission and range iteration accept flags 0, 1, 8 and 9 (the exits
  the CFG emitter runs); a transaction-failure exit stays refused.

## Results

- New `tests/self_hosted/parity/mir/mir_loop_flow_carriage_owner.sh` (self-host
  gates shard): loop rows of 6 routines equal native's (flags 9, 9, 9, 9, 1,
  9, and 8, 8 in the never-falling-through fixture); native carries a
  resource-flow symbol for `SumUntil`; C (ASan) and LLVM equal native. Four
  forged shapes are refused on both backends (effect placeholder, a resource
  row with the mark false or absent, a flags-41 exit); the unchanged and the
  carried-mark controls are admitted. PASS on `driver_lf1`, FAIL on
  `driver_hd1` (no mark).
- Census on `driver_lf1`: 815 programs equal (loop kind and flags, and for
  carried routines every field), 1 differing: the dead-code loop of
  `function_param_flow_summary/control_navigation_dead_ancestor_valid`, which
  native does not record and the self-host CFG keeps.
- Sweep of 2,746: every row equal to `driver_hd1` on both backends (C 1,419
  / 601 / 8 / 85, LLVM 983 / 1,045 / 0 / 25).
- Codegen bootstrap gen2 == gen3 (126,059 lines); native oracle C emission of
  the driver source rc 0 (432 s); SoT edge (145 derived carriers) and
  adequacy, gate reachability PASS; `self-host-mir-resource-flow-identity` and
  `self-host-mir-loop-flow-summary` make targets PASS. Of the 96 direct-MIR,
  ownership and loop gates run with `driver_lf1`, 33 fail, each also on
  `driver_hd1`.

## CI repair carried here

`695a28193` failed `build-linux-self-host-contracts` ([run 46117044994](https://github.com/srtdog64/PergyraLang/actions/runs/46117044994))
in `self-host-collection-inout-effect-test-smoke`. Run through make with the
gate's stop removed, two fixtures differ, on both backends, and nothing else:

- `inout_borrowed_loop_sibling_negative`: still refused at the inout call
  (14:9), now as `unproved_exclusive_sink_entry` (the argument shares storage
  with `sibling`) instead of `binding_use_after_move`. Native refuses the
  move itself (11:34).
- `exclusive_scalar_set_holder_positive` (a generality gap native admits):
  the column copied at its definition no longer reports a member use after a
  move (19:5); the String element pushed from the copy is refused first
  (18:5, `owned_element_escape`).

Both now expect `borrow_boundary_escape`. The change comes from the
definition-copy commit (`695a28193` and its parents): the `e6097ba08`
driver reports the old diagnostics, `driver_hd1` and `driver_lf1` the new
ones. The gate PASS through make with the moved expectations.
- The general route refuses two never-falling-through shapes on both
  drivers: a `while true` left only by `return` (terminal return) and a
  literal range whose body always returns (range CFG join).

Self-host effect masks, loop states and the resource-flow universe remain
uncomputed: `semantic.loop_flow_summary` and `semantic.resource_flow_universe`
stay BRIDGE.
