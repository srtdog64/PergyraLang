# 2026-11-20: a foreach String binding lends its element, and an owned pop retires loans

Decided by: the Claude lane, at the user's request to refuse the native
foreach use-after-free. It extends the
[returned String element lends](2026-10-19_returned_string_element_lends.md)
and [every loan is checked](2026-10-21_every_loan_of_a_path_is_checked.md)
rules to the `for` binding. The user can overturn it.

## Rule

1. `for x in xs` with String elements binds `x` as the element read
   `xs[cursor]`, and `x` carries the loan that read carries. An
   `Array<String>` iterable lends from its storage source. Any other
   iterable (a `Slice<String>`) lends from an unknown source, which every
   owned release retires, as `let x = view[i]` does.
2. Each iteration binds a fresh element. A release in one iteration
   retires the loan held in that iteration, not the next one.
3. An owned `ArrayPop` (one that frees the String it removes) retires every
   loan of its receiver's source, as an owned `ArraySet` does. Before, only
   `ArraySet` and `ArrayDropOwnedStrings` retired loans, so
   `let x = xs[1]; ArrayPop(xs); Log(x)` was admitted as well.
4. A read of a retired loan is refused at the read. A store of the binding
   into storage that outlives the loan lands an owned copy, as for
   `let x = xs[i]` (for example `ArrayPush(out, x)` becomes
   `ArrayPushOwnedString`).

The binding does not own a copy. No decision makes an element binding an
owner, and a copy on every iteration would charge every walk for the few
bodies that release what they walk.

## Why

On main `9c4ac12af` the native checker recorded the foreach binding as an
independent String, with no loan. A body that set, popped or dropped the
walked array and then read the binding printed freed memory on C and LLVM,
and so did a binding kept in an outer String or pushed into another Array
([record](../evidence/2026-11-20_native_foreach_string_borrow_refusal.md)).

## Not covered

- `List<String>`: the String loan owner has no storage source for a List,
  and `ListSet` and `ListRemove` retire nothing. Both the foreach form and
  `let x = ListGet(xs, 0); ListSet(xs, 0, y); Log(x)` read freed memory on
  native C. OPEN: the missing fact is a List storage source in the loan
  owner, retired by the List mutators that free.
- Reassigning the iterable: emitted code frees nothing on reassignment,
  so it is admitted for the binding as for `let x = xs[i]`. If
  reassignment starts to free, it must retire loans in the same owner.

Replaces: nothing.
