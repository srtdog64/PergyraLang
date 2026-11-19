# Scalar CFG reads `var` declarations again (2026-11-19)

Source: main `f25b0581e` plus this change. Trees: WSL `Ubuntu-C-WSL`
`/root/ts2` (patched) and `/root/ts3` (unpatched worktree at `f25b0581e`).
Drivers from `make self-host-compiler`: patched `bin/pgy-self-driver`
SHA-256 `539cd85eec0217f964d60e5a890a38dbf3823749bc6eb2568e2ed3bce14dc860`
(955 s); unpatched
`691d17183f23bd74398f7ca69876840af7c6188acf7c972aeb1b543abbcb8ef7` (1000 s).

## Defect

Since `8a56b31e4` (2026-11-10) both MIR producers name a `var` declaration
row `mutable-def` and a `let` row `ssa-def`;
`BindingDeclarationModeFromMirName` (`src/self_hosted/hir/assignment_operation_owner.pgy`)
owns that vocabulary. Three direct MIR readers still compared the name with
`ssa-def` alone:

- the scalar CFG graph admission (`definition fact is invalid`);
- the scalar CFG `ArrayReverse` source admission;
- `DirectMirScalarDefinitionLiteral`, the CFG entry literal reader.

A program that reached them with a `var` declaration was refused with
`CODEGEN ERROR` on the default LLVM route and on `--mir-json-backend=c|llvm`.
`var xs: Array<Int> = [...]; var total: Int = 0; for v in xs { ... }` was one.
The scalar CFG gates run only in the manual `self_host_parity.yml` workflow,
so no required job saw it.

The change reads the name through the owner. A non-declaration row still must
be `ssa-def`.

## Corpus sweep (default LLVM route, runtime against native LLVM)

Inputs: the 1,247 `tests/cases` and `examples` programs that declare a `var`,
plus the 151 codegen and MIR-lower fixtures (1,398). Each program is compiled
with `bin/pgy --backend=llvm` and run with a 10 s limit and no stdin.

| Driver | equal | refused | compile failed | differs | native refused |
|---|---|---|---|---|---|
| unpatched | 470 | 466 | 445 | 7 | 10 |
| patched | 483 | 453 | 445 | 7 | 10 |

The only change is 13 programs moving from refused to equal:
`examples/break_continue.pgy`, `examples/for_test.pgy`, the codegen fixtures
`array_max`, `array_pop`, `for_sum` and `for_each`, the MIR-lower fixtures
`for_break_exit`, `foreach_array_int_sum` and `for_each_call`, and
`tests/cases/backend_compare/{break_continue,for_in_array_int,for_in_call_iterable,nested_loops}/main.pgy`.

The 7 that differ from native differ on both drivers (not introduced here):
`codegen/fixture/log_trailing_newline.pgy` (extra empty lines) and six
`tests/cases/function_param_flow_summary/*_landed_valid.pgy` programs:
`array_result_owned_named_inout_shallow_set` aborts (134),
`by_value_argument_before_its_inout` prints 55 for 15,
`by_value_formal_array_sorted` 11 for 13, `local_copy_source_sorted`,
`by_value_field_alias_written_by_inout_callee`, and
`named_string_identity_loop_reassignment`, which prints different garbage
bytes on each driver.

## Gates on the patched driver

| Gate | Result |
|---|---|
| `direct_mir_mutable_declaration_owner.sh` (new) | PASS: 2 programs on six legs; unknown declaration name and a reassignment named `mutable-def` refused on C/LLVM with no artifact |
| `self_hosted_component_contract_smoke.sh` | PASS (graph admission at its 450-line cap) |
| `sot_authority_edge_smoke.sh` | PASS: CLOSED=70 BRIDGE=22 ACTIVE=2 (unchanged) |
| general-emitter coverage and builtin rows, `one_mir_array_param_projection.sh`, Array<String> cleanup, Array<Int> return smoke | PASS |
| `one_mir_array_pop_projection.sh` (manual workflow) | PASS; refused the program before |

The 18 scalar CFG gates the manual workflow runs (named in
`one_mir_cfg_break_case.sh`): 1 passes on the unpatched driver
(`string_array_mutation`) and 4 on the patched one (adding `array_pop`,
`returned_array_foreach`, `scalar_cfg_foreach_array_int`). No gate that
passed before fails now. Nine refused their program before (`c rejected`):
three now pass, four fail at a later check (emitted C without `<stdint.h>`
under the gates' strict compile in `continue_backedge` and `for_break_exit`,
a missing runtime header in `mixed_collection_foreach`, an artifact text pin
in `array_int_max`), and two are still refused (`array_index_assignment`,
`indexed_string_array`). The other eight fail as before: four stale
negative mutations the general route admits as valid programs, a source MIR
hash pin, a missing artifact line, and two `-Werror` warning builds.
