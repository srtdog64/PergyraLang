# 2026-11-17: driver and fuzz shard failure set, and the general emitter rows that close part of it (DM2)

Track DM2 of the Claude root lane, after
[DM1](2026-11-16_direct_mir_dm1_coverage_families.md). Run on WSL
`Ubuntu-C-WSL`.
- Branch `dm2-direct-mir`, from `a1cb26a93` (DM1 on `integrate/track-a`).
- `/root/tdm1` holds the branch; the pair was rebuilt there for each step.
  All push shards ran with `TMPDIR=/root/tdm1-tmp`.
- Census drivers, never committed: `/root/tdm1d` (DM1 plus the DM0 census
  patches) and the same tree with the DM2 source delta applied.

The weekly run 43914895554 stopped on direct MIR in two places:
- the driver shard (`ci_self_host_platform_parity_shard_owner.sh`,
  `PGY_CI_SELF_HOST_PARITY_SHARD=driver`): `valid_array_builtins` refused at
  `stage=builtin-call node=3 row=8`;
- `make fuzz-backend-parity-matrix-test-smoke`: the generator refused at
  `stage=builtin-call node=3 row=285`.

## The whole failure set, counted first

### Driver shard

Method: a scratch copy of `driver_rung2_body_parity.sh` and the owners it
sources, in which every `exit` records the failing check and the fixture
and the gate goes on. C lane, the installed driver, the 20 body fixtures and
285 MIR fixtures. The weekly log shows only the first stop.

34 fixtures, 97 failure records. One fixture can fall in more than one
group.

