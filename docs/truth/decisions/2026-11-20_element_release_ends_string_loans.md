# 2026-11-20: an element release ends the String loans of its array

Decided by: the Claude lane, from the user's request to refuse an owned
String pop while a borrowed element alias is live. The user can overturn it.

## Rule

An element release frees String elements of an `Array<String>`:

- an owned `ArrayPop` frees the last element (collection receipt effect 5);
- an owned `ArraySet` frees the element it replaces (receipt effect 4);
- a call frees elements of an argument whose formal has an element
  requirement: the callee pops or deep-drops that formal, directly or
  through a forwarding call.

The receipt producer and the requirement owner decide that an element is
released. The self-hosted front end then ends the String loans that the
release may free, and refuses (`borrow_boundary_escape`):

- a later read of an ended loan (`indexed_string_use_after_element_release`);
- a loan the releasing call, or a call enclosing it, took as an earlier
  argument (the same boundary), unless a compiler Clone selected for that
  argument copies it where it is taken;
- a loan declared outside a loop and read in a loop that releases
  (`indexed_string_loop_use_after_element_release`). Overwriting the whole
  local is not a read.

Which loans end:

- A direct release (pop or set in the routine) ends, as a direct deep drop
  does, every loan of the released storage, every loan whose source is
  unresolved or joined, and every loan when the released storage is
  unresolved (a formal, or a local with no proved storage).
- A release inside a callee ends only the loans proved to borrow the one
  known storage it is handed, as the own hand-off to a deep-dropping callee
  did before.
- A proved-independent String and a map payload loan are never ended.

A pop or set takes effect after the releasing statement's last expression
lane, so a value an owned set stores is read before the element it
replaces is freed.

## Three loan sources the release needed

Each makes a loan proved independent that the lattice called unresolved.
None weakens a release that can reach the storage.

- **A value a selected compiler Clone copies** is fresh text. Every route
  that compiles the program carries that Clone; the direct-MIR routes refuse
  a landing they cannot carry. Without it,
  `named_string_field_lineage_same_call_retire_grow_landed_valid.pgy`,
  which the default C route runs clean with the Clone at
  `MakeRecord(rows[0])`, was refused.
- **A place read through a `ref` formal** borrows the caller's storage. No
  release in the routine can free it: the routine cannot release a `ref`
  formal, and an `inout` or `own` argument may not share its storage.
- **A direct call's String result** borrows only storage its arguments
  reach, because no global storage holds an `Array<String>` element. The
  source is the join of the arguments' sources: a literal, a primitive or
  arithmetic reaches none; a String local carries its loan; an
  `Array<String>` local or its element carries that local's storage; any
  other argument is unresolved. A member call stays unresolved: its
  receiver is not among its arguments.
- **A call to a function whose String result is always fresh text**
  borrows nothing. The fresh String result plan
  (`ast_collection_fresh_string_result_plan_owner.pgy`) is a bounded fixed
  point over the functions: every value return is a String literal, a
  builtin whose result is fresh (the call hand-off rule, or
  `TextBuilderFinish`), a call to a function already in the plan, or a
  normal String local whose initializer and every whole-local assignment
  are such values and that no call may rebind (the domain exposure facts).
  A bare return, a formal, an element read, `ToString` or `Clone` of a
  String, a member access, or an unresolved assignment in the function
  leaves it out. The `let` or assignment that takes such a call's result
  records no loan.

Without the last two the canonical compiler program was refused twice, at
an owned `ArraySet` on one `inout` formal while a String read from a `ref`
formal, and a String a call made from `ref`-formal text and Int bounds, were
live (`expression_identity_epoch_owner.pgy`).

## Why

The general direct-MIR route ran `let name = names[i]; ArrayPop(names);
Log(name)` with heap-use-after-free on C and LLVM: the receipt marked the
pop owned while `name` borrowed the popped element. The same held for an
owned set (the request said a set leaks instead; the owned set frees the
replaced element, `pgy_as_set_owned_copy`), for a pop in an `inout` callee,
for a loan passed as an earlier argument of the popping call, for a loan a
callee hands back, and for a loan an enclosing loop reads after the pop.

The own hand-off rule (`667e45554`) scanned the callee body for
`ArrayDropOwnedStrings` of the formal. The element requirement already
records that, transitively and for pops. The scan is deleted, and the
requirement is the one source for releases inside a callee. Their
diagnostic is now the element-release boundary.

## Not covered

- Reassigning the array (`names = []`) while a loan is live still runs with
  heap-use-after-free: the backend releases the old storage, and no
  front-end fact records that release.
- A `for name in names` binding is not a local binding row of the semantic
  facts, so these loans do not see it. The direct-MIR `collection_range`
  stage still refuses a String walk whose body pops, sets or reassigns the
  walked array.
- `cur = names[k]; Log(cur); ArrayPop(names)` in a loop, with `cur`
  declared outside it, is refused although every pass overwrites `cur`
  before reading it. Declaring `cur` inside the loop is admitted.
- A call outside the fresh String result plan whose argument is an
  `Array<String>` formal stays unresolved: the lattice has no callee return
  provenance beyond that plan. A call nested in another expression
  (`ToString(F(xs))`) does not consult the plan.
- Native: a separate task.

Replaces: nothing. Narrows the own hand-off retirement of `667e45554` to the
element requirement.
