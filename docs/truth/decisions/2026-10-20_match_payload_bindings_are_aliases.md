# 2026-10-20: a match on a value that holds a formal binds aliases of it

Decided by: the Claude lane. It extends the storage proof of a by-value
formal. The user can overturn it.

## Rule

When the subject of a `match` holds the formal's storage (the formal, a
path of it, an alias or a call result that holds it), each payload
binding of each case pattern is an alias of that storage, as a loop
variable over a path of the formal is. The pattern itself names nothing
of the formal; the case guards and bodies are walked as uses. A match on
any other subject is walked as before.

Before, the subject itself was refused as a copy of the formal's storage,
so every routine that matched a by-value enum formal failed its storage
proof, even when it only read the payload.

## Why

- On the canonical driver input, matching a by-value enum formal was the
  first cause of a failed storage proof in 24 routines
  (`DriverSourceCRequestDiagnostic` and its siblings).
- Main admits a routine that pushes onto a matched payload of its by-value
  enum formal while the caller uses the enum again (C with ASan:
  heap-use-after-free). Here the binding is an alias, so the push is a
  change of the formal's storage and the caller's later use is refused.

Replaces: nothing.
