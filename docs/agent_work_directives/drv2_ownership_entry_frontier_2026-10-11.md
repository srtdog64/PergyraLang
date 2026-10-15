# DRV-2 ownership-entry frontier: sink inference for the remaining entries

Status: **ACTIVE — R, E, S, T, G and B land; L, M and the remaining sink
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

Last-use refinements (B, after G). Each rule hands the case to a check that
already runs, so no reader of the old storage goes unseen:

- A copy holder, copy source or sibling holder declared at or after the use
  holds only copies made there or later; older storage reaches it only
  through a later read of the root, an alias or an older holder, which the
  scan and the holder checks still see.
- A statement in another branch of an `if` or `match` than the use, with no
  loop around both, is not reached from the use. A loop around both keeps
  the read (`exclusive_other_branch_loop_negative`).
- The target of a whole-binding write (`x = v`, `let x = v`), the consuming
  statement's own included, is no read of its old value; a member or
  indexed write still is (`exclusive_alias_element_write_negative`).
- The binding the consuming statement wholly writes names its new value in
  later statements; at the statement itself it is still a reader, so
  `w = F(place, w)` stays checked.
- A local definition of a place that no path reads (every path writes the
  whole binding first or leaves its block) aliases nothing
  (`ast_collection_exclusive_value_dead_definition_owner.pgy`). A statement
  shape the cover does not model counts as a read; a path that keeps the
  default refuses (`exclusive_dead_alias_partial_negative`).

Shape this admits: the instruction-core admission, which threads an inout
expression set through a growing call, takes the admitted set as a default,
and overwrites it in every match branch. Fixtures: eight new (63 in the
suite), 32 rows, one hundred twenty-two diagnostic locations. Every earlier
fixture keeps its verdict (683 inputs compared against the previous probe).
No annotation or copy was added to the corpus.

Corpus hazard fixed with B (introduced by the S/T slice): the sink-entry
check classified its argument into `let copied = copies;`, a value copy of
the copies map. A HashMap value copy shares its buckets, so a `MapSet` that
rehashed through the copy retired storage the facts still read. The map now
moves out of the facts for the check and back before any read, and the
check records into that one live map. The same hazard is outside the
exclusive-value model today: growth summaries count ArrayPush, ArrayPop and
ArrayPushOwnedString, but not `MapSet` or `MapRemove`, although a HashMap is
growable storage (AGENTS worker rule). Counting map growth is the open
obligation this case falsifies; the comparison with Valen group borrowing
that names it is `audits/valen_group_borrowing_comparison_2026-10-11.md`.

Local gate note: `bin/pgy.exe` (2026-10-06) predates the lexical array type
fix of `2b08d86d`; with it the LLVM identity probe reads `modes[observed]`
from an `Array<Int>` as a string (the probe reuses the spelling `modes`) and
crashes on mutation 37. The inout gate is evidence only with a compiler built
from the current C sources (`PGY_BIN`); CI builds its own.

B evidence: the inout-effect gate passed end to end (C and LLVM) with
`PGY_BIN=.tmp/claude-bin/pgy.exe`, in a local copy whose only change raised
the 60 s identity unit to 240 s (it took 66-76 s here, as it does at HEAD);
the committed gate keeps 60 s. Borrowed-descriptor-read, component contract
and source-scan passed unchanged. All 683 earlier fixture inputs keep their
verdicts against the pre-B probe; of the eight new fixtures the four
positives are admitted and the four negatives refused. The recount with the
copies fix is the same 97 sites.

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

## Recount after B

97 refusals (from 101): the four option-match member mutations
(`MirExpressionGraphSequenceAppendUnaryCall`) are admitted; no refusal is
new. `unproved_exclusive_sink_entry` 27, `unproved_indexed_read_entry` 21,
`unproved_formal_indexed_read_entry` 9, `owned_string_actual_unproved` 9,
`unproved_formal_element_use_entry` 7, `unproved_exclusive_member_mutation`
7, `unproved_shallow_mutation_entry` 4, `owned_string_drop` 4, `ArrayPush`
4, `owned_element_escape` 2, three singletons. The instruction-core sinks
(lines 27, 59, 67) are not the core's own shape any more: the class of the
inout set turns shared in the third round of the fixed point through shared
result fields of `DirectMirScalarProgramAppendExpression` that reach
`admission.facts`. No whole value in that chain is shared first, so the
origin is a shared first field inside the direct-MIR expression admission
(the same owners as its indexed-read cluster); that field is the next
falsifier to locate.

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

