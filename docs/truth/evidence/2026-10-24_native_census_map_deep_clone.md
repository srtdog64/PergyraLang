# 2026-10-24: map deep Clone on C and LLVM, and the canonical census after it

Recorded by the Claude lane on branch `claude/p0-native-checker` at
`7ecff78c2` plus the change measured (local, not pushed). Rule:
[decision](../decisions/2026-10-24_hashmap_clone_is_deep.md); contract:
[request](../../agent_work_directives/hashmap_deep_clone_backend_2026-10-22.md).

## Runtime

C: `pgy_map_clone_<value>` for the macro maps, `pgy_map_clone_int` and
`pgy_map_clone_string`, all through one slot copy
(`pgy_hashmap_clone_slots`). LLVM: `pgy_map_clone_raw_export` over the raw
layout. The Clone emitters (`transpiler_clone_map`, `llvm_clone_map`) call
them for a String, Int, Long or Bool key and a String or scalar value, and
copy any other map by value.

`tests/cases/backend_compare/map_clone_deep` prints
`1 2 1 false 39 1 2 one two true 11 30 false 40 1 2` on C and LLVM (the
request's falsifiers plus Int/Long and Bool keys); ASan and UBSan are clean
on C. Before the change the first line was `2 2` and growing the copy was a
heap-use-after-free.

## Canonical census

| Run | Final | Outside `direct_mir_*` |
|---|---|---|
| `7ecff78c2` (tp2) | 168 | 126 |
| map deep Clone (mc1) | 144 | 102 |

Removed, none added: 14 `function_tables` moves into
`SemanticExpressionGraphCallTargetCaptureFromSignatures`, 3
`mutating_formals` moves, 3 `facts.consumed` local copies, 1
`function_tables` local copy, 1 `constructors` local copy, 1 borrowed
storage handed to the role operator resolution, and the empty
`no_effects` map in two fields. Moves 52 -> 35, local copies 10 -> 5,
borrowed parameter storage 4 -> 3, two parts of one storage 1 -> 0.
On an idle machine the census took 1092 s and 3979556 KiB at peak, with
three landing analyses (tp2: 1027 s, 3979376 KiB).

## Fixtures

Errors and MIR `Clone` counts compared on 1092 fixtures under
`tests/cases` and 1352 under `tests/self_hosted` (those that import no
compiler source) against the same tree without the change. Every
difference is a program refused before that now lands a copy.

- Under `function_param_flow_summary`, seven refusals kept their rule
  with a map of Arrays, which Clone still copies by value, and refuse at
  the same lines as before. Their map versions are new landed fixtures
  with outputs on C and LLVM: moved table `1`, inout exit copies `0 0`,
  inout callee field `3`, local copies `11 true`, field-held copies
  `0 0 1 0`, inout callee changes `10`, two map parts `0`.
- New: a ref map returned through Clone (String values, Int/Long/Bool
  keys, a struct with a map field) is admitted, and one with Array values
  is refused.
- Under `tests/self_hosted/parity/fixture`, 26 map refusals now land a
  copy. Their gates judge them with the self-host checker, not the native
  one. `aggregate_storage_transfer_smoke.sh` now admits
  `map_alias_negative` (copy at the first part, output `0`, ASan clean;
  [open question](../decisions/2026-10-23_open_aggregate_smoke_negatives_copied.md)).

ASan on C finds no memory error in any landed fixture. Each leaks its maps
at exit, as a map program without Clone does: emitted code frees no map.

## Mutations (11 fixtures)

| Mutation of `clone_copies_deeply` | Caught by |
|---|---|
| no String key | `map_clone_ref_returned_valid` |
| no Int key | `map_clone_scalar_keys_ref_returned_valid` |
| no Long key | the same |
| no Bool key | the same |
| any value | `map_clone_array_value_ref_returned_invalid` |
| no String value | `map_clone_ref_returned_valid` |
| no scalar value | `map_clone_scalar_keys_ref_returned_valid` |
| no map branch | every map positive |
| any key | not caught |

The key test stays as a guard: the type checker refuses every other key
(`HashMap currently supports only String, Int, Long, or Bool keys`), and
both backends copy a map of any other key by value.

## Gates

`function-param-flow-summary` passed (217 s). `test-semantic` 3240,
`test-transpile` 1014, `test-dir` 15, `test-air` 163, `test-rir` 44,
`test-hir` 58, `test-mir` 702 passed, 0 failed (the target exits 2 at its
self-host driver step, as on main); `test-datastructures` and
`production_header_size_smoke.sh` passed. `compare_backends.sh` over all
981 cases: 952 pass; the same 29 fail without the change (8 of the cases
are missing from the default list on main too, so the run passed every
case explicitly). Ownership smokes give the same result with and without
the change except `aggregate_storage_transfer` above; five of them stop
at the missing Rocq toolchain on this machine.

## Found on the way

LLVM gives a wrong `Double` map value with or without Clone
(`MapSet(m, 4L, 0.5)` then `ToString(MapGet(m, 4L))` prints `5.2221e-315`;
C prints `0.5`).

## Not covered

The self-host C emitter copies only Arrays whole, so its String, Option,
struct and map Clone is by value. Nothing in the installed-driver gates
runs a map Clone yet.
