# Native checker: ListSet and ListRemove retire String loans (2026-11-20)

Source: main `9c4ac12af` plus the uncommitted
[foreach String loan change](2026-11-20_native_foreach_string_borrow_refusal.md)
(the base here), plus this change, on branch
`claude/stoic-kilby-72aa03`. Tree: WSL `Ubuntu-C-WSL` `/root/pgy-lst`
(base binary kept as `bin-base/pgy`); Linux gcc 13.3 with ASan/UBSan,
LLVM 18. Rule:
[decision](../decisions/2026-11-20_list_string_release_retires_unknown_loans.md).

## Before

What the runtime does with a `List<String>` element, C
(`pgy_runtime_list_set_inline.h`) and LLVM
(`pgy_runtime_lib_list_raw_exports.h`) alike: `ListPush` and `ListSet`
store a `strdup`; `ListSet` frees the String it replaces; `ListRemove`
frees the String it removes; `ListGet` returns the stored pointer.

The String loan owner retired loans only for an `Array<String>` receiver,
and the List mutators retired nothing. A `ListGet` result and a for-in
binding over a List already carried the unknown source. On base, each of
these fixtures (`tests/cases/foreach_string_borrow/`) compiled, printed
freed memory and failed ASan with heap-use-after-free on native C and on
the LLVM leg (`/root/legs_lib.sh`: IR at -O0 with `sanitize_address`),
where LLVM compiled it:

| Fixture | Shape | Base C | Base LLVM |
|---|---|---|---|
| `list_set_while_bound_invalid` | `for name in items { ListSet(items, 0, ...); Log(name); }` | UAF | UAF |
| `list_remove_while_bound_invalid` | the same with `ListRemove(items, 0)` | UAF | UAF |
| `list_get_set_copied_valid` | `let name = ListGet(items, 0); ListSet(items, 0, ...); Log(name)` | UAF | refused: `missing source-local type metadata for 'name.1'` |
| `list_get_remove_copied_valid` | `let name: String = ListGet(...)`, `ListRemove`, `Log(name)` | UAF | UAF |
| `list_next_iteration_outer_read_copied_valid` | outer `let first: String = ListGet(...)` read at the top of the next iteration after the body's `ListSet` | UAF | UAF |
| `list_kept_after_remove_copied_valid` | `kept = name` in a List loop, `ListRemove` after it, `Log(kept)` | UAF | UAF |

The LLVM refusal of an unannotated `let` from `ListGet` is a codegen gap
unrelated to ownership: every such program, safe or not, stops there, so
the other `let` fixtures carry `: String`. The three positives that read
before the free, or only push (`list_read_before_set_valid`,
`list_get_read_before_remove_valid`, `list_get_push_valid`, which grows
the List past its capacity of 16), were ASan-clean on base.

## Change

- `indexed_string_borrow_invalidate_owner_source` also takes a
  `List<String>` receiver. It retires loans of the unknown source and of
  the receiver's storage source, which no List loan carries.
- `ListSet` and `ListRemove` on a `List<String>` call
  `semantic_indexed_string_borrow_invalidate_owned_element` after their
  argument checks. `ListPush` does not.
- The refusal names `ListSet` and `ListRemove` among the releasing
  builtins: "String 'x' borrows an Array or List element that was replaced
  or released".

Objective card: no admitted program reads a String a List mutator freed.
Fact owner: the indexed String loan owner, which already retires
unknown-source loans for owned Array releases. Last consumers: the use
check (`semantic_indexed_string_borrow_validate_use`), which refuses a
retired foreach binding and sends a retired `let` or assigned loan to the
existing copy at its definition, and the loop owners, which check a body
again when it retired a loan. Forbidden: a List storage source that the
store checks would read as an Array owner (decision, "Why"), a copy per
`ListGet`, a scan for mutator names.

## Results

