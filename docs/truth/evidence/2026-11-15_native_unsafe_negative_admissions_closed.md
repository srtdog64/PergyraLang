# 2026-11-15: native unsafe admissions after the checker fixes

Run by the Claude root lane on the fix branch, with the method of the
[2026-11-14 remeasurement](2026-11-14_native_unsafe_negative_admissions_remeasured.md)
extended to every fixture of the two directories (868: negatives,
positives and analysis inputs). The baseline is native `pgy` from
`b06f07c1a`; the fixed compiler is the same tree plus the checker fixes
listed below.

## Result

| | baseline | fixed |
|---|---|---|
| Negatives admitted that exit non-zero | 39 | 4 |
| of which a memory error under ASan | 36 | 1 |
| Non-negative inputs admitted that are not clean | 52 | 50 |

- Every fixture whose verdict changed is a negative, except
  `focus_recovery_scope_break_continue` (an analysis input that freed one
  buffer twice; it now lands a copy and runs clean). No positive changed.
- Negatives that now run clean with a landed `Clone` (copy semantics of
  the 2026-11-12 decision): eight `exclusive_*` field-growth cases,
  `exclusive_branch_rebind_continue`, `exclusive_alias_element_write`,
  `numeric_string_shadow`.
- Negatives now refused: the 13 owned-String transfers, the 5 releases of
  shared or borrowed parts, `inout_{index_nested_own,
  nested_own_argument_copy, index_deferred_own, deferred_copy_own}`,
  `exclusive_{focus_reads_grown_field, mutating_sibling_call}`,
  `read_temporary_releasing_sibling`, and nine negatives that had run
  clean (`aggregate_release_duplicate_storage`,
  `empty_capture_{after_push, capacity, foreign_member_assignment,
  member_local_alias}`, `field_ctor_own_formal_nested_retire_statement`,
  `inout_event_nested_own_push`, `member_completed_read_borrowed_text`,
  `owned_string_literal_transfer_{aliased_concat_result, multi_element}`).
- Still admitted and not clean (all `_negative`):
  - `inout_cycle_drop`: the stack overflows in the endless mutual
    recursion before the drop, which releases an empty Array. The
    positive `inout_cycle_read_positive` overflows the same way; both are
    analysis inputs, and no lifetime rule bounds recursion.
  - `copy_field_loop_holder`: the holder is a copy that never grows, so
    the loop never ends (copy semantics, no freed memory read).
  - `exclusive_nested_copy_{grown,prefix}_read`: the copy is shorter than
    the index, which the runtime bounds check stops with a panic.

## Checker fixes measured

- A later argument of the same call that moves (own hand-off) or changes
  (a callee growing a copy of the field) storage an earlier argument
  holds refuses the earlier argument, unless the callee never touches
  that field.
- A defer cleanup that reads an outer binding is checked at every routine
  exit; an own hand-off after the defer refuses the read.
- Owned String transfers (`own String` actuals, drops of Array<String>
  literals of named Strings): the storage source is released for every
  binding sharing it; a literal, borrowed formal, field or element read,
  allocator-owned result or released String is refused; a user-call
  result is refused when its owned-result examination is disproved
  (every return a literal, a returned local kept elsewhere, a literal
  argument handed back) and left at the native boundary when merely
  unproved.
- Struct field verdicts separate a write from an escape through the
  result; a lending call that writes the field moves it. The move walk
  carries break and continue states to the next iteration.
- A String read from a binding's storage beside that binding handed to a
  releasing formal, and a binding whose parts share storage or hold
  borrowed elements handed to a releasing formal, are refused; a formal
  releases when its routine deep-drops storage rooted at it or forwards it
  to a releasing formal.

- An owned-result receipt whose source calls a routine that returns only
  literals copies that call's result where it is returned (as it copies a
  literal branch) instead of counting it owned.

## Canonical driver

Native `--emit-c` of `driver_bootstrap_main.pgy`: 712.8 s and 2.70 GB
with the baseline compiler, 748.8 s and 2.70 GB with the fixes. The
emitted C keeps every literal-branch copy of the baseline (302
`StringConcat("", "")` returns) and adds 14 + 9 copies of static-only
callee results (`MirDeclFieldKindSpellingAt`,
`CompilerHarnessLspToolSourcePath`) and one Array `Clone`; the other
differences are renumbered temporaries. test-semantic 3394/3394.
