# General route: a range bound is the header's Int expression (2026-11-19)

Source: main `411649d31` plus this change. Tree: WSL `Ubuntu-C-WSL`
`/root/ts2`. Driver from `make self-host-compiler`: SHA-256
`6a5dc54e5404f3874b36d4bce833d8c2fd72dc4a6e85441141bb1bdca76c8d34`.
The comparison driver is the one in the
[collection iteration record](2026-11-19_general_route_collection_iteration.md)
(`8b3ad5d4...`).

## Before

The program route compared a range binding only with a literal or a local
bound.

- The range owner died on any other bound shape ("range bound graph is
  invalid"). This included a parameter bound without wire refs, such as
  `for i in 0..n` in a one-loop routine.
- An `ArrayLength(xs)` bound reached the header admission and was refused
  there (`range-condition`).

## Change

- A program-route range takes the MIR iteration fact's iterable type,
  `"Int"`.
- Its bound is the branch row's expression graph. The header admits that
  graph as any Int expression and compares the binding with it. The bound
  is evaluated again on every pass. Native does the same: a body that pushes
  extends `0..ArrayLength(xs)`, and a call bound runs once per pass plus once
  more.
- The program route's literal/local bound shape checks are deleted. A
  one-routine range keeps `""` and its old bound owner.
- A collection iteration is now the iterable types other than `""` and
  `"Int"` (`DirectMirScalarCfgRangeWalksCollection`).

## Results

| Check | Result |
|---|---|
| `direct_mir_general_route_collection_iteration_owner.sh`, two range programs added | PASS. 7 programs are equal on native C/LLVM, default LLVM and the general emitter's C. 2 forged MIR and 1 String iterable are refused. |
| Same gate on the baseline driver | FAIL |
| runtime helper, mutable declaration, general-emitter coverage, array parameter, component contract, SoT edge | PASS |
| Default-LLVM sweep against the collection-iteration driver | 12 moved from refused to equal and nothing else moved: equal 501 -> 513 |

The 12 programs are all under `tests/cases/`:

- `backend_compare/`: `cell_grid_total`, `for_factory_inline_chain`,
  `for_range_class_factory`, `for_range_factory_chain`,
  `for_range_match_parity`, `for_range_sum`, `for_token_classify`,
  `inline_class_compare_loop`, `item_chain_in_for`, `map_keys`,
  `quest_collect_for_range`;
- `function_param_flow_summary/`: `default_aggregate_move_field_alias_read_valid`.

## The one-routine guard (scratch experiment, not committed)

On the slice-1 driver, a scratch driver without the general route's
`single-program-shape` guard changed only 4 programs:

- `codegen/fixture/for_continue.pgy` and `backend_compare/for_range_explicit`
  moved from refused to equal;
- `codegen/fixture/array_max.pgy` (an `ArrayLength` bound, fixed here) and
  `codegen/fixture/for_each.pgy` (String elements) moved from equal to
  refused.

String elements are the remaining blocker for deleting the guard.