## Handover continuation: collection structural mutations (GPT)

Status: IMPLEMENTATION IN PROGRESS, base `23d1e65f`. The B checkpoint is
preserved; main is the only production editor. This is the existing DRV-2
ownership-entry rung, not another memory-model or SoT track.

- Objective/priority: reject descriptor-copy mutation hazards before reducing
  the remaining driver refusals; preserve B's last-use and restore rules,
  admitted identities, no additional caller annotation/copy, and gate budgets.
- Reached chain: source-to-MIR driver -> admitted expression/call identity and
  exclusive-value facts -> mutating-formal fixed point -> ordered ownership
  scan -> call argument and builtin mutation admission -> body bundle verdict.
  C and LLVM compile these Pergyra owners into the source probes; DRV-2 must
  consume the same verdict. No driver-name exemption or backend-local policy.
- Owners/changes: one bounded operation owner classifies admitted builtin
  mutations; growth consumes it for map receiver/field propagation, and the
  existing mutation admission consumes it for receiver exclusivity. MapNew
  is fresh only under its admitted builtin identity. Existing last-use,
  copy/field classes, sink-entry and restoration remain the only proof of
  descriptor exclusivity. Set/List/Queue mutation has no new value model and
  refuses an unproved exclusive receiver.
- Effects: MapSet may relocate backing and replace entries. MapRemove is a
  conservative structural invalidation (entry removal), not a claim that the
  runtime always reallocates. The current summary requires exclusivity for
  both. Returned payload-view lifetimes are a separate existing obligation.
- Forbidden paths to remove: Array-only builtin mutation enumeration at the
  growth consumer and silent acceptance of map/Set/List/Queue mutation just
  because their descriptors are absent from Array<String> origin rows.
- Order: record/analyze negative fixtures on B; connect operation effects and
  receiver admission; replay map restore positives and the full existing
  corpus; run unchanged inout-effect C/LLVM gate and structural gates; recount
  fixed DRV-2; only then investigate its shared direct-MIR first field.
- Integration owner/gate: main, `collection_inout_effect_owner.sh` with a
  current-source native compiler. Its compiler-scale identity unit remains
  60 seconds. Source-admission probes never execute unsafe fixture programs.
  Budgets remain 60 s static, 5 min focused and 30 min integration shards.
- Falsifiers: map copy -> insertion -> old descriptor read; forwarded map
  mutation; removal through a shared descriptor; shared Set/List/Queue
  receiver; foreign/user target identity on the MapSet spelling (source-level
  builtin names are reserved); field move-out/restore and threaded
  exclusive map positives. Missing/unconverged summary remains a refusal.
- Evidence: baseline/candidate source and executable hashes, actual verdicts,
  integration and CI outcomes will be recorded after execution. This plan
  alone changes no CLOSED status or substitution count.

### Continuation observations (2026-10-12; not a closure receipt)

- Private native compiler: SHA-256
  `103470c2ee4d14c9b52dc2534a926e487462a1db7b35f62eb2f430ee840a9fc3`.
  Installed binaries are untouched. Source admission inputs were never run.
- The unchanged-budget inout-effect gate completed on C/LLVM with 522 source
  inputs per backend and four new identity guards at
  `.tmp/self_hosted/collection-inout-effect.K2ocKX`. The subsequently added
  disjoint-map-field positive/shared-map-field negative pair passed both frozen
  probes; the current script registers 524 inputs and awaits final full replay.
  B accepted all eight new safety negatives. Existing expectations were not
  lowered, including the 60 s compiler-scale identity check.
- Full structural component inventory and source-scan passed. Counted owner
  lines: exclusive values 531/565, growth 235/235, mutation 201/300,
  operation identity 16/40. No existing cap was raised. On the fixed edited
  source boundary, no caller annotation, restoration sequence, explicit copy
  or deep-drop was added. The one `ref` in the new internal operation-query
  signature denotes its read-only admitted graph view, shared by the growth
  and mutation consumers; it is not a workaround imposed on user code.
- Borrowed-descriptor matrix: the five-minute attempt has no full receipt;
  it reached the imported runtime-ABI input. Replay at its integration
  boundary, without changing the script's intrinsic conditions.
