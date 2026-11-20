# Native checker: a String of unknown source stored past a List or a promoted Array is copied (2026-11-20)

Source: branch `claude/list-string-store-copy` on `d0953c243` (the
[List loan retirement](2026-11-20_native_list_string_loan_retirement.md),
itself on the uncommitted foreach change), plus this change. Base binary:
`d0953c243`. Tree: WSL `Ubuntu-C-WSL` `/root/pgy-lsc` (base kept as
`bin-base/pgy`); Linux gcc 13.3 with ASan/UBSan, LLVM 18. Rule:
[decision](../decisions/2026-11-20_unknown_string_store_copied_past_list_or_promoted_array.md).

## Before

On base, each of these programs compiled on native C and LLVM, read the
String after its owner freed it, and failed ASan with heap-use-after-free
on native C and on the LLVM leg (`/root/legs_lib.sh`, IR at -O0 with
`sanitize_address`):

| Shape | Release |
|---|---|
| `ArrayPush(out, ListGet(items, 0))`, `out` not owning its elements | `ListSet` |
| the same through `let name: String = ListGet(items, 0)` | `ListSet` |
| `Row { name: ListGet(items, 0) }`, or `Row { name: name }` from such a `let` | `ListSet`, `ListRemove` |
| `ArrayPush(out, view[0])`, or through `let name = view[0]`, `view` a `Slice<String>` of `names` | owned `ArraySet(names, ...)` |
| `Row { name: view[0] }` | owned `ArraySet(names, ...)` |
| `MakeRow(view[0])`, where `MakeRow` keeps its String in the Row it returns | owned `ArraySet(names, ...)` |
| `for name in names[0..1] { ArrayPush(out, name); }` | owned `ArraySet(names, ...)` after the loop |

Why, from a scratch build that traced the store walk in
`indexed_string_borrow_argument`:

- List shapes: for a String of unknown source the walk visits every
  `Array<String>` in scope as a possible owner. A `List<String>` is not
  one, so the walk met only `out` (no owned elements, skipped) and the
  store stayed shallow. With an `out` that owns its elements, the walk met
  `out` itself and copied, which is why that variant was safe.
- Slice shapes: the walk met `names` while its element ownership was still
  unknown and recorded it as a conditional non-owner
  (`semantic_indexed_string_nonowner_record`). In the first analysis the
  history proof failed (owned `ArraySet`, the slice), and the landing made
  `names`'s stores owned. In the next analysis
  `nonowner_resolved_by_transfer_or_owned_after_selected_edges` found owned
  transitions on `names` and dropped the risk as resolved, though the use
  was a store into `out`, not into `names`. No copy landed.

## Change

- `indexed_string_borrow_argument`: for a String of unknown source, every
  `List<String>` in scope is an owner that may free it. It takes the same
  copy decision as an `Array<String>` owner, now the function
  `indexed_string_store_unproven` (moved, not changed, except that a List
  has no ownership fact to mark).
