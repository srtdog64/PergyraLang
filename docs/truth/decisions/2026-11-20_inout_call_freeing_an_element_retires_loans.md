# 2026-11-20: A call whose inout formal frees a String element retires the loans of its actual

Decided by: the Claude lane, closing the inout-call gap the List loan change
names under "Not covered" (`2026-11-20_list_string_release_retires_unknown_loans.md`
on branch `claude/stoic-kilby-72aa03`, not yet on `main`), at the request
of the session that made it. The user can overturn it.

## Rule

1. A routine frees String elements of the storage rooted at one of its
   formals when, on a place rooted at that formal, it runs an owned
   `ArraySet` or `ArrayPop`, `ListSet` or `ListRemove` on a
   `List<String>`, or `ArrayDropOwnedStrings`. It also does when it hands
   that formal to an `inout` or `own` formal that frees them, or to a
   callee with no body. This is the formal release summary of
   `call_argument_move_order_owner`, which already said which formals
   release their whole storage; it now also says which free an element.
   The rows are recorded in Pass 2 and closed over the hand-offs after it.
2. A call that passes storage to an `inout` formal whose summary frees
   elements retires the String loans live across the call that may borrow
   from that storage. These are the loans a direct release of that storage
   retires: those of the actual's exact `Array<String>` source, and those
   of unknown source. A `ListGet` loan has the unknown source.
3. A retired loan read after the call is treated as after a direct release:
   - a loan of an exact source is refused at its first read;
   - a loan of unknown source is copied at each reaching definition, where
     the definition owner can issue that copy, and refused where it cannot.
4. A callee that only reads or pushes onto its `inout` formal frees nothing
   and retires no loan.

## How the call is judged

The callee's summary is final only after Pass 2: the callee may be checked
after its caller, or reach a releasing routine through hand-offs. So the
call cannot retire the loans where it is typed, as a builtin release does.
Pass 2 gives each String local that may borrow from the actual an edge
naming the call (`indexed_string_call_release_owner.c`), and each read of
such a local records the edges it has there. The String receipts phase
judges every read against the closed summary.

The edges follow the existing invalidation flag of the loan owner, so a
call retires a loan on the same paths a direct release does:

- an `if` branch that does not fall through drops its edges at the join,
  and a read in one branch does not see a call in the other;
- a `defer` body's calls are dropped after it is checked;
- a re-initialized local starts with no edges.

A loop body is the one place this goes further than a direct release on
`main`: a read before the call in the body is joined with the edges the
body holds at its end and at each `continue` (the back edge), so the next
iteration's read meets the call. The loop is not checked again for this.

## Why a List loan is copied, not refused

The request asked for both reported programs to be refused at the read.
The `List` one is copied instead. A loan of unknown source read after a
direct owned `ArraySet` is already copied where it is defined (as for
`let x = view[i]` over a `Slice<String>`), and the List loan change gives
a `ListGet` loan read after a direct `ListSet` the same copy. A release
inside a callee is the same release; refusing the loan here and copying it
there would give one hazard two answers depending on where the release is
written.

## Why not the member-carrier owner

`member_element_borrow_carrier_owner` already refuses a String read through
an aggregate member and held past an `inout` call that may change that
field. Extending it to `names[0]` would track a second time the loans the
loan owner already names by storage source. The member case keeps its own
diagnostic (at the call) and is not changed.

## What this costs

- The summary is per formal, not per element: a callee that frees
  `names[1]` retires a loan of `names[0]`.
- A loan of unknown source that is not an element of the actual (a field
  read, an unproved call result) is also retired by a call that frees an
  element of any `inout` collection of Strings. It is copied or refused, as
  after a direct owned `ArraySet` today.
- A callee with no body that takes a collection of Strings `inout` counts
  as freeing.

## Not covered

- A loan stored in a container or a struct field, then read after the call
  (the store gap the List loan change names).
- A release on a path that `break`s, read after the loop: the `if` that
  holds the `break` does not fall through, so its edges are dropped at the
  join, and nothing carries them to the loop exit. A direct owned `ArraySet`
  before the `break` has the same gap on `main` (its invalidation flag is
  dropped the same way); both read freed memory on C and LLVM.
- `match` arms do not snapshot the edges, as they do not snapshot the
  invalidation flag: a call in one arm retires a loan read in a later arm.
- When the foreach String loan change lands, its loop re-check counts only
  flagged loans; loans with call edges rely on the back edge here instead.
  The back edge reads the scopes around the loop, which leaves the loop's
  own binding (re-bound each iteration) out.
