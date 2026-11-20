# 2026-11-20: a String of unknown source stored past a List or a promoted Array is copied

Decided by: the Claude lane, closing the first "Not covered" item of the
[List loan decision](2026-11-20_list_string_release_retires_unknown_loans.md)
under the [2026-10-19 rule](2026-10-19_string_landing_copy_then_move.md)
that an unprovable String store lands as an owned copy. The user can
overturn it.

## Rule

1. Where a String of unknown source (a `ListGet` loan, an element of a
   `Slice` view, any String with no storage source) is stored, every
   `List<String>` in scope counts as an owner that may free it, as every
   `Array<String>` owner already does. The store lands a copy: a builtin
   push stores an owned copy (`ArrayPushOwnedString`, or a `Clone` where the
   receiver cannot own its elements), a struct field or other shallow store
   takes a `Clone`, and a declared callee's summary decides whether it keeps
   the String.
2. An empty local `Array<String>` whose element ownership is not yet known
   is excluded as an owner of such a String while it owns no String. Once
   its stores are promoted to owned copies, and the routine takes a `Slice`
   view of it and reads a String through a `Slice`, a String of unknown
   source may be one of its payloads. Each such String recorded against the
   Array gets an independent copy where it is used, unless it is stored into
   the Array itself or passed to a declared callee whose checked result
   neither depends on nor retains it. If no copy can be selected there, the
   program is refused.

## Why

- The store checks walked only `Array<String>` owners. A `ListGet` loan
  stored in an Array that does not own its elements, or in a struct field,
  met no owner and stayed shallow; `ListSet` or `ListRemove` then freed it
  under the stored pointer.
- The exclusion in rule 2 rests on the empty Array holding no String of its
  own. The old check dropped it as resolved as soon as the Array's stores
  became owned, which is when the premise fails. A `Slice` view of that
  Array lends its elements with the unknown source, so a stored view
  element read freed memory after an owned `ArraySet`.
- Rule 2 needs the view and a read through a view. Without them no String
  of unknown source in the routine can be one of the Array's payloads: its
  element reads carry its exact source, and a view handed to a callee lends
  nothing to the routine (a callee returning an element of a view it was
  given was safe on base). Copying every recorded String on promotion alone
  copied arguments of declared callees in 67 self-host probe programs.
- A copy, not a refusal: the 2026-10-19 rule. A `let` read of a released
  List or Slice element is already copied where it is defined.
- The List keeps the unknown source (List loan decision, "Why"). Rule 1 is
  the store checks learning List owners, which that decision named as the
  condition for an exact List source; this change does not need one.

What this costs:

- Every String of unknown source stored while a `List<String>` is in scope
  is copied, whether or not anything frees an element afterwards. A
  `return ListGet(items, 0)` of a local List returns a copy, as
  `return names[0]` of a local Array already does.
- In a routine that views and reads a promoted empty Array through a
  `Slice`, every String of unknown source recorded against it and kept past
  its use is copied, whether or not it came from the view.

## Not covered

- A loan held across a call whose `inout` formal frees the element (List
  loan decision, second item): unchanged.
- A `List<String>` reached through a path (`ListGet(bag.items, 0)`): the slot
  escape analysis refuses it today.
- Arrays that share another Array's Strings under an exact source of their
  own (`ArraySort` and `ArrayReverse` return their receiver's storage) were
  not examined.

Replaces: the first "Not covered" item of the
[List loan decision](2026-11-20_list_string_release_retires_unknown_loans.md).