- Whole DRV-2 diagnostic census: both dev-profile analyzers ended at 1800 s
  before a complete list; hashes of the fixed source set still match. A
  300 s phase-observed dev replay reached `expression-places:start`. A
  separately built production/release-profile baseline/candidate comparison
  runs on the same 247926-node import-composed program. These are diagnostic
  profiles, not substituted dev-gate or installed-driver receipts. Do not
  report a new remaining count or source admission from a partial scan.
- Newly reached dependency: parser import membership has two SetAdd sites,
  one SetHas consumer and two inout dispatch signatures, all in
  `parser/program_parse_owner.pgy`, `decl_dispatch_owner.pgy` and
  `source_path_owner.pgy`. B admits `parser/main.pgy`; the candidate refuses
  `SetAdd(import_paths, source_path)` at the new exclusive-storage boundary.
  Any representation migration must preserve normalized path identity,
  shared-import deduplication, cycle-before-seen refusal, traversal/declaration
  order and diagnostic positions, migrate the full signature chain and
  retire its old structural gate path. This dependency is not a reason to
  admit unmodelled Set mutation. Decide the complete migration after the
  current full refusal comparison; parser parity and import negative cases
  are its acceptance/falsifying gates.
- Exact HEAD Actions: `38148691868` at `23d1e65f` is docs-only success with
  ten skipped jobs, B's `38148668461` is cancelled, and the preceding G
  implementation run `38130176872` failed. No candidate compiler-green
  Actions, installed pair, C-path replacement or CLOSED registry promotion.

### Fixed-input census and reached repair order (2026-10-12)

- The complete release-profile diagnostic scans finished on the same frozen
  247926-node DRV-2 input: B owners 97 refusal sites (1486.77 s), candidate
  owners 115 (1427.72 s). All 97 old source/callee sites remain; exactly 18
  sites are newly exposed. This is a refusal census, not source admission,
  a dev-profile gate receipt, a 3 GiB measurement or substitution progress.
  `census-inputs.final-recheck.log` validates the frozen source hashes.
- The 18 reached sites form one dependency chain: parser import-membership
  Set (one mutation and two forwarding sites); exclusive-value fact-table
  updates (twelve MapSet sites); and three forwarding calls into the
  exclusive classifier. Do not exempt these compiler routines or hide map
  growth from the summary. Resolve them before publishing the candidate.
- First executable repair: preserve the import-composition contract while
  representing the parser's private membership table as
  `HashMap<String, Int>` (`1` denotes membership). Priority is path identity,
  cycle-before-seen refusal, one composition per normalized import and exact
  declaration/diagnostic order, then representation size. `program_parse_owner`
  issues the sole table, `decl_dispatch_owner` threads/inserts it, and
  `source_path_owner` consumes it for membership. The last consumer is the
  parser's composed artifact delivered to semantic admission. No Set fallback,
  duplicate membership table, iteration over map keys or path renormalization.
  Migrate both dispatch signatures and all recursive calls together; replace
  and negatively ratchet the old structural inventory assertions. Acceptance:
  current-source parser C/LLVM parity (198 manifest cases), parser-source
  admission, normalized shared-import/cycle/missing-import observations, then
  the full unchanged-condition collection gate. These checks do not claim an
  installed-driver or compiler-green CI receipt.
- Independently diagnosed old frontier: the direct-MIR `types` member alias
  remains exclusive (`member_ready=true`), but its current descriptor is made
  unknown by a loop-bound element-escape call. Minimal scratch falsifiers
  `text_escape_descriptor_positive.pgy` and its loop variant reproduce the
  false retirement without compiler names: return borrowed literal text,
  then read the still-live array. Distinguish descriptor effects from element
  retention at their existing formal-effect owner; do not discard element
  lifetime, mixed mutation, consuming, opaque or missing-fact obligations.
  This is a reached fact-producer issue, not permission to downgrade effect 5.
- Native field `inout` is still unsupported, and the Pergyra type owner rejects
  a direct member MapSet receiver. The scratch direct-member probe confirmed
  both boundaries. Do not rewrite fact-table updates to direct field calls
  until their typed-place and native/backend consumers have been surveyed and
  admitted. Adding more manual copies/restoration is not the default repair.
