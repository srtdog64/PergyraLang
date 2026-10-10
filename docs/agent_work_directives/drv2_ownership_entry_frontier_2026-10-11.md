# DRV-2 ownership-entry frontier: sink inference for the remaining entries

Status: **ACTIVE — R and E land; S developed out of tree; L and M open
(Claude, sole lane, 2026-10-11)**. Base `main @ 1c1e0308`. The user asked to make CI green
and to finish sink inference. CI run 38065600591 fails in
`backend-compare-toolchain-linux` at the same refusal as local DRV-2
(`node=125550`, boundary `ArrayPush`), so the two requests are one rung.

## Objective card

- Objective: DRV-2 (`--emit-mir-json-verified driver_bootstrap_main.pgy`)
  admits the driver source under the collection-ownership verdict. Every
  newly admitted entry is sound, and the three admitted use-after-free shapes
  below are refused.
- Priority: soundness (fail closed, holes closed) > no new `own`/`ref`
  annotation, copy or carrier > reuse of the existing entry owners >
  corpus edits only where the corpus holds a real hazard > patch size.
- Fact owners: formal effects (`ast_collection_formal_use_owner.pgy`), call
  argument verdicts (`ast_collection_call_argument_verdict_owner.pgy`,
  `..._effect_verdict_owner.pgy`), builtin transitions, indexed String
  borrows, String-formal borrow facts, and the sink consumption owner
  (`ast_collection_aggregate_sink_consumption_owner.pgy`).
- Last consumer: `SemanticAstCollectionOwnershipScanIntoVerdict` via the
  semantic body bundle that codegen `--source` and DRV-2 require.
- Forbidden: weakening or skipping the verdict for the driver; advisory
  mode; annotations or copies added to the driver to satisfy the analyzer
  without a recorded contract; admitting an entry by spelling or by
  function name.
- Gates: a focused source gate per family (positive and negative fixtures,
  C/LLVM), the existing collection gates, and DRV-2.

## How the frontier formed

The last fully green CI run is `aed8a904` (2026-09-30). That tree had 18
collection ownership owners and none of the entry rules below. Between
10-01 and 10-10 the GPT lane added about 18k lines under `semantic/`,
including every rule that refuses here, while CI stayed red in an earlier
job. The driver corpus never passed these rules. They are shadowed gate
debt, not a regression of this lane's P1b work.

## Measured failure set (fixed DRV-2 input, 239 refusals in 81 functions)

A scratch copy (`.tmp/claude-sh/p1s`) logs every scan refusal, its source
line and callee, and continues. It also logs each site that blocks a
String-sequence formal's effect. Grouped by cause:

| Family | Count | Shape |
|---|---|---|
| R: temporaries into read-only formals | ~45 | `SelfMirNoUses()` (31), `Args()`, `["",owner,"",""]` (10) into indexed-read formals; boundary `unproved_formal_indexed_read_entry` |
| S: member aliases of default aggregate formals | 59 | `let types = facts.node_types; DirectMirScalarProgramAppendExpressionNode(kinds, types, ...)` (56), then `return Set(..., types, ...)`; callers thread `facts = F(facts, ...)` |
| E: element-use entry | 80 | callee formals blocked at 55 sites: element passed to a retaining String formal (site 5), formal stored into a constructor (site 7), element stored or returned (site 8) |
| L: local indexed-read entry | 21 | local bindings into indexed-read formals after an earlier retiring site |
| M: remainder | 34 | `owned_string_actual_unproved` 9, `ArrayPush` 6, `owned_string_drop` 4, three aggregate/inout/shallow singletons |

## Admitted hazards (falsifiers, all currently `body_ok=true`)

Runtime `ArrayPush` stores the String pointer, and a push that grows
reallocates and frees the old buffer.

1. `ArrayPush(b, a[0]); ArrayDropOwnedStrings(a); StringLength(b[0])` with
   owned `a`: use-after-free through a pushed element.
2. `let b = [a[0]]; ArrayDropOwnedStrings(a); StringLength(b[0])`: the same
   through an array literal.
3. A callee pushes onto `let types = facts.types` of a default-mode formal
   until the buffer grows; the caller then reads `facts.types[0]`. The
   caller's descriptor points at freed storage. Through an inout helper the
   same shape is refused (family S); directly it is admitted.

## Decisions

- **R.** A read-only (indexed-read) formal may receive a temporary: an
  array literal whose elements are String literals or borrowed String
  formals of the enclosing routine, or the result of a call that takes no
  collection or aggregate argument. The temporary lives through the call,
  the callee only reads it, and no argument of the same call can release
  storage the temporary borrows. Element-retaining formals keep refusing.
- **S.** Doc 27 §2.3 step 7 for aggregates: a default-mode aggregate formal
  whose body mutates a member alias (directly or through a shallow-mutation
  callee) is an inferred sink. Its member aliases then carry exclusive
  storage authority in the callee. Every call edge must consume its
  argument (sink consumption owner, extended so that `x = F(x)` redefines
  `x` in the same statement). Hazard 3 is refused by the same rule. Corpus
  sites that keep an alias of a sink formal alive across its mutation (an
  `invalid` set built from `facts` before the pushes and returned after
  them) are real hazards and are edited to build the failure value from the
  current locals.
- **E.** Holes 1–2: an Array<String> local that receives an element of a
  tracked storage becomes a borrower of that storage in the indexed String
  borrow owner, and a deep drop invalidates it. Site 5: a String formal of
  a synchronous routine with no inout/own formal, whose result type and
  formals cannot carry a String, cannot retain its text past the call and
  is borrow-only. Site 8: a call that hands a binding to an
  element-retaining formal is admitted only when the binding's elements
  cannot be released (not owned, not under an aggregate release
  requirement), and the binding's later deep drop is refused. Site 7
  follows S: a formal stored into a constructor is a sink.
