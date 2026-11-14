# 2026-11-15: self-host admissions of the lifetime fixtures

Run by the Claude root lane on `fix/ref-formal-value-argument` (base
`a7d7655e1`). The method is the one in the
[native measurement](2026-11-15_native_unsafe_negative_admissions_closed.md),
applied to the installed self-host driver: public `pgy --emit-c` without
`--native-pipeline`, with `PGY_SELF_DRIVER_BIN` set. Each admitted program
is built with `-fsanitize=address,undefined` and run for at most 10 s. The
inputs are the 868 fixtures of `collection_field_lifetime/` and
`aggregate_storage_transfer/`. The baseline is the driver built at
`a7d7655e1`. The fixed driver adds the two changes listed below.

## Result

| | baseline | fixed |
|---|---|---|
| Negatives admitted (of 485) | 17 | 17 |
| of which a memory error under ASan | 0 | 0 |
| Non-negative inputs admitted and clean (of 383) | 224 | 223 |
| Non-negative inputs refused | 105 | 111 |
| Non-negative inputs admitted and not clean | 54 | 49 |
| of which a memory error under ASan | 5 SEGV + 1 stack overflow | 1 stack overflow |

- Only six verdicts changed, each from admitted to refused:
  - five positives that crashed with SEGV: `borrow_formal_{chain_read_positive,
    identity_input, same_name_local_positive}` and
    `element_escape_{formal_chain,static_root}_positive`. Each deep-dropped
    an `Array<String>` local that aliased a `ref` formal, or that held
    literal elements it did not own.
  - `storage_opaque_return_read_positive`, which had run clean. It is a
    user call result whose elements the self-host has no fact for (see
    below).
- The stack overflow is `inout_cycle_read_positive`. It is endless mutual
  recursion, the same case as native.
- The 17 admitted negatives all run ASan-clean. 16 exit 0.
  `exclusive_inout_exit_before_writeback_negative` exits 1, which is the
  `Int` its `Main` returns. Native rejects that fixture at parse time
  because `fail` is a reserved word. The self-host parser accepts `fail`
  as a parameter name.
- Two positives produce C that does not compile:
  `builtin_retention_multi_ordinal_positive` and
  `generic_default_local_selection_positive`.
- The 46 other non-clean results are ASan-clean runs that exit non-zero.
  A probe with `func Main() -> Int { Log(7); return 3; }` showed why: the
  self-host C exits with the value `Main` returns (3), while native C
  exits 0.

## Changes measured

- An actual of a `ref` formal passed on by value is dereferenced on the
  way: `(*arg)` in C, a load in LLVM. Before this change the callee
  received the pointer slot. The backend compare case
  `ref_formal_value_argument` runs it on both legs.
- Direct MIR `Array<String>` cleanup
  (`direct_mir_scalar_program_array_string_cleanup_policy_owner.pgy`)
  takes its drop from the admitted transition row. A local without a row
  is decided by its definitions, provided nothing pushes into it, sets
  it, or pops it:
  - one literal of borrowed elements: the storage is released;
  - `Split`, `Args`, `DirWalk` or `SliceCopy` results, whose elements the
    runtime copied: the storage and the elements are released;
  - anything else, such as a user call result or an alias of another
    local: refused with `CODEGEN ERROR` and no artifact.

  `tests/self_hosted/parity/ownership/direct_mir_array_string_cleanup_fact_owner.sh`
  (self-host contracts shard) checks both legs. It refuses a call result
  and a formal alias, and it compiles and runs a borrowed literal, pushed
  owned copies and a `Split` result.

## Gaps left open

- The self-host lands no element copy where a callee's elements escape
  through its result. Native records the call result's owned elements, so
  it admits `storage_opaque_return_read_positive`; the self-host refuses
  it.
- The self-host front end admits `let first = values` for a `ref` formal
  `values`. Native refuses this alias semantically. The cleanup refusal
  now catches it at codegen.
- Native admits 297 of these fixtures that the self-host refuses; the
  baseline driver left 294 such. Native refuses 17 that the self-host
  admits, four of them negatives (`exclusive_inout_exit_before_writeback`,
  `inout_shadow_drop`, `native_aggregate_release_intervening_call`,
  `post_field_return`), and all 17 run clean.

## Regression checks

- Native `--emit-c` of `driver_bootstrap_main.pgy`: the C emitted by the
  `a7d7655e1` compiler and by the fixed compiler is identical (0
  differing lines).
- Linux push shards on the fixed tree: seed, driver and core pass. In the
  contracts shard, only the new cleanup gate failed. The cause was the
  script: it searched stderr for the refusal text, but the driver prints
  its `CODEGEN ERROR` on stdout.
- test-semantic: 3394/3394.