- Fact-table alias repair: the family-use owner treats MapHas as a possible
  grow/retain use, so a read-only early return incorrectly requires writeback.
  The scratch `map_read_early_exit_positive.pgy` reproduces this refusal;
  `map_old_query_negative.pgy` requires rejection of a stale descriptor query.
  Mark only admitted receiver-ordinal MapHas/MapSize calls as observational
  family uses. Keep MapHas in the later-source-read scan: it dereferences
  buckets and must never be ignored after a backing move. Do not admit MapGet
  here; its payload lifetime is separate. Existing copy, exit, restore and
  field-read owners remain authoritative. Verify both falsifiers and the full
  map fact-table input, then replay the unchanged-budget integration gate.
- Integration replay `.tmp/self_hosted/collection-inout-effect.GOhDET` reaches
  all 527 C source observations and the identity/negative checks, but stops at
  the unchanged 60 s compiler-scale actual-formal unit (line 1112). LLVM has
  not run; this is not a green matrix. The current observation-type producer
  unnecessarily resolves the binding again although its callers already own
  an admitted sink place. Consume that place's local row / exact formal key
  and the same canonical type tables instead. This is owner-directed type
  projection, not permission to guess a missing type or a measured bottleneck
  claim. Retain foreign-call, unknown/member-type and stale-descriptor refusals,
  and replay the exact input at 60 s after migration of both consumers.
- Payload review: independently rerunning the C/LLVM source analyzers confirms
  that MapGet<String> -> same-key MapSet/MapRemove -> retained text use is
  still admitted. These unsafe programs were not executed. The registry already
  keeps `mapget_string_borrow_escape` OPEN. Backing exclusivity alone does not
  discharge this payload obligation; connect its result borrow and payload
  invalidation before claiming map lifetime closure.
- The full classifier input still refuses RecordReturn after the first query
  repair. Its remaining read-only forwarding reaches CopyCount's MapGet on
  `HashMap<String, Int>`. Refine the same family-use owner with the admitted
  receiver's canonical type: MapGet of an Int map returns a copied scalar and
  neither retains nor mutates that family. Do not extend this to String values,
  member paths with unknown receiver type, unbound calls or unknown generics.
  Thread the existing local/formal type into all family-read consumers; retain
  the separate later-source-read check for MapGet after descriptor invalidation.
  Falsifiers are an old map queried after copy growth and a retained scalar
  used after safe growth; the full classifier remains the integration input.

### Reached conditional / loop alias closure plan

Objective: admit the existing single-owner fact-table update chain without
compiler-name exceptions, extra copies, annotations or weakened map growth.
Priority: exact place identity, no stale descriptor read, restoration on every
reachable growing path, negative ratchet, then source size. The exclusive-value
flow/copy/consumption owners produce these facts; mutation admission and the
ordered ownership scan are the last consumers before the body verdict.

The fixed classifier input exposes three connected limitations, not twelve
independent MapSet fixes: later lexical reads after a returning mutation branch;
aliases from before a loop that are dead or disjoint inside it; and conditional
write-back inside the same growing branch rather than an unconditional write
on a path that may not grow. Private instrumentation binds each to admitted
places, families, statement IDs and the existing block/loop predicates.

Required changes, if the private candidate survives falsification:

1. Ignore a later lexical reader only when every relevant family growth exits
   the routine before it; deferred bodies and jumps that leave the block refuse.
2. Check old-alias reads on the loop backedge as well as later in source order.
   A dead alias or a different field is not itself a repeated read; a live old
   descriptor read at the beginning of the next iteration must still refuse.
3. Certify write-back after each growing family use in its own dominating,
   jump-free block, before exits or the next iteration that reloads the source.
   Do not replace that certificate with a missing-write-back exemption.
4. Keep source-read and copy-holder checks separate from restoration; querying
   a stale map or keeping an alias through growth stays a negative case.
5. Migrate every reached consumer behind these predicates, remove the blanket
   dead-alias refusal only with its replacement, register responsibility-owned
   sources/caps, and verify all twelve map mutation and three forwarding sites
   on the fixed classifier input, not just its first diagnostic.

Acceptance/falsifiers: returning-branch restoration; conditional loop restoration;
missing restoration; stale read in the next loop iteration; break/continue before
restoration; deferred reads; disjoint-field updates; then unchanged-condition
C/LLVM collection integration and a new fixed-input DRV-2 diagnostic census.
The private prototype is not production admission or a CLOSED registry row.
