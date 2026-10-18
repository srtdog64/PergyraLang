# 2026-10-18: a call moves a default-mode aggregate the callee changes

Decided by: the user, asking for the long-term choice (the Claude lane
recommended this one).

## Rule

A default-mode parameter of aggregate type (struct, Array or another
collection value) is a shallow copy: the callee and the caller share the
same storage. If the callee mutates that storage in place, or returns it
or part of it, the call moves the argument.

- The caller may pass a fresh value (a call result, literal or
  constructor), or reassign the same binding from the result:
  `cfg = AddBlock(cfg)`.
- Any other later use of the moved value is refused at that use.
- The move is inferred from the callee's parameter-flow summary. There is
  no `own` annotation to write, and the diagnostic names the callee and the
  parameter that moved the value.

## Why

`func AddBlock(cfg: Rows) -> Rows { let ids = cfg.ids; ArrayPush(ids, 0); ... }`
can reallocate `cfg.ids` and free the caller's buffer. A caller that reads
its old `cfg` afterwards reads freed memory. The native checker already
refuses this ("Array return still carries backing ... inferred transfer
proof is unavailable") without a rule that says when the transfer holds.

- Copy semantics would be safe but would copy whole arrays on every
  functional update, and needs a counted copy-on-write runtime to avoid it.
- `own` annotations would be the user discipline the language avoids.
- The move gives the compiler the ownership fact that automatic cleanup
  needs, at no runtime cost.

Replaces: nothing. Applies to the native checker first; the self-host
checker adopts the same rule from one rule source later.