- **L, M.** Inspected after R, S and E land, because those change the
  states they read.

## Implementation record

**R (landed in tree, gate rows added).** `ast_collection_read_literal_argument_owner.pgy`
admits a zero-argument call result and a literal of String literals and
live place reads into an indexed-read formal. The place-read form also
requires every other formal of the call to be `ref` or of a type that holds
no String (`SemanticAstCollectionReadCallReleaseFree`).

**E (landed in tree, gate rows added).**

- Runtime facts this rests on: an ordinary `Array<String>` never frees its
  elements; `ArrayPushOwnedString` pushes a `strdup` copy; only
  `ArrayDropOwnedStrings` frees, and only owned pushes fill the arrays it
  drops (`src/runtime/pgy_runtime_builtin_storage_inline.h`). A borrowed
  String is therefore freed only by the deep drop of storage that owns it.
- Contract (shared elements): text that no release can reach may be shared
  by pointer; storage a routine releases may not share its elements. The
  removal condition is ownership-clean elaboration (doc 27 §2.3), which
  turns each sharing site into a copy or a move once the source is release
  bound.
- `ast_collection_element_escape_entry_owner.pgy` (new): released fields
  are the field ids with a DeepDrop requirement row (complete before the
  scan, because a deep drop needs a member-move binding). An element-use
  call is admitted for a caller formal that is not `own`, a local whose
  elements are not owned (an empty local becomes borrowed), a member path
  whose field name is not released, or a literal of String literals and
  borrowed String formals.
- The same owner closes hazards 1–2 and a third one found here (an owned
  element handed to a retaining user String formal, then the array
  dropped). An element of releasable storage may only be compared,
  passed to a borrowed builtin or a borrow-only String formal, logged,
  matched, or bound to a String local (which the indexed String borrow
  owner tracks). Boundary `owned_element_escape`.
- Text escape-free bodies (synchronous; result and inout/own formals hold no
  String) cannot keep text past their return. Their String formals are
  borrow-only and their element uses do not block a String-sequence
  formal, except at a moving transfer (`ArrayPushOwned`, an `own` formal).
  `ArrayPushOwnedString` copies, so it releases nothing of the caller.
- Existing retention negatives whose roots were static literals were safe
  programs under this contract. Their roots are now owned and dropped after
  the escaping call, so the same call refuses with the same boundary.
  `inout_index_role_{eq,ne}_drop` read through a Bool role method and are
  positives now. `own_{storage,event}_formal_*_unknown` keep an unproved
  callee by returning the element.

**S (developed, not landed).** The owner and its scan wiring are kept
outside the tree (C: scratchpad `ev_wip/`); the R/E landing carries none of
it. With it wired, every S fixture behaves (four threading positives
admitted; caller reuse, borrowed wrap, copy alias and both stale failure
values refused), but DRV-2 rises to 770 refusals: 437 sharing, 147 sink
entry, 73 alias growth. The analysis is flow-insensitive, and the corpus
relies on two path facts it cannot see: a field is moved out, then the
original aggregate is returned on an early path before any growth; and an
error value is built from the aggregate after a field grew (a real stale
descriptor; `SelfMirRoutineBuildError` alone has 147 callers). Next:
measure consumption from the first growth of the alias instead of from its
definition (reads inside the growth's loop count as later), then fix the
error constructors that carry grown storage.

Earlier design notes for S:

- Measured hazard scale: 121 corpus functions grow 724 member aliases of
  aggregate fields directly; the verdict admitted every one of them without
  an exclusivity proof (hazard 3), and refused the same growth only behind
  an inout helper (the 59 refusals).
- `ast_collection_exclusive_value_owner.pgy` (new): a greatest fixed point
  over every routine classifies values as exclusive or shared (header of
  that file). `x = F(x)` keeps `x` exclusive: the redefinition ends the old
  value in its own statement, while every other alias of the old value is
  still checked in later statements. A definition does not count its own
  destination as a later reader.
- Mutating formals: default or inout formals whose field alias grows, by
  call, builtin call or ArrayPush/ArraySet/ArrayPop statement, or that are
  passed to a mutating formal (fixed point).
- Scan checks: a call entering a mutating formal needs an exclusive,
  consumed argument (inout: exclusive); a field alias grows only when its
  root is an exclusive local or a non-ref formal; a growing root may not be
  handed to a result-carrying formal or a constructor while it stays live
  (the stale failure value). Boundaries `unproved_exclusive_sink_entry`,
  `unproved_exclusive_member_mutation`, `exclusive_root_shared`.

## Recount after R/E

126 refusals (from 239): `unproved_shallow_mutation_entry` 59,
`unproved_indexed_read_entry` 21, `unproved_formal_indexed_read_entry` 12,
`owned_string_actual_unproved` 9, `unproved_formal_element_use_entry` 9,
`ArrayPush` 6, `owned_string_drop` 4, `owned_element_escape` 2, four
singletons. The first stop is `mir/declaration_generic_rows_owner.pgy:35`:
locals filled with owned elements and never dropped push those elements
into an outliving inout array.

## Order and gates

R, then S, then E, then L and M. Each family lands with its fixtures in a
focused gate and an instrumented DRV-2 recount. No family is called closed
while its refusals remain or while a falsifier above is admitted.