- `nonowner_resolved_by_transfer_or_owned_after_selected_edges`: when the
  excluded Array has owned transitions and the routine both takes a
  `.Slice` view of it and reads a String through a `Slice` (an index of a
  Slice value, or a for-in over one), a use that is not stored into the
  Array, has no copy selected, and is not passed to a declared callee whose
  checked result neither depends on nor retains it no longer counts as
  resolved. `semantic_indexed_string_nonowner_receipts` selects an
  independent copy at each such use (or refuses: "A String that may name an
  owned element of 'x' has no independent copy"). The view rule is the new
  TU `indexed_string_nonowner_view.c`.

A first version reported every such use on promotion alone, without the
view condition. It fixed the same shapes, but on the `tests/` sweep it
changed the MIR of 73 programs beyond the new fixtures (the 5
`nonowner_risk_*_landed_valid` fixtures, one `raw_view_ownership` probe and
67 self-host probe fixtures) by copying arguments of declared callees (one
fixture: 5,924 changed lines of C). The view condition was added for that
reason; with it those 73 programs are byte-equal to base again (rechecked
one by one, then by the sweep below). The self-host compiler takes `.Slice` views (for example
`bindings.Slice(0, n)` handed to a callee in
`SemanticExpressionGraphGenericCallFactCaptureFromResolvedTarget`) but
reads no String through one in those routines.

Objective card: no admitted program reads a String that a List or Array
release freed after it was stored. Fact owner: the indexed String store
owner and its non-owner receipt. Last consumers: the `ArrayPush` landing
(`ArrayPushOwnedString` or a value `Clone`), the shallow-store and
constructor `Clone` landing, the named-call record. Forbidden: an exact List
source, a copy per `ListGet`, a refusal where the 2026-10-19 rule copies.

## Results

| Check | Result |
|---|---|
| `unknown_string_store_copy_smoke.sh` (new; 10 programs, C and LLVM, expected stdout; a filler String allocated after the free makes a stale read print it) | this change PASS. Base FAIL at `list_get_pushed_then_set_copied_valid (c)`: printed `77 77 1`, expected `5 77 1`. On base the 9 shapes print `77` for the stored String on both backends; the tenth (`out` owning its elements) prints `5 77 2` on both binaries |
| The 9 shapes above, under ASan on the native C and LLVM legs | this change: admitted, ASan-clean, print the value read before the free. Emitted C: the List pushes become `pgy_array_push_owned_String`, the fields `StringConcat(x, "")`, the Slice uses and the retaining callee's argument `StringConcat(x, "")` |
| `foreach_string_borrow_refusal_smoke.sh` | PASS (11 refusals, 12 admissions, C and LLVM) |
| Lifetime fixtures (`collection_field_lifetime`, `aggregate_storage_transfer`, 868) under ASan, `/root/fx_all.sh` | verdicts identical to base (0 diff lines) |
| MIR verdict sweep, native `--mir-json`, 4498 programs under `tests/`, 300 s limit (base: the first run plus the 2 fixtures added since) | no verdict changed. MIR changed for the 9 new `_copied_` fixtures (the landed copies); the tenth (`out` owning its elements) is byte-equal. Every other diff line is a run that hit the limit in one of the two runs (next row) |
| The 32 programs that hit the limit in either run, rerun on both at 1200 s | 30 admitted on both with byte-equal MIR. `driver_rung2_mutable_analysis_reject` and `inout_array_storage_source_mir_probe` hit 1200 s on both under load; run alone with a 3600 s limit they finish in about 510 to 540 s, admitted on both with byte-equal MIR. So no program under `tests/` changed verdict or MIR apart from the 9 new fixtures |
| The 20 sources the codegen bootstrap compiles natively (codegen, MIR lower, parser, semantic, lexer, fuzz generator, 14 tools), `--emit-c` | admitted by both, byte-equal C |
| Canonical DRV-2 input (`driver_bootstrap_main.pgy`), `--mir-json`, run beside other jobs | base: exit 0, 0 errors, 2,597,312 KiB peak, 53:38, 458,805,685 bytes `f8f72615e2bf`. This change: exit 0, 0 errors, 2,597,656 KiB, 39:36, byte-equal |
| 23 probe programs beside the gate (table below, and the `MakeRow` and Slice for-in shapes now in the gate), emitted C base vs this change | byte-equal except those two shapes and the four table rows whose last column names a change; every admitted one ASan-clean on both legs |
| Binaries | base `bin-base/pgy` sha1 `f88a1928…`, this change `bin/pgy` sha1 `650a7537…`; tree files compared equal to the commit before the runs |
| `make test-semantic` | 3404 passed, 0 failed |
| `make test-transpile` | first 1030 passed, 5 failed, all in `nested specialized collection signatures emit type defs before prototypes` (base: 1035 passed). Its `RenderBuckets` returns `ListGet(loaded, 0)` with a `List<String>` local; this change copies that return in a landing round (the driver on the same program: base returns the element, this change `StringConcat(element, "")`), and the test lowers with `lower_program_to_mir_strict`, which runs no landing round, so lowering stopped. The test's subject is prototype order, so its body now returns `Concat(ListGet(loaded, 0), "")`. Then 1035 passed, 0 failed |
| `make test-hir`, `test-rir`, `test-dir`, `test-air`, and the `test-mir` unit binary | 58, 44, 15, 163 and 754 passed, 0 failed. The smoke `test-mir` runs next needs the installed self-host driver, which this tree does not build |

## Probes beside the gate

| Program shape | Base | This change |
|---|---|---|
| `ArrayPush(out, names[0])` (exact source), owned `ArraySet`, `Log(out[0])` | owned push, ASan clean | same C |
| `Row { name: names[0] }` (exact source), owned `ArraySet` | `StringConcat` copy, ASan clean | same C |
| `let name = view[0]`, owned `ArraySet`, `Log(name)` | copy at the definition, ASan clean | same C |
| `let name = names[0]`, owned `ArraySet`, `Log(name)` | refused at the read | same |
| `ArrayPush(parts, view[0])`, owned `ArraySet(parts, ...)` | owned push into `parts` | `StringConcat` copy, then a plain push; same output, ASan clean |
| `ArrayPush(out, ListGet(items, 0))` with no release after it | shallow push, ASan clean | owned push, and `out` is dropped with `pgy_array_drop_owned_String` (the cost the decision names) |
| `return ListGet(items, 0)` of a local List | returns the element | returns `StringConcat(element, "")` |
| an unrelated field read pushed beside a List, or beside a promoted Array | owned push | same C |
| `let name: String = ListGet(...)` read only; a String kept from a List in an inner block | ASan clean | same C |
| a List parameter's element pushed into a returned Array | refused (Array return carries the parameter's lifetime) | same |
| `Shout(view[0])`, a declared callee returning `Concat(name, "!")` | ASan clean | the argument is copied (the checked relation reports that the result depends on it); same output |
| `First(names)` or `FirstOf(names[0..2])` (a callee returning an element) pushed or kept in a field, then an owned `ArraySet` | ASan clean | same |
| a view taken in an inner block, its element kept and pushed after the block | ASan clean | same |

## Not run

- Required CI, the codegen bootstrap, gen2 == gen3 and the installed-driver
  jobs. The 20 sources the bootstrap compiles natively and the canonical
  DRV-2 input are byte-equal to base (Results).
- A loan held across a call whose `inout` formal frees the element stays
  a gap (decision, "Not covered").

## Size

Counted lines (`scripts/source_size_count.py`): `indexed_string_borrow_owner.c`
822 to 852 and `indexed_string_nonowner_risk.c` 1446 to 1471, both over the
weekly `semantic-tu-size` cap of 599 before this change (that gate fails on
base); the new `indexed_string_nonowner_view.c` has 86.
