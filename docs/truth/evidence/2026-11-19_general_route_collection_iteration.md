# General route: foreach over runtime arrays, slice 1 (2026-11-19)

Source: main `999b416c4` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler`:
`bin/pgy-self-driver` SHA-256
`8b3ad5d4660e26f3f168cfbfc0c0e6b4ae24338fceaa26bd85e6cda9e25e47f0`.
The baseline driver is the one in the
[runtime helper record](2026-11-19_runtime_helper_materialization.md)
(`f671f294...`, source of `f0d16adaf`). Plan:
[directive](../../agent_work_directives/general_route_runtime_foreach_2026-11-19.md).

## Before

The general route took no foreach:

- A parameter iterable has no SSA use on its MIR rows, only a `parameter:`
  wire ref, so the literal foreach admission died ("foreach facts are
  invalid"), or the range owner died on its non-Int type ("range flow/type
  identity is invalid").
- A local iterable produced literal foreach facts, and the routine input
  died ("external collection plan").

## Change

- A collection iteration is a range receipt with an iterable type, a cursor
  local and an element expression row. The program route builds it from
  the `loop-init`/`branch` pair; the literal foreach admission stays for the
  one-routine plan.
- The header condition is `cursor < ArrayLength(xs)`. The body entry binds
  `v = xs[cursor]`. Init and backedge effects step the cursor. Both rows
  re-admit the branch's iterable graph, so the walk reads the live
  collection, as native does: an `ArrayPush` in the body extends it.
- Admitted element types are Int and Bool. Long is not admitted: the route
  can neither build nor receive an `Array<Long>` yet. Any other element type
  is refused at the `collection_range` stage.
- Plan readiness counts the element row as a use of its expression row and
  checks it at the binding's type.
- The wire range-scope check now scopes only `iteration:` refs. It used to
  refuse every declaration or parameter ref in a routine with a range, for
  example `ArrayPush(xs, v)`.
- An `Array<Bool>` value parameter's LLVM copy-in took the `Array<String>`
  branch and died without a String array projection. It now uses the
  `Array<Bool>` ABI fact's alignment.

## Results

| Check | Result |
|---|---|
| New `direct_mir_general_route_collection_iteration_owner.sh` (push core step) | PASS. 5 programs are equal on native C/LLVM, default LLVM and the general emitter's C. Two forged loop-inits (binding, iterable) are refused on C and LLVM with no artifact. An `Array<String>` iterable is refused at `collection_range`. |
| Same gate on the baseline driver | FAIL: default LLVM dies with "range flow/type identity is invalid" |
| runtime helper, mutable declaration, general-emitter coverage, array parameter | PASS |
| `self_hosted_component_contract_smoke.sh` | PASS. No cap raised. Three new owners have new caps. |
| `sot_authority_edge_smoke.sh` | PASS: CLOSED=70 BRIDGE=22 ACTIVE=2 |
| Default-LLVM sweep, 1,398 programs | 11 moved from refused to equal and nothing else moved: equal 490 -> 501, refused 453 -> 442, compile-fail 438 and differs 7 unchanged |

The 11 programs are all under `tests/cases/`:

- `backend_compare/`: `array_cond_compound`, `for_in_double_option_match`,
  `for_in_match_classify`, `for_match_inner_if`, `for_v_match_v_shadow`,
  `game_round_dispatch`, `match_arms_with_local_for`,
  `match_case_for_loop_dispatch`, `parallel_arrays_loop`;
- `semantic_loop_flow/`: `loop_2`, `loop_3`.

## Not covered, found on the way

- **Gate legs.** The gate has no default C leg and no general-emitter LLVM
  leg.
  - The default C route picks the general emitter by its own rule: a
    collection ownership fact, or wire refs with every routine claimed.
    These programs fall to the MIR-to-AST round trip. On both drivers it
    refuses the parameter iterables and prints garbage for the gate's
    `live_local_push` (`1 2 <garbage> 3`).
  - The general emitter's LLVM leg links no runtime library, so it cannot
    resolve the Array index bounds-panic export.
- **`nested_for_in_match`** stops at a `match` temporary. Its wire ref is
  `match-synthetic:`, and the LocalRef plan requires `declaration:` for an
  `AST_LET_DECL`.
