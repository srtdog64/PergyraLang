# DRV-2 ownership-entry frontier: sink inference for the remaining entries

Status: **ACTIVE — R, E, S, T and G land; L, M and the remaining sink
entries open (Claude, sole lane, 2026-10-11)**. Base `main @ 1c1e0308`. The user asked to make CI green
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

**S (landed in tree, gate rows added).** Exclusive values are inferred by six owners
(`ast_collection_exclusive_value_{,consumption_,field_read_,copy_,flow_,mutation_}owner.pgy`).
Values stay flow-insensitive; the path facts the corpus relies on are each
modelled with a falsifier in the inout-effect gate:

- Field-read summaries of aggregate formals, a fixed point over forwarding,
  by first field and by full member path: a whole value entering a formal
  that never reads the grown storage does not overlap it. `!` marks a
  formal whose body keeps or changes what it reads; `*` a whole use.
- A definition writes a local or a formal; that destination and its alias
  family hold the moved value, and a later read of a path the family never
  touches is not stale.
- A read that copies no buffer is no reader: a scalar or String field, an
  array length, or, while the consuming call still evaluates its arguments,
  a scalar or String element or a read-only formal returning a value.
- Exit scopes (a block ending in a function exit with no leaving jump), a
  return before every non-read family use (later, or in another branch of
  the same `if` or `match`, and no loop carrying a use to it), write-backs
  every path runs once no family use follows them, loop rebinding, and the
  inout copy-out at every exit, per growth scope. A function with a deferred
  body has no forward order and refuses.
- Copies: a definition that copies a place inside a call or constructor
  without consuming it holds the source's dependencies; each later use of
  that holder, the source, or another holder of the source reads the same
  storage. A plain alias, or a holder that escapes (push, statement call,
  match), shares its source instead. A reassigned formal joins every value
  assigned to it; `Array<Int>` is tracked.
- After a whole rebinding of the consumed place, only aliases taken before
  it name the old value, and a copy holder rebound on every path before its
  next read holds a new value (`exclusive_holder_rebind_*`,
  `exclusive_rebind_branch_stale_negative`).
- An exclusive member alias grown through an inout formal is written back
  each time, so its read and copy entries check only descriptor-retiring
  sites (`exclusive_alias_inout_then_read_positive`,
  `exclusive_alias_read_shared_root_negative`).
- Last-use proofs are memoized by the copy edges of the place's root (its
  holders, the sources it holds and their other holders; edges are only
  added) instead of a program-wide epoch. On the inout-effect gate's
  compiler-scale unit the fixed point drops from 8.0 s to 3.9 s and the
  unit from about 53 s to 47 s; its timeout is 60 s, and the HEAD tree
  took 56 s on the same machine.

Corpus fixes, each a real hazard the analysis exposed:

- Error values carried the failing state, whose rows a growth may already
  have released: `SelfMirRoutineBuildError` (the last consumer retires failed
  builds, so a double free), `SelfMirProgramFactsError`,
  `MirExpressionGraphSequenceError` and
  `DirectMirScalarProgramExpressionSetInvalid`. Each now carries only its
  diagnostic in fresh storage; every consumer checks `ok`/`valid` first.
- `SemanticAstMatchMaterializationFactsAttach` read the admitted graph in
  the loop that grew its aliases (a use-after-free once a second scrutinee
  is materialized and a push reallocates) and returned the stale graph on
  failure while its caller keeps reading it. It reads roots first (without
  copying the graph), appends second, and writes back on every exit.
  `SemanticAstIterationSyntheticGraphRootsAttach` and
  `AstTreeArtifactBindDestructurePayloads` returned stale storage on
  failure after a growth; both now carry the grown storage on every exit.
- `DirectMirScalarProgramAppendExpression` rejected with the caller's set
  after its node aliases grew (31 sites in the node loop and three after
  it), and the option-int try operand did not take the returned set back on
  failure; both callers kept stale descriptors. Every failure now hands back
  the set the aliases form (marked invalid), both callers write it back, and
  an unrecognized literal hands the set back as the populated-literal
  admission already did.
- Two inline `SelfMirExpressionGraphUses(build, ...)` arguments move into the
  preceding statement, the idiom every other site uses; a negative
  statement-fact self-test now writes its field aliases back explicitly.

Open, recorded: member aliases are tracked only for `Array<String>` locals
whose path has one step from a binding, so growth through `Array<Int>`
aliases (`s_int_array_copy_negative` stays admitted) or through a nested
path (`let kinds = arena.topo.kinds`) is unchecked by this verdict.
`p_exit_scope_add_local_positive` is refused by the member-move owner
(`member_use_after_move`), which has no exit scopes.

