# OPEN 2026-10-19: a getter of a by-value aggregate, lend or move

Asked by: the Claude lane. Waiting for the user.

## The question

The [default-aggregate move rule](2026-10-18_default_aggregate_move.md)
says a call moves a default-mode (by-value) aggregate argument when the
callee "mutates that storage in place, or returns it or part of it". A
getter that only reads its by-value struct and returns one String field
therefore moves the struct, and the caller may not use it again.

Since [results lend `ref` inputs](2026-10-19_results_lend_ref_inputs.md),
the same getter with a `ref` parameter lends that field instead: the
caller keeps the struct and proves the loan.

## Why it matters now

The move owner runs only when every owner before it is clean, so the
canonical census has not shown its refusals. With it forced to run
(scratch census at `a4e2649c8`), the canonical program has 3654 move
refusals:

| Count | Moved into |
|---|---|
| 1986 | a getter that only reads and returns (for example `TypedAstArenaAtomText(arena, ...)`, `LookupKindType(env, ...)`) |
| 1032 | a callee that changes, keeps or releases the argument |
| 636 | a callee whose summary is unsupported (all escapes) |

The canonical program passes facts and arenas by value everywhere. The
V1 input must stay unchanged, so adding `ref` by hand is not the route.

## Options

| | Lend (recommended) | Keep the move |
|---|---|---|
| Getter that only reads and returns a part | lends; the caller keeps the value and proves the loan | moves; a later use is refused |
| Callee that mutates, keeps or releases | moves (unchanged) | moves (unchanged) |
| Canonical refusals removed | about 1986 | none |
| Source changes needed | none | `ref` on each such parameter |

Lend keeps the move rule's purpose: a callee that can free or reallocate
the caller's storage still moves it. What changes is only a callee that
returns part of the storage without touching it. That case is the same
as a `ref` input, which now lends.
