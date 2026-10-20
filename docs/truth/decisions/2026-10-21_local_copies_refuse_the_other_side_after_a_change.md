# 2026-10-21: a change through one side of a local copy refuses the other side

Decided by: the Claude lane. It applies the source model's value semantics
(`docs/semantics/27_ownership_clean.md` section 2.1: each read is an
independent value) and the [escaping loan rule](2026-10-20_escaping_loan_copies_where_it_fails.md)
(copy where the proof fails; the user asked for the long-term choice) to
copies inside one routine. The user can overturn it.

## Rule

1. **A copy row.** When a routine copies an access path of a struct,
   tuple, collection or payload enum into another binding, the move owner
   records a copy row, in execution order: a `let` initializer, an
   assigned value, an Array push or set value (the receiver is the
   binding), and a path inside a constructor, an Option wrap, a by-value
   `Clone` or an Array or tuple literal built into one.
2. **A change.** A push, set, owned String push or slot write through a
   path, an element assignment, a call that moves the path, and an `inout`
   argument change the storage the path names in place.
3. **The other side.** After a change through either side of a copy row,
   the other side, and every copy that shares the storage through other
   copy rows, is refused at its next use. The next analysis copies the
   path where it was copied (`Clone`), when `Clone` copies it whole.
   Otherwise the use stays refused.
4. **Fresh values.** A `let` that runs again, or an assignment of a whole
   binding, ends that binding's copy rows.
5. **Owned String pushes.** A plain `ArrayPush` of a String into an
   `Array<String>` that owns its elements (a `Clone` result among them)
   was refused for good. It is now selected, and the next analysis lands
   it as `ArrayPushOwnedString` when the receiver admits one. A refused
   push or set still checks its value, so the value's facts are published
   for the owners that run after the refusal.

A copy that is only read, or whose other side is never used after the
change, costs nothing.

## Why

- `main` (`7cd004fab`) admits `let other = outer.inner`, pushes onto
  `other.items`, then a read of `outer.inner.items[1]` (and the same for
  an assignment, constructor, literal, ArrayPush value, a copy of a copy,
  a push onto the source, an element store, an `inout` call, and a
  by-value formal): C with ASan reports heap-use-after-free, double-free,
  or a read of the other side's write. The `local_*alias*`,
  `local_copy_*` and `formal_let_alias_*` fixtures hold each shape.
- [The first attempt](../evidence/2026-10-21_native_census_copy_lend_not_landed.md)
  checked copies with the getter loan proof, which is not flow-sensitive.
- On the canonical driver input the rule adds 70 refusals of copies
  `Clone` cannot copy whole, mostly a field of an `inout` analysis passed
  to a callee that may change it: no fact says which fields an `inout`
  callee changes, so the whole argument counts as changed.

Replaces: nothing.
