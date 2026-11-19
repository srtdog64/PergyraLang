# Self-host default LLVM gap census (2026-11-19)

Read-only finding. It owns no semantics, progress or registry status.

Source: main `9a5d7362e` (driver built in WSL `Ubuntu-C-WSL` `/root/ts2`,
`bin/pgy-self-driver` SHA-256
`539cd85eec0217f964d60e5a890a38dbf3823749bc6eb2568e2ed3bce14dc860`).
Corpus: the 1,398 programs of the
[mutable declaration sweep](../truth/evidence/2026-11-19_scalar_cfg_mutable_declaration.md)
(every `tests/cases` and `examples` program that declares a `var`, plus the
codegen and MIR-lower fixtures). Each program is compiled with
`bin/pgy --backend=llvm` (the self-host driver) and with
`--native-pipeline --backend=llvm`, then run for at most 10 s with no stdin.

## Totals

| Result | Programs |
|---|---|
| Native admits | 1,105 |
| Self-host output equal to native | 483 |
| Self-host output differs from native | 7 |
| Self-host `CODEGEN ERROR` | 362 |
| Self-host fails before codegen or at link | 253 |
| Native refuses (self-host also refuses all but 10) | 293 |

## Wrong or broken output (fix first)

- 7 programs run with output unlike native: six
  `tests/cases/function_param_flow_summary/*_landed_valid.pgy` and
  `codegen/fixture/log_trailing_newline.pgy` (names and symptoms in the sweep
  record above).
- 6 programs emit LLVM IR that `clang` rejects (`use of undefined value`:
  `mir_lower/fixture/nested_loop_cfg`, `backend_compare/class_node_field_access`,
  `string_split_first`, `named_string_all_def0_branch_valid`;
  `invalid indices for extractvalue`: `map_key_variants`, `map_ops_long_bool`).
- 1 driver panic (array index out of bounds) on
  `mir_lower/fixture/ast_node_array_set.pgy`.

## Front-end refusals (234 diagnostics)

The self-host semantic or parse stage refuses a program native admits.

| Count | Stage / code |
|---|---|
| 109 | semantic `borrow_boundary_escape` |
| 19 | semantic `ast_artifact_invalid` |
| 15 | semantic `undefined_function` |
| 14 | semantic `builtin_native_pipeline_only` |
| 12 | semantic `initializer_type_unresolved` |
| 11 | parse `statement_head_unexpected_token` |
| 11 | semantic `statement_native_pipeline_only` (8 `parallel`, 3 channel send) |
| 9 | semantic `let_type_mismatch` |
| 19 | parse `surface_not_covered` (zone bind 8, intent step clauses 6, zone maintain/vessel 3, `use` 1, `pin` 1) |
| 15 | 9 other codes (call arity 4, expected token 3, type name 2, six with 1) |

The other 19 failures are: `ArrayDrop` provenance 4, collection ownership
rows 3, DIR intent endpoint or step binding 3, one `defer` target and one
machine-layer projection, plus the 6 link failures and the panic above.

## `CODEGEN ERROR` refusals (362), largest groups

| Count | Refusal |
|---|---|
| 30 | scalar CFG expression admission `leaf-operand` (25 binary, 5 call) |
| 25 | `three-routine structural shape is unsupported` (multi-routine dispatcher) |
| 24 | scalar program route `seal` |
| 17 | compiler `Clone` landings that direct MIR cannot carry |
| 15 | scalar program route `single-program-shape` |
| 12 | `defer` statement in the scalar CFG routine admission |
| 12 | `Array<String>` cleanup without an admitted ownership row |
| 35 | scalar CFG range and loop facts: range bound graph 11, range flow/type identity 10, range LocalRef binding 4, foreach facts 5, indexed-assignment facts 5 |
| 9 | scalar CFG `collection_ownership_transition` |
| 11 | scalar CFG verified plan identity (`program_readiness`) |
| 29 | scalar CFG routine admission: `local_inventory` 10, `return_abi` 10, intent declaration rows 9 |
| 143 | smaller groups |

## Experiment: the general route claims every one-routine program

The scalar program route claims a one-routine program only when a
control-flow or builtin shape owner claims it, or it carries a record or
enum (`single-program-shape`). A scratch driver without that guard
(SHA-256 `e972e12bcee8652b3bb1b21841a3dd2c39b23fd9234047f8e37194d326419949`,
not committed) changed 7 programs on the same corpus:

- refused -> equal: `codegen/fixture/for_continue.pgy`,
  `backend_compare/for_range_explicit`;
- equal -> refused: `array_pop`, `for_each`, `foreach_array_int_sum`,
  `backend_compare/for_in_array_int` (the route ends in `Die`: "routine input
  has an external collection plan") and `array_max` (`range-condition`).

The general route's routine input requires no foreach fact and no String
array, Array<Int> or indexed-assignment plan
(`DirectMirScalarCfgProgramInputReady`). Foreach and Array mutation exist only
as one-routine extensions of the scalar CFG plan, so the three collection
`projection.direct_mir_*` BRIDGE rows (String array push, collection pop,
Array<Int> program) cannot retire by deleting their routes until the
general route owns foreach and Array mutation per routine. The fourth row,
`projection.direct_mir_scalar_cfg_program_extension`, is the plan the general
route itself runs on.