**T (landed in tree, gate rows added).**
`ast_string_formal_text_containment_owner.pgy` (new): an `Array<String>`
formal of a body whose result or writable formals may hold String keeps its
element text in the frame when every text source (an element of the
formal or of a contained local array, or a contained String local) is read
by a primitive or borrowed consumer or a borrow-only String formal, binds
or overwrites a String local, or enters `ArrayPush` (call or statement
form) of a local array that starts empty and is only pushed, measured and
indexed. The formal-use inventory asks it only for a formal whose sole open
use is the text's escape. `borrow_formal_retained_owned_seed_negative`
pinned the signature-only rule on a safe program (the bound element is only
compared and copied by `ArrayPushOwnedString`); that shape is now
`borrow_formal_bound_owned_seed_positive`, and the negative retains the
element for real (a raw push into the inout destination, read after the
source's deep drop). Falsifiers: `owned_element_text_escapes_{result,
string_local,array_alias}_negative` refuse at the caller;
`owned_element_text_contained_{local_array,string_local}_positive` admit.

**G (landed in tree, gate rows added).** Exclusive classes per first field
(`ast_collection_exclusive_value_class_owner.pgy`): a value is exclusive,
shared at some first fields, or shared, and each dependency on a formal
records the first field of the value it reaches and the first field of the
formal it comes from. A constructor lands each argument at its field; a
result records which formal field lands at which result field and which
result fields are shared; a local records its shared fields and dependency
paths. The growth owner (`ast_collection_exclusive_value_growth_owner.pgy`)
summarizes the first fields each formal and root grows; a rebound alias and
a dependency grow any field. Then:

- a sink needs exclusivity and a last use only of the fields it grows: a
  holder of another field, and a formal source whose copies land only at
  fields the sink never grows, are not readers of grown storage;
- a place whose only later readers are copies it shares moves, and is
  shared only at the fields those copies reach
  (`ast_collection_exclusive_value_copy_landing_owner.pgy`);
- a root may be handed to a carrying formal at a field it never grows, and
  a definition that rebuilds the root itself retires the old value;
- a member alias may grow when its root's field (not the whole root) shares
  no storage.

Falsifiers: `exclusive_grown_field_{shared,holder}_negative` refuse at the
sink; `exclusive_grown_field_{disjoint,holder,ref_holder}_positive` and
`exclusive_self_rebuild_positive` admit. The 26 earlier rows keep their
exact first refusals.
- ArraySet and an indexed assignment write in place and retire no buffer,
  so they are not growth; only ArrayPush, ArrayPop and callee effects that
  may reallocate are.
- A dependency formal grows where its path lands in the growing root, and
  a copy edge into the binding the consuming statement itself writes is the
  move's destination, not a reader of the old storage.

Corpus hazards fixed with G: `AstTreeArtifactBindExpressionOwnerKinds` read
an atom through the arena whose atom table its loop grows (a stale read once
a push reallocates) and failed with the pre-growth artifact; it now reads
every row through the aliases it extends and hands back the arena they form.

Gate re-pin: `aggregate_owned_push_leaf_adapter_alias_negative` is refused
first by the exclusive sink entry (its alias copy shares the storage the
call grows) instead of `aggregate_release_incomplete`.

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

## Recount after G

101 refusals (from 112): `unproved_exclusive_sink_entry` 27,
`unproved_indexed_read_entry` 21, `unproved_exclusive_member_mutation` 11,
`unproved_formal_indexed_read_entry` 9, `owned_string_actual_unproved` 9,
`unproved_formal_element_use_entry` 7, `unproved_shallow_mutation_entry` 4,
`owned_string_drop` 4, `ArrayPush` 4, `owned_element_escape` 2, three
singletons. No refusal is new. Open shapes: callers that grow a sequence's
rows through two-step aliases and then hand the pre-growth sequence to
`MirExpressionGraphSequenceFromExtendedRows` (the option and tagged-enum
match owners; a signature change that passes the rows instead made the
callee mutate a value formal and was withdrawn), the driver entry points
that hand an artifact and the analysis built from it to one sink, and the
direct-MIR expression admission's node aliases read after an inout growth.

## Recount after S/T

112 refusals (from 126 after R/E, 138 with the first S wiring):
`unproved_exclusive_sink_entry` 30, `unproved_indexed_read_entry` 21,
`unproved_exclusive_member_mutation` 12, `unproved_formal_indexed_read_entry`
9, `owned_string_actual_unproved` 9, `unproved_formal_element_use_entry` 7,
`exclusive_root_shared` 7, `unproved_shallow_mutation_entry` 4,
`owned_string_drop` 4, `ArrayPush` 4, `owned_element_escape` 2, three
singletons. Most sink entries and all seven root-shared refusals hand an
aggregate that holds a copy of another value's storage in one field (the
program facts hold the analysis graph) to a sink that grows other fields;
the next step (G) gives exclusive classes per first field and summarizes
the fields each sink grows.

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