| Group | Fixtures | What fails |
|---|---|---|
| A. positive program refused by the general emitter | `valid_array_builtins`, `loop_collect_distinct_set`, `for_each`, `array_index_assign` | `ArrayMap` (row 8) and `ArrayFilter` (row 9) have no builtin row; `SetSize` on `Set<String>` has none; `for_each` is declined at route stage `single-program-shape`; `array_index_assign` is refused at `collection_ownership_transition` on `Main`. The transition refusal of `valid_array_builtins` is a cascade: the receipt plan check fails when an earlier expression refusal leaves the routine's operations short. |
| B. self MIR differs from the native oracle MIR | `reassign_block`, `nested_loop_cfg`, `if_else_assign`, `branch_defer_scope`, `branch_defer_skipped`, `dish_result_collect`, `class_factory_result_wrap`, `class_result_chain_loop`, `class_method_result_loop`, `class_bump_option_match`, `class_method_enum_classify`, `array_match_action_sim`, `valid_array_builtins` | the `mir-json` artifact parity, then the `self-mir-c` parity. In `reassign_block` the oracle's canonical MIR folds `if true` to one block and the self MIR keeps three. |
| C. negative still refused, refusal text drifted | missing/invalid expression graph on `for_each`, `str_array`, `valid_array_builtins`, `array_index_assign`, `array_destructure`, `array_of_strings_loop`, `bubble_sort_small`, `loop_collect_distinct_set`; and `str_array`, `class_method_coalesce_call`, `class_method_short_circuit`, `class_param_method_arr`, `coalesce_accumulate_loop`, `coalesce_in_if_condition`, `nested_coalesce_chain`, `for_in_array_literal_iterable`, `generic_default_ability_bind_dispatch`, `list_ops`, `list_push_get_loop`, `for_each` (iteration graph), `array_index_assign` (native assignment lane) | the mutated input is still refused, with another diagnostic. For the graph negatives the general emitter now refuses earlier, at `stage=definition`. |
| D. negative now accepted | `list_literal_context`, `sequence_literal_list_queue`, `for_each_call` | a List literal and a Queue literal of the wrong element type are accepted from source; MIR without the foreach synthetic local is accepted (that probe's mutation also did not apply). |
| E. MIR fact or emitted-C text pin | `nested_generic_containers` (`source_locals` fact), `for_each_call` (foreach synthetic local), `loop_collect_distinct_set` (`Set index C missing`, four terms) | text checks on emitted artifacts. |

### Fuzz matrix

Method: the generator built natively, seeds 1, 42 and 1001, 12 cases each,
every case and the generator through the default C and LLVM routes and
native, stdout and exit compared.

- The generator (`src/self_hosted/fuzz/backend_parity_generator/main.pgy`),
  both default routes: `SeedRandom(seed)` (row 285) and `Random(n)` (rows
  256, 257, 291) have no builtin row. The transition refusals of `Main` and
  `RenderProgram` are the same receipt-plan cascade as above.
- Generated programs: native refuses 20 of the 36. The templates of
  variants 1, 2, 7 and 8 declare with `let` bindings the program reassigns
  or grows (`xs`, `parts`, `delta`, `branch_pos`, `pred_pos`, `i`).
- With the templates fixed, native admits all 36, and:
  - variant 0 (3 cases), default LLVM: `stage=range-condition`, a range
    loop with a literal bound (`for i in 0..5`);
  - variant 1 (5 cases), default LLVM: `range bound graph is invalid`, a
    range loop bounded by a `let` value;
  - variant 5 (4 cases): default LLVM declines at `single-program-shape`;
    default C falls to the older codegen, whose program redefines
    `pgy_bool_to_string` next to `pgy_runtime.h` and calls an undeclared
    `pgy_log`;
  - variants 2, 3, 4, 6, 7 and 8 pass on both routes. Default C
    (`--emit-c-verified`) passes variants 0 and 1, while the MIR backend
    (`--mir-json-backend=c`) refuses them: the source route reaches another
    emitter for them; which one was not traced.

## What DM2 adds to the general emitter

| Row | What the general emitter now does | Owner |
|---|---|---|
| `Random(n)`, `SeedRandom(seed)` | expression kinds `RandomInt` (156) and `SeedRandom` (157) in the case-math family, on the registry rows `math/random` (`returns_int`) and `math/seed-random` (`int_to_void`). Both backends call native `Random` and `SeedRandom`, so the random capability check and the runtime generator are the native ones. C: `pgy_random(n)` returns `Random((int32_t)n)`, and the program includes `pgy_runtime.h`, which then owns the print helpers. LLVM: `trunc` to `i32`, `call @Random(i32)`, `sext`. | `scalar_program/direct_mir_scalar_program_case_math_*_owner.pgy`, `..._c_int_math_materialization_owner.pgy`, `..._llvm_int_math_materialization_owner.pgy`, `scalar_cfg/direct_mir_scalar_cfg_program_c_artifact_emission_owner.pgy` |
| `SetSize` on `Set<String>` | expression kind `SetStringSize` (158). C: `pgy_set_size_string(&s)`. LLVM: `pgy_set_size_raw_export(ptr)` (`i32`, then `sext`). The receiver is a Local or Parameter. | `..._collection_builtin_signature_owner.pgy`, `..._collection_expression_readiness_owner.pgy`, `..._c_set_string_expression_owner.pgy`, `..._llvm_set_string_expression_owner.pgy`, `scalar_cfg/direct_mir_scalar_cfg_llvm_foreign_declaration_owner.pgy`, `codegen/runtime_abi/set_runtime_owner.pgy` |
| range loop with a literal bound, program route | the range condition admits a bound node that is the Int literal the range fact names, and the range graph shape accepts the program route's expression condition for it. | `scalar_cfg/direct_mir_scalar_cfg_program_range_condition_owner.pgy`, `scalar_cfg/direct_mir_scalar_cfg_range_graph_shape_owner.pgy` |
| expression kind range | the set shape check bounded kinds at 155; the last identity is 158. | `..._task_expression_kind_owner.pgy` |
| `ToString(Bool)` with `pgy_runtime.h` | a C program that includes the header uses its `pgy_bool_to_string` (same fresh-String contract) instead of defining a second one. Found when `loop_collect_distinct_set` was admitted. | `..._c_builtin_materialization_owner.pgy` |
| fuzz generator templates | seven template bindings that the generated code reassigns or grows are `var`. | `src/self_hosted/fuzz/backend_parity_generator/main.pgy` |

Owner caps: twelve rows of `tests/self_hosted/parity/mir/scalar_program_owner_caps.tsv`
and the C artifact owner's row in `self_hosted_component_contract_smoke.sh`
are set to the new sizes.

## Gate

`tests/self_hosted/parity/direct_mir/direct_mir_general_emitter_builtin_rows_owner.sh`
(push core shard). It passes with the branch driver and fails at its first
positive with the DM1 driver (built at `821b3cc8e`, whose direct MIR code
`a1cb26a93` carries).

| Case | Kind | Result on the branch |
|---|---|---|
| `random_draw`: `SeedRandom` twice, `Random` through a callee and in a loop, `Random(0)` | positive | same stdout and exit 0 on native C, native LLVM, default C, default LLVM and the direct MIR C backend |
| `set_size`: `SetSize` of a grown and an empty `Set<String>`, `ToString(SetHas(...))` | positive | five legs, exit 0 |
| `literal_range`: `for i in 0..5` calling a routine (fuzz variant 0) | positive | five legs, exit 0 |
| `array_map`: `ArrayMap` with a declared function | neighbour | default LLVM refuses, native prints 6 |
| `let_range`: `for i in 0..limit`, `limit` a `let` value (fuzz variant 1) | neighbour | default LLVM refuses, native prints 14 |
| MIR mutation: `SeedRandom(7)` given a String literal, graph digest recomputed | negative | refused, `stage=builtin-call` |

The three positives under ASan (`/root/legs_lib.sh`, LLVM at `-O0`) on
native C, native LLVM, self-host C and self-host LLVM, and the direct MIR C
backend under gcc ASan/UBSan: all clean, stdout equal to native.

## Census

The DM0 census inputs (`semantic`, `mir_lower`, `codegen` MIR, as in DM1),
`--mir-json-backend=llvm`, 9 GiB watchdog. Before: the DM1 census driver
(label `mine2`). After: the same tree with the DM2 source delta through
`b3eaa5b9e` applied (label `dm2`); `1af528f30` changes only C helper text,
which the LLVM census does not reach.

| Input | Before | After |
|---|---:|---:|
| `semantic` | 31 | 31 |
| `mir_lower` | 52 | 52 |
| `codegen` | 59 | 59 |

No cause appears that was not there before, and no routine is refused that
was not refused before. The compiler's own sources do not reach the new
rows in a refused routine.

## After

Pair rebuilt at `1af528f30`.

Driver shard, same recorded-exit copy: 97 records -> 91, still 34
fixtures.
- `loop_collect_distinct_set` now runs on the direct MIR C backend with the
  native oracle's output (`4`, `true`, `true`, `true`, `false`); its MIR
  integration, self-MIR consumer, source path, compile and run records are
  gone. Left: the four `Set index C missing` pins, which expect the older
  codegen's source-named locals (`PgySet_String distinct = ...`); the
  general emitter calls the same `pgy_set_*_string` functions on
  `pgy_local_N` slots. And the graph-negative diagnostic drift of group C.
- `valid_array_builtins` is unchanged: `ArrayMap` has no row.
- No record is new.

Fuzz matrix (`make fuzz-backend-parity-matrix-test-smoke`): the generator
now builds on both routes; the target stops at generated case 2 of seed 1
(variant 1, the `let`-value range bound) on LLVM. The enumeration over the
36 cases leaves variant 1 (5 cases, LLVM) and variant 5 (4 cases, C and
LLVM).

Push shards and the fixed point, `/root/tdm1`, prebuilt pair built at
`1af528f30` (later commits change only gate and doc files),
`PATH=/root/nowsl-bin`, `TMPDIR=/root/tdm1-tmp`:

| Run | Result |
|---|---|
| `PGY_CI_SELF_HOST_MODE=prebuilt PGY_CI_PUSH_LINUX_SHARD=core PGY_ALLOW_MISSING_COQ=1 make ci-push-linux`, at `4a3a87987` | all 65 steps ok |
| same, shard `self-host` | all 14 steps ok |
| `driver_bootstrap.sh`, full fixed point, seed part | `gen2 == gen3` (242,003 lines) |

A first run of the two shards failed one step each. Core
`gate-script-reachability` found the scratch gate copy of this census left
in the tree; it was removed. Self-host `self-host-preparation-contract`
found the C artifact owner over its component line cap; the cap was raised
in `4a3a87987`.

## Left, with the missing fact

In the order they block the two targets:
1. `ArrayMap` / `ArrayFilter` (`valid_array_builtins`). A builtin call's
   argument gets no expected type: `DirectMirScalarProgramContextualExpectedType`
   needs a callee syntax id, and builtins have none. A declared function
   passed as a value is admitted only with an expected callable type.
   Missing facts:
   - the callable argument's type for a builtin call;
   - an `Array<Int>` map/filter helper row for C and LLVM;
   - a fresh owned result for the collection transition (as `MapKeys` has
     in `direct_mir_scalar_program_owned_collection_definition_owner.pgy`).
   Owners: `..._builtin_signature_projection_owner.pgy`,
   `..._call_argument_expected_type_owner.pgy`. Last consumers: the C and
   LLVM expression emitters.
2. Range loop bounded by a `let` value (fuzz variant 1). The branch's bound
   is a leaf with one SSA use; `direct_mir_scalar_cfg_range_bound_owner.pgy`
   accepts a leaf only with no use. Missing fact: a typed bound identity for
   an SSA value in the range fact. `stop_value_id` cannot carry it: the
   older single-routine branch admission reads it as an `ArrayLength`
   bound. Owners: the range bound and range fact owners. Last consumer:
   `direct_mir_scalar_cfg_program_range_condition_owner.pgy`.
3. Programs of one routine (`for_each`, fuzz variant 5). Route stage
   `single-program-shape` leaves them to per-shape routes that no longer
   claim them. Variant 5 also needs `Slot<T>`: the value type plan rejects
   `Slot<Int>` (`stage=definition-projection`) before any slot operation.
4. `array_index_assign`: `collection_ownership_transition` on `Main`, the
   ownership family.
5. Group B (13 fixtures): native oracle MIR and self MIR differ (for
   example `if true` folding). That is the MIR producer side, not the
   direct MIR emitter.
6. Group D first, then C and E. The accepted negatives (List and Queue
   literals of the wrong element type accepted from source; MIR without the
   foreach synthetic local accepted) may be fail-open and need a check
   before the text drifts.
7. Found while writing the gate: a two-routine program ending in
   `for j in 2..2 { Log(j); }` fails `--emit-c-verified` with `MIR expression
   occurrence admission failed`, with the DM1 driver too; the MIR backend
   admits it.
8. `SetHas` whose result goes straight into a call (`Log(SetHas(s, x))`)
   is refused at `builtin-call`: its row is selected only for an expected
   `Bool`.
9. Variant 5 on C: the older codegen's program defines `pgy_bool_to_string`
   next to `pgy_runtime.h`.

Items 2 and 3 of the DM2 brief (the role receiver ABI in the general
emitter, and `MapNew` outside the entry block) were not started.
