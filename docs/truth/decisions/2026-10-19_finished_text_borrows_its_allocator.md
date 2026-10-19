# 2026-10-19: text finished with a non-result allocator borrows it

Decided by: the Claude lane, from what the runtime does. The user can
overturn it.

## Rule

`TextBuilderFinish(builder, allocator)` gives a String whose storage
depends on the allocator:

- a **result lane** (a local made by `AllocatorResult()`, never rebound
  and never handed to a call that could rebind it) gives the String heap
  storage of its own, independent of the allocator;
- any other allocator, a pool among them, holds the String until
  `AllocatorDestroy` frees it, so the String is a loan of the allocator.

A routine whose result is such a loan of an allocator formal says so in
its summary (it returns that formal), and its callers apply the same rule
to the allocator they pass. For a loan of a **local** allocator, a final
owner (`allocator_lane_loan_owner.c`) requires:

- the loan stays in the routine;
- each `AllocatorDestroy` of that allocator is a statement of the routine
  body, and nothing read after it could hold the loan: no binding declared
  or named between the allocator's birth and its destroy, unless it is a
  physical scalar or a non-`inout` formal.

A declared routine that takes an allocator by value or by `ref` cannot
rebind the caller's binding, so passing a result lane to it keeps the
lane. Any other call still makes the lane unproved.

`AllocatorPool(capacity)` is new storage that depends on nothing.

## Why

`pgy_text_builder_finish` hands the builder's heap buffer to the result
for a result lane, and copies it into the pool otherwise;
`AllocatorDestroy` frees only a pool's buffer. Before this rule the native
checker admitted a pool's text read after `AllocatorDestroy` (ASan:
heap-use-after-free on main, `allocator_pool_read_after_destroy_invalid`,
`allocator_pool_loan_kept_outside_invalid`) and a pool's text returned past
the pool. It also left every routine that finishes text with an allocator
parameter unsupported, which made every caller's summary all escapes:
`JsonEscapeStringWithAllocator` did that to the parser's diagnostics.

Replaces: nothing. `pool_builder_birth_invalid` and
`rebound_pool_builder_birth_invalid` stay refused, now by this owner
instead of an incomplete relation.
