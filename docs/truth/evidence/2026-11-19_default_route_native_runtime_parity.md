# Default routes run native's output or refuse (2026-11-19)

Source: this change on main `f0d16adaf`, then rebased onto main
`2eae64138` and rechecked (end of Gates). Tree: WSL `Ubuntu-C-WSL`
`/root/tm1` (a worktree of `/root/ts2`'s repository). Drivers from
`self_host_compiler_build.sh`: baseline `bin/pgy-self-driver` (main
`b978e8a82`) SHA-256
`31e017760754431e06a111fd4d416e297043ba8de95a035a86553debdfdce76f`; final
`4c4ccec618d6b19e3c130cbab53d41a5cc27a90bc9b20fc75db329c255499079`.

## What each program reached

The [earlier sweep](2026-11-19_scalar_cfg_mutable_declaration.md) looked
only at programs that declare a `var`. A sweep of all 2,693 `tests/cases`,
`examples` and codegen/MIR-lower fixture programs found an eighth program,
`by_value_array_grown_by_inout_callee_last_use_valid` (native 2, LLVM 1).
The default C route was wrong on six of the eight too.

| Program | Route and owner | Cause |
|---|---|---|
| `log_trailing_newline` | scalar program LLVM log operation | printed `%s\n` for every String; the runtime adds a line break only when the text lacks one, and native normalizes a Log literal with a line break as a banner |
| `array_result_owned_named_inout_shallow_set` | scalar program set emission, C and LLVM | MIR's `owned-string-set` receipt (effect 4) was admitted but emitted as a plain set: the literal was stored and the deep drop freed it (abort) |
| `by_value_array_grown_by_inout_callee_last_use` | scalar program LLVM expression owner | a by-value Array formal handed to an inout callee was read from its entry value, not from the entry slot the callee wrote |
| `local_copy_source_sorted` | self-host semantic: statement mutation owner | `ArraySort` (a call, not a statement-form set) selected no Clone of the copy taken before it |
| `by_value_formal_array_sorted`, `by_value_field_alias_written_by_inout_callee`, `by_value_argument_before_its_inout` | self-host semantic: exclusive value growth (formal summary) | an in-place write (sort of a field alias, an inout callee's set reached through a field alias, a set beside the same variable passed by value) did not move the formal, so no Clone landed |
| `named_string_identity_loop_reassignment` | self-host semantic: indexed String borrow | a shallow `ArrayPush` of a borrowed element did not make the receiving Array carry the loan, so its use after `ArrayDropOwnedStrings` read freed bytes |

Where a Clone was selected but MIR carries none, the direct routes still
emitted the program: `driver_compiler_clone_route_owner.pgy` counted only
landings at declared-routine and constructor arguments. A Clone at a
binding's initializer (`let before = items; ArraySet(items, ...)`) was
dropped on C and LLVM.

## The change

- Log: `log_line_text_owner.pgy` holds native's literal rule (banner
  normalization); `direct_mir_llvm_log_line_owner.pgy` emits the runtime line
  rule as one internal LLVM function; the scalar program and scalar CFG
  LLVM emitters call it for every String Log.
- Owned String set: the receipt source owner copies every effect-4 value;
  `direct_mir_scalar_program_array_string_owned_set_owner.pgy` picks the set
  helper that frees the replaced element (C and LLVM).
- Formal reads: a by-value array formal's entry slot is its read storage
  (`direct_mir_scalar_program_llvm_parameter_read_storage_owner.pgy`); the
  rule that the slot exists is in the read-only array parameter policy
  owner, which the slot writer and both readers call.
- Route refusal: a direct route refuses every landing that is not the stored
  value of an owned-string-push or owned-string-set instruction.
- Semantic (`ast_collection_*`): `ArraySet`, `ArraySort` and
  `ArrayReverse` write in place (decision
  `2026-10-21_array_sort_permutes_in_place`).
  - A call-form write, or a hand-off to an inout formal its callee grows or
    writes in place, selects the copies of its local taken before it. For
    the hand-off these are copies from earlier statements only: a copy the
    call statement makes (its result holding the inout argument) is formed
    after the callee writes, and its node is the inout argument itself. A
    write-back copy (a local defined from a member path and assigned back to
    it) and a focus-recovery part keep the sharing admitted before, as a
    statement-form change of their member path does.
  - A formal written whole is recorded as written (`w:`), not grown. A
    by-value Array handed whole to such an inout callee keeps sharing the
    caller's elements, as native does and the self-hosted parser's cursor
    needs (OPEN `2026-10-23_open_by_value_array_element_write_through_inout`).
    A by-value argument beside a written inout argument naming the same
    variable is copied, as beside a grown one.
  - A write through a field alias, a member path or a local holding a
    by-value formal's storage moves that formal
    (`ast_collection_exclusive_formal_write_through_owner.pgy`) when Clone
    copies the formal whole and no other statement of the routine names it.
    When the routine names it again, native copies the alias where it is
    taken, and the self-host keeps the sharing it admitted before.
  - A shallow `ArrayPush` makes its receiver carry the pushed String's exact
    storage loan, including a loan the pushed binding is assigned later in
    the same loop.

## Results on the eight programs

| Program | default C | default LLVM |
|---|---|---|
| `log_trailing_newline` | equal | equal |
| `array_result_owned_named_inout_shallow_set` | equal | equal |
| `by_value_array_grown_by_inout_callee_last_use` | equal | equal |
| `local_copy_source_sorted`, `by_value_formal_array_sorted`, `by_value_field_alias_written_by_inout_callee`, `by_value_argument_before_its_inout` | equal (the round trip lands the Clone) | refused: `direct MIR emission cannot carry the 1 compiler Clone landing(s)` |
| `named_string_identity_loop_reassignment` | refused: `indexed_string_use_after_deep_drop` | same |

The new gate also compiles five written programs. On the baseline driver
`inout_set_after_copy` (a copy, then an inout callee's `ArraySet` of the
source) printed 99 on both default routes for native's 19; the final driver
prints 19 on C and refuses on LLVM (Clone landing).

## Sweep (2,693 programs, each run with a 10 s limit and no stdin)

694 programs are refused by native. For the rest, each default route of
each driver against native LLVM (three examples whose native compile failed
under load in the final run were rerun on their own and are counted here;
all three classify the same on both drivers):

| Leg | equal | refused | other failure | differs |
|---|---|---|---|---|
| C, baseline | 1,392 | 537 | 52 | 18 |
| C, final | 1,398 | 538 | 51 | 12 |
| LLVM, baseline | 755 | 1,192 | 44 | 8 |
| LLVM, final | 766 | 1,197 | 36 | 0 |

Every move: C 5 differs to equal, 1 differs to refused, 1 failure to
equal; LLVM 3 differs to equal, 5 differs to refused, 8 failures to equal.
The failure-to-equal moves are main `f0d16adaf` (runtime helpers declared
with the facts that emit them). No program moved from equal to refused or
to differs on either route.

The 12 default C differences predate this change and LLVM refuses all of
them: `abi_pipeline/projection_chain_abi`, seven `backend_compare`
programs (`branch_defer_skipped`, `float_arith_chain`,
`float_to_string_precision`, `float_unary_minus`, `long_unary_minus`,
`mixed_param_types`, `party_role_bind`) and four
`function_param_flow_summary` programs
(`by_value_lend_result_written_argument_copied_landed_valid`,
`default_aggregate_move_field_element_write_landed_valid`,
`default_aggregate_move_field_set_landed_valid`,
`local_copy_source_element_set_landed_valid`).

An intermediate driver with a wider formal move rule also made
`default_aggregate_move_field_set_landed_valid` equal on C; the final rule
leaves it as it was.

Four intermediate builds were wrong in ways the final one is not:
- recording an inout callee's element set as a growth of the formal moved
  `by_value_cursor_set_through_recursive_parser_valid` from equal to differs
  on C (`[a]0` for native's `[a]3`). It landed a Clone at a by-value cursor
  native shares. The `w:` record replaced it; the program is in the new
  gate.
- moving a formal for every write through its storage made the compiler's
  own source fail in the full fixed point (`full_mir_seed`:
  `unproved_exclusive_sink_entry` at `canonical_instruction_id_owner.pgy:47`,
  then `canonical_mir_execution_owner.pgy:23`). Those routines read the
  formal again (`var field_ids = declarations.field_source_syntax_ids` is a
  copy). The write-through owner's "names it again" condition replaced it.
- driver `c04a25cb...` selected the copy an inout call's own result makes
  (`let query = SemanticExpressionGraphDomainQueryValidateCall(...,
  symbolic_identifier_nodes, ...)` in the compiler's `mir_lower`), so the
  Clone landed at the inout argument and C emitted `&({ ... clone ... })`:
  `cannot take the address of an rvalue` in
  `match_subject_family_owner.sh`. The earlier-statements condition replaced
  it; `inout_result_holds_argument` in the new gate refuses on that driver
  (C build failure) and equals native on the final one.
- driver `c34ba275...` selected a Clone at
  `var graph = analysis.expression_surfaces.expression_graph` in
  `SemanticAstAnalysisResolveExpressionIdentities` (a write-back copy handed
  to an inout callee). Its type holds
  `Array<SemanticAstBuiltinArgumentRetentionCallFact>`, which the C Clone
  emitter does not copy, so the fixed point's `gen2_emit` stopped with
  `CODEGEN ERROR: array kind code requires ...`. The write-back condition
  replaced it.

Compiler Clone landings in the compiler's own MIR
(`driver_bootstrap_main.pgy --pressure-owned-full-fixpoint`): 129 routines
with the baseline driver, 130 with the final one. The only addition is two
`HashMap<String, Int>` Clones in `SemanticAstCollectionExclusiveValueFactsFrom`
(the `starts` and `ends` arguments of the facts constructor, which an inout
callee then changes). The final driver emits C from that MIR (30 MB, 241 s).

## Gates

Run in `/root/tm1` with the final driver and the `pgy` built beside it.

- `default_route_native_runtime_parity_owner.sh`: PASS, 5 programs equal
  on both default routes and 9 equal or refused. With the baseline driver
  it fails at its first program (`log_trailing_newline`: default LLVM
  prints an empty line after `a`, `c`, `line2` and `end`). With driver
  `c04a25cb...` it fails at `inout_result_holds_argument` (default C:
  `self-host C compile failed`).
- 61 gates: 42 scripts from the push core steps and 19 make targets, among
  them those that reach the changed owners (collection ownership semantic,
  array string cleanup, shared storage copy landing, MIR round trip
  semantic, function parameter flow summary, codegen seed and fixed point
  receipts). 59 pass.
  `self-host-replacement-frontier-installed-test-smoke` exits 2 with the
  baseline binaries too (`declaration source module path escapes
  repository`). `self-host-preparation-contract-test-smoke` stops at
  `sot_authority_adequacy_smoke.sh` because this tree has no Rocq; with
  `PGY_ALLOW_MISSING_COQ=1` (declared skip of the Rocq model only) every
  step passes, 253 s.
- Fixed point: `driver_bootstrap.sh` with
  `PGY_SELFHOST_DRIVER_FULL_FIXPOINT=1`, `PGY_SELFHOST_FULL_MIR_BULKHEAD=1`
  and the prebuilt codegen seed: `integrated MIR-consumer fixpoint ok: gen2
  == gen3 (243542 lines)`, 2,338 s.
- Rebased onto main `2eae64138` (driver SHA-256
  `d52961a76806623f7fa9f6069a73d4b203abf139c68f14d608d9a9635a09a0f2`):
  the eight programs and the written ones classify as in the table above;
  the new gate and main's
  `direct_mir_general_route_collection_iteration_owner.sh` PASS; the 61
  gates and that one give the same results as above. The preparation
  contract passed once `bin/pgy.exe` (a copy of `pgy` that a gate run left)
  was removed: with it the installed-driver check looks for
  `pgy-self-driver.exe`. A sweep of the 2,693 programs on the first rebase
  (main `983b6ca4b`) against the pre-rebase driver: 23 LLVM and 1 C
  programs refused -> equal, from main's general-route range and foreach
  commits, and nothing else moved. The fixed point on the rebased commit is
  left to CI.

## Not changed

- The MIR to AST round trip's C `pgy_log` drops every trailing line break of
  a String value; native drops them only from a literal. No corpus program
  shows it.
- A shallow push of a String of unknown provenance (a declared call's
  result) does not make its receiver carry a loan; only exact storage loans
  are carried, as for a carrier a call hands a loan.
- The 12 default C differences that predate this change are unchanged (see
  the sweep).
- A write through the storage of a by-value formal that the routine names
  again keeps sharing on the self-host; native copies the alias there. No
  corpus program shows a difference.
