# 2026-11-20: ListSet and ListRemove retire String loans; a List loan keeps the unknown source

Decided by: the Claude lane, answering the OPEN `List<String>` item of the
[foreach decision](2026-11-20_foreach_string_binding_lends_its_element.md),
at the request of the session that made it. The user can overturn it.

## Rule

1. A `List<String>` owns its String elements on C and LLVM. `ListPush` and
   `ListSet` store a copy, `ListSet` frees the String it replaces,
   `ListRemove` frees the String it removes, and `ListGet` returns the
   element itself.
2. `ListSet` and `ListRemove` on a `List<String>` retire every String loan
   of unknown source in the routine, as an owned `ArraySet` does. `ListPush`
   retires nothing: growing a List moves its pointer array, not its Strings.
3. A loan from `ListGet`, and a for-in binding over a List, keep the unknown
   source the foreach decision gives "any other iterable". So:
   - a retired foreach binding read afterwards is refused at the read;
   - a retired `let` or assigned String read afterwards is copied where it
     is defined, under the existing unknown-source rule that already copies
     `let x = view[i]` for a `Slice<String>`.

## Why the List gets no storage source of its own

The OPEN item named a List storage source as the missing fact. Retirement
does not need one: every List loan has the unknown source, and rule 2
retires those. A List source would also change the store checks, which
read an exact source as an `Array<String>` owner with an ownership fact.
A List has no such fact, so a store of a List loan would match no owner
there. Today the unknown source sends the store through every
`Array<String>` in scope, and a push into an Array that owns its elements
lands an owned copy (probe in the
[record](../evidence/2026-11-20_native_list_string_loan_retirement.md)).
An exact List source therefore needs the store checks to learn List owners
first; it is not a retirement change.

What this costs:

- A `let` List loan read after a release is copied, where the same read of
  an `Array<String>` element is refused.
- `ListSet` and `ListRemove` also retire unknown-source loans that are not
  List elements (a field read, an unproved call result). A later read of
  one is copied or refused, as after an owned `ArraySet` today.

## Not covered

- A List loan stored where the element type is borrowed, then freed:
  `ArrayPush(out, ListGet(items, 0))` into an Array with borrowed elements,
  or `Row { name: ListGet(items, 0) }`, then `ListSet`, then a read of
  `out[0]` or `row.name`. Both still read freed memory on C and LLVM, before
  and after this rule. The same store of a `Slice<String>` element does
  too, so this gap belongs to the store checks for unknown-source loans.
- A loan held across a call whose `inout` formal takes `ListSet` (or, for
  an Array, an owned `ArraySet`) still reads freed memory. Loans are
  retired only at a direct builtin call on the receiver, for Array and List
  alike.

Replaces: the OPEN `List<String>` item under "Not covered" in the
[foreach decision](2026-11-20_foreach_string_binding_lends_its_element.md).
