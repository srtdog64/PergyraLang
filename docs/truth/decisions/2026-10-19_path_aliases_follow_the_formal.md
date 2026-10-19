# 2026-10-19: a local or loop variable that takes a path is the formal's alias

Decided by: the Claude lane, from what the runtime does. The user can
overturn it.

## Rule

In a routine's storage proof for one by-value or `ref` formal
(`function_param_array_storage_transfer_owner.c`):

- `let x = F.f` with an aggregate path of the formal, and `for x in P`
  where `P` is the formal or one of its paths and the element type is not
  a String or copy-only value, make `x` an **alias**. Its uses are proved
  with the same path rules as the formal's: a builtin writer through it
  changes the shared storage, an element write below an index step does
  too, and any other aggregate read of it copies the storage.
- An alias that leaves the routine is never carried by the formal's
  summary, so returning it, or handing it to a routine whose summary for
  that formal is more than a read, leaves the storage unproved.
- Statements that name only an alias are walked as well as those that name
  the formal.
- Assigning the alias itself changes only the local.

## Why

Before this rule any `let x = F.f` of an aggregate counted as unproved,
the largest storage-leaf census row (222 at `a6c221188`), so readers like
`let ids = arena.ids` moved their caller's argument.

The proof also skipped the iterable of a `for` loop. On main
`for block in blocks { ArrayPush(block.ids, 7) }` with a by-value
`blocks: Array<Block>` was admitted, and the caller then read freed
storage (ASan: heap-use-after-free,
`default_aggregate_move_array_loop_element_store_invalid`).

Assigning a local that took a path does not release the caller's storage:
ASan is clean with `Array<Int>` and `Array<String>` fields
(`default_aggregate_move_field_alias_rebound_valid`).