| Check | Result |
|---|---|
| `foreach_string_borrow_refusal_smoke.sh` with the 9 List fixtures added (11 refusals at the read with no executable, 12 admissions with expected output, each on C and LLVM) | this change PASS. Base FAIL at `set_while_bound_invalid (c)`, because the refusal text changed; with a scratch copy whose needle accepts both texts, base FAIL at `list_set_while_bound_invalid (c)`: "expected a semantic refusal, exit 0" |
| The 9 List fixtures, run on C and LLVM and under ASan on both legs | this change: both foreach negatives refused at 11:13 on both backends; the 7 positives ASan-clean on both legs and print the expected output, the 4 `_copied_` ones the value read before the free. Base: the table above |
| Emitted C of `list_get_set_copied_valid` | `name = StringConcat(pgy_list_get_string(&items, 0), "")`: the copy lands at the definition |
| Lifetime fixtures (`collection_field_lifetime`, `aggregate_storage_transfer`, 868) under ASan, `/root/fx_all.sh` | verdicts identical to base (0 diff lines; 294 refused). The only ASan reports are the two endless-recursion stack overflows (`inout_cycle_*`), as on base |
| MIR verdict sweep, native `--mir-json`, 4488 programs under `tests/`, 300 s limit | 25 files differ. 19 are existing Array refusals with the same exit status, MIR and position whose text changed to the new wording. 6 are the List fixtures (under their names before the rename): the two foreach forms refused, the four `let`/kept forms admitted with new MIR (the landed copy). Nothing else changed apart from runs that hit the limit (next row) |
| The 16 programs (large self-host probe fixtures) that hit the 300 s limit in either run (15 base, 13 new), rerun on both with a 1200 s limit | all 16 admitted on both, with byte-equal MIR. So no program under `tests/` changed verdict or MIR apart from the List fixtures, and none changed anything but wording among the other refusals |
| `examples/` MIR verdict sweep, 267 programs | identical verdicts, MIR and first errors |
| `make test-semantic`, `make test-transpile` | 3404 passed, 0 failed; 1035 passed, 0 failed |
| `tests/repo/docs_truth_size_gate.py` | PASS |

## Probes beside the gate

| Program shape | Base | This change |
|---|---|---|
| `let name = names[0]; ArraySet(...); Log(name)` (Array, exact source) | refused at the read | refused at the read |
| `let name = view[0]` from a `Slice<String>`, `ArraySet`, `Log(name)` | admitted, copy landed, ASan clean | same |
| `let name: String = ListGet(...)`, `ArrayPush(out, name)` into an Array that owns its elements, `ListSet`, `Log(out[1])`, `ArrayDropOwnedStrings(out)` | admitted, `pgy_array_push_owned_String` (a copy), ASan clean | same |
| `ArrayPush(out, ListGet(...))` or the same through a `let`, into an Array with borrowed elements, then `ListSet`, `Log(out[0])` | UAF on C and LLVM | UAF on C and LLVM |
| `Row { name: ListGet(...) }`, `ListSet`, `Log(row.name)` | UAF on C and LLVM | UAF on C and LLVM |
| `ArrayPush(out, view[0])` from a `Slice<String>`, `ArraySet`, `Log(out[0])` | UAF on C and LLVM | UAF on C and LLVM |
| `return ListGet(items, 0)` of a local List | ASan clean | ASan clean |
| a String kept from a List in an inner block, read after it | ASan clean | ASan clean |
| `ListGet(bag.items, 0)` and `ListSet(bag.items, ...)` on a List field | refused: slot escape analysis fails closed | same |
| `let name: String = ListGet(items, 0)`, then a call `Replace(items)` whose `inout` formal takes `ListSet`, `Log(name)` | UAF on C and LLVM | UAF on C and LLVM |
| `let name = names[0]`, then a call whose `inout` formal takes an owned `ArraySet`, `Log(name)` | UAF on C and LLVM | UAF on C and LLVM |

The three stores that still read freed memory are the decision's "Not
covered" item. The Slice row shows the gap is in the store checks for
unknown-source loans, not in List retirement. The two `inout` rows are a
separate gap for Array and List alike: the loan owner retires loans only at
a direct builtin call on the receiver, not at a call whose callee frees an
element of the storage it takes.

## Not run

- Required CI, the codegen bootstrap, gen2 == gen3 and the installed-driver
  jobs.
- The canonical DRV-2 input. It contains no `ListSet` or `ListRemove`
  call (`src/self_hosted` has no `List<String>` value at all), so this
  change has nothing to retire there.
