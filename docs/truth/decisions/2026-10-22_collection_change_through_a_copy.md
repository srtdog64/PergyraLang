# 2026-10-22: a HashMap, Set, List or Queue change through a copy refuses the other side

Decided by: the Claude lane. It completes the changes and copy rows of
[the local copy rule](2026-10-21_local_copies_refuse_the_other_side_after_a_change.md),
which named only Array, owned String and slot changes. The user can
overturn it.

## Rule

1. **A change.** `MapSet`, `MapRemove`, `SetAdd`, `SetRemove`,
   `ListPush`, `ListSet`, `ListRemove` and `QueuePush` change the storage
   their receiver shares with a copy, as an Array push or set does. After
   one through either side of a copy, the other side is refused at its
   next use.
2. **A copy row.** `MapSet`, `SetAdd`, `ListPush`, `ListSet` and
   `QueuePush` copy their value into the receiver, as an Array push or set
   does. When the value's storage changes after the store, the next
   analysis copies the value where it is stored (`Clone`) if `Clone`
   copies it whole. Otherwise the read through the collection stays
   refused.
3. **Not a change.** `QueuePop` and `ArrayPop` move only the receiver's
   own head or length, which each copy holds by value.
4. **The holder's path.** A value assigned into a field, or stored into a
   collection a path names, shares that path's storage only. A change
   through it refuses that path, and a change reaches the value only
   through a path that contains the field or lies inside it. Before, the
   copy row named the whole holder, so a change through a copy of one
   field refused a value assigned into another.

## Why

- The branch at `f0bf3e6a1` admits each of the eight changes through a
  local copy followed by a use of the source. C with ASan reports
  heap-use-after-free after growth through `MapSet`, `SetAdd`, `ListPush`
  and `QueuePush`, and LLVM prints a stale value for `QueuePush`. The
  source reads the copy's change after `MapRemove`, `SetRemove`,
  `ListSet` and `ListRemove`. An Array stored by `MapSet`, `ListPush`,
  `ListSet` or `QueuePush` and grown afterwards reads freed storage
  through the collection (ASan heap-use-after-free).
- It was found while measuring a deep `Clone` of a HashMap: a by-value
  `Clone(map)` grown afterwards was admitted the same way.
- An `inout` callee that changes such a collection was already refused:
  its formal fails the storage preservation proof.
- Rule 4: with rules 1 to 3 alone, the canonical driver input gained one
  refusal, in `SemanticAstCollectionExclusiveRecordReturn`. A loop there
  copies `facts.shared_results`, sets entries on the copy and assigns it
  back, and a later loop does the same with `facts.carrying`. The change
  through the second copy reached the first through the whole `facts`.
- One admitted program changed:
  `tests/cases/backend_compare/empty_literal_collection_result` pushed
  onto a local copy of a by-value `List` parameter, then read the
  parameter's size. It now reads the size before the push. `Clone` does
  not copy a List, so no copy can stand in.

Replaces: nothing.
