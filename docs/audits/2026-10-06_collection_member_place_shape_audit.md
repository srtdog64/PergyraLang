# Collection Member-Place Shape Audit

Date: 2026-10-06 (Asia/Seoul)

Status: read-only audit record. It does not own compiler semantics, registry
status or a successor rung. The executable gate
`tests/self_hosted/parity/collection_member_place_rules.sh` and the owners named
below are authoritative.

## Why this audit exists

Closing the last bootstrap refusal needed an adapter: `IntentSubjectSlotSelect`
could not read `declarations.field_identities.field_names`, a two-level member
path of a ref formal. Instead of adding more adapters, every way an array can
reach a policy function was checked against every way its storage can be
used.

Objective card:

- Objective: one storage place, a root binding plus an exact field path at
  any depth, gets the same ownership verdict whatever shape carries it.
- Priority: soundness first (no use-after-free admitted), then precision (no
  refusal of a read that cannot dangle), then patch size.
- Fact owners: `ast_collection_member_place_owner.pgy` (place identity) and
  `ast_collection_member_place_use_owner.pgy` (per-occurrence
  classification), `ast_string_formal_borrow_owner.pgy` (text formals).
- Last consumers: the member-transition pass, the release-plan check in
  `ast_collection_ownership_scan_owner.pgy`, formal element-use effects, and
  the collection mutation receiver walk.
- Forbidden fallback: adapters or signature reshaping that only move code out
  of the analyzer's view; grants from a nominal type or a parameter mode alone.
- Gate: `collection_member_place_rules.sh`, C and LLVM, 15 positives and 20
  refusals.

## The root cause

Ownership facts were keyed on an `Array<String>` binding: a local, or a
direct array formal. A field path (`t.names`, `o.t.names`) was not a storage
place. So the same array got different verdicts by shape:

| Shape | Direct array | Through a struct field |
|---|---|---|
| Readonly reader of a nested path (`o.t.names`) | n/a | refused (too strict) |
| Borrowed element returned by a callee, then the aggregate released | refused | admitted (use-after-free) |
| Borrowed element pushed by a callee into an out array, then released | refused | admitted (use-after-free) |
| `let p = t.names[0]`, release, use `p` | refused | admitted (use-after-free) |
| Element pushed into a local array, release, read it | refused | admitted (use-after-free) |
| Element captured by a constructor, release, read it | refused | admitted (use-after-free) |
| Element through an identity function, release, use | refused | admitted (use-after-free) |
| Inout root: bind element, release in the same body, use | n/a | admitted (use-after-free) |
| Extract a nested table, release it, restore, use an earlier element | n/a | admitted (use-after-free) |
| Element passed to a user `String` formal that only compares it | refused (too strict) | admitted |
| Field mutation through a plain struct parameter | refused for a plain array | admitted |

Every admitted use-after-free row was already present on origin/main
`49f8eaf1` (checked with an observer built from that tree). Arrays grow with
`realloc`, and the release frees each owned element, so each row is a real
dangling read, not a style issue.

## What changed

1. Member places. `SemanticAstCollectionMemberPlaceForNode` walks a member
   chain to its root and types each selector through the constructor field
   table. A generic or unresolved step keeps only the root, so consumers fail
   closed on that root.
2. Release-side element borrows. The member-transition pass classifies each
   occurrence. A String element read through a member place stays inside its
   expression only as a comparison or concatenation operand, a borrowed
   builtin argument, or an argument to a borrow-only text formal. A nominal
   sub-place stays only as a selector prefix or a user-formal lending. Every
   other use is an escape of the root, and lending edges carry a callee's
   escape to its caller. A root that a release plan protects with an escape
   is refused as `aggregate_release_element_borrow`, at the first borrow
   site. Code that never releases the aggregate is unaffected, which is why
   this rule refused one site in the whole bootstrap source before rule 4.
   Binding a direct field (`let tables = outer.tables`) is a move owned by the
   existing carrier and restoration owners, not an escape. Storing an element
   into a field of its own root (the codegen type-environment epochs do
   `state.local_rows = state.owned_epochs[k]`) stays inside that root's
   lifetime and is not an escape either. A borrowed element does not block
   nested reads: it neither aliases nor writes a descriptor.
3. Nested reads. A nested `Array<String>` place of a readonly formal or a
   local may be lent to a readonly reader, indexed or measured while no
   occurrence of the root or of a path prefix can alias, write, consume or
   release it. A readonly sub-place lending to a callee that may alias its
   formal blocks the root's nested reads too. One-level rules are unchanged.
4. Borrow-only text formals. A default or ref `String` formal of a
   synchronous body with no writable or consuming formal is borrow-only when
   each occurrence is a comparison or concatenation operand, a borrowed
   builtin argument, or an argument to another borrow-only formal (fixed
   point). Interpolation lowers `${x}` to `ToString(x)` inside a
   concatenation; `ToString` of a String may return its argument, so its
   result stays the same borrow and its own consumer decides. Formal
   element-use effects, member escapes and this summary share one consumer
   predicate.
5. Plain parameters are readonly roots. `docs/mut_borrow_parameters.md` says
   value semantics are not permission for untracked alias mutation. The
   receiver walk refused field mutation only under a `ref` root; a plain root
   now gets the same `value_param_collection_mutation` (mode `default`). Two
   functional-update owners that `ArraySet` a plain `build` parameter take
   `own build` instead; their callers already reassign the result.
6. The `IntentSubjectSlotSelect` adapter is removed; the call reads the
   nested path directly again.

## Census

Release observers of this tree over every bootstrap closure (lexer, parser,
semantic, fuzz, mir_lower, codegen and the 14 tools) report no refusal. A
temporary build that listed every protected root with an escape, not only the
first, found 21 roots before the same-root rule, all in the codegen
type-environment epoch protocol, and none after it. The only other hit, in
`ast_destructure_binding_fact_owner.pgy`, was an element passed to
`SemanticError2`, which only interpolates its text; borrow-only text formals
admit it.

## Still open

Copy-out aliasing. `let copy: T = place` shares the descriptors of `place`.
Mutating the copy can reallocate a buffer the source still names. For
`Array<String>` locals the existing member-move and binding-move owners track
this. For every other collection type and for struct copies nothing does. The
source has 541 non-String array copies and 4 struct copies that are mutated
after the copy, almost all in the copy, mutate, restore idiom. A blanket
refusal would break them; the sound fix is to extend member-move tracking
(move, then restore before any other use of the source) to every collection
and aggregate type and to nested paths. Its falsifiers are four shapes that
are still admitted: pushing through a whole struct copy of a local and then
reading the original; the same copy taken from a ref formal; moving an
`Array<String>` field out of a ref formal and pushing to it; and pushing
through a copy of a nested sub-struct of a ref formal.

A text formal of a callee that also takes a writable or consuming formal is
not borrow-only. That is deliberately conservative: such a callee could
release the storage the element points into.

A String field that aliases an owned element of the same root (the epoch
`local_rows` view) is not checked after a release of that root. The epoch
owner keeps that discipline by retiring the whole scope state; a rule for
reads of an aliasing field after a release is a separate obligation.

Line caps: `ast_collection_formal_use_owner.pgy` (426 → 437) and
`ast_collection_ownership_member_transition_owner.pgy` (181 → 220) were
re-pinned at measured size. The transition owner should hand its
per-occurrence recording to the place-use owner when it is next split.
