# 2026-11-15: `Array<String>` storage handed to a callee is not released

The user chose this when the Claude lane asked how blocker 3 (the
hand-off fact) should release storage after a move.

## The case

A local `Array<String>` is passed to a default-mode parameter of a user
callee that grows, stores or returns it. By the
[2026-10-18 rule](2026-10-18_default_aggregate_move.md) the call moves the
argument.

- Native releases a caller local only under a privacy proof
  (`src/compiler/mir/mir_loop_collection_cleanup.c`, `private_uses`), so
  native releases nothing here and the storage leaks.
- The self-host semantic row stayed `live`, so direct MIR released it
  again in the caller. Since `f6ab5f0ce` direct MIR refuses that release,
  which costs 38 LLVM lifetime fixtures native admits.

## The rule

- The semantic verdict marks the caller's binding as handed off at that
  call. The caller does not release it.
- The callee does not release it either. Both front ends leak the same
  storage; no program reads or frees freed memory, and output stays equal.
- A local handed to a struct constructor is released once by the record's
  release recipe, as direct MIR already does when the record has one.

## Not chosen

- The callee consumes and releases its moved parameter. This needs a
  consuming-parameter fact in the MIR signature and the element ownership
  of each caller, and native would have to change in the same way to keep
  parity.
- Keep the direct MIR refusal.

## When to reopen

Reopen when a real program needs the storage back (a long-running loop
that hands off a growing array per iteration), or when the callee-release
fact is added to both front ends together.
