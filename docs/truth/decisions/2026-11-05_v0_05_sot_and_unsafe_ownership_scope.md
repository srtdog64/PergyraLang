# 2026-11-05: v0.05 closes the SoT spine and raw-pointer ownership cleanup

Decided by: the user, on 2026-11-05.

This is a post-V1 milestone. It does not widen or replace the V1 finish line in
[the V1 decision](2026-10-14_v1_scope.md) and its later amendments.

## Scope

v0.05 includes both:

1. **The whole SoT owner spine.** Every live top-level fact family in the
   [owner registry](../../semantics/sot_owner_spine_registry.md), including
   families discovered before the v0.05 freeze, has one authoritative producer.
   Its reached consumers use that producer; missing facts fail closed; the old
   semantic read is deleted; and a behavioral negative gate proves that the
   old answer cannot silently return. Obsolete rows retire with their routes
   instead of being declared `CLOSED`. No live row remains `ACTIVE` or `BRIDGE`.
2. **Ownership automation through View and raw-pointer layers.** Typed
   `ReadView<T>` / `WriteView<T>` and Pin/Lease are the checked, usual way to
   perform low-level access. The View keeps its backing owner live and its
   lexical cleanup releases the lease. A separate scoped `unsafe(raw)`
   capability permits the system-tier raw-pointer route. When that route
   starts from a compiler-known allocation or resource owner, the compiler
   carries the same provenance, transfer, and release obligation through
   supported raw-pointer operations and synthesizes the owner's cleanup on
   every exit after its last legal use. An owned raw-pointer result may carry
   that obligation when its allocation contract says so; a borrowed raw
   alias never becomes a second owner merely because it holds an address.
   This applies to both C and LLVM without a second backend ownership
   decision; supported FFI handoffs state whether they borrow, consume, or
   return ownership.

The current [`unsafe { ... }` contract](../../132_unsafe_capability_scope.md)
is a lexical boundary marker. It does not authorize arbitrary raw-pointer
operations, and `SlotRawPointer(...)` remains rejected today. This milestone
requires automatic cleanup of the *known owner behind* supported raw-pointer
use, not a free of each pointer value. An address from opaque foreign code
cannot become compiler-owned by guess. If a boundary does not establish who
owns the allocation, the compiler must not invent a release obligation; an
unsupported ownership claim must refuse with a specific diagnostic. v0.05
does not claim that arbitrary raw-pointer writes or unknown aliases are safe.

## Completion evidence

- The [Rocq feasibility model](../../semantics/proofs/RawPointerOwnershipClean.md)
  states the owner and scope conditions under which this can work. It is not
  compiler, backend, or release evidence.
- V1's published-commit, bootstrap, installed-driver, C/LLVM, and required-CI
  evidence remains a prerequisite; a green local slice is not a release.
- For each live SoT row, verify the last consumer, missing-fact refusal,
  removal of the old read, and its behavioral negative gate. Retired routes
  have no live consumers or bypasses.
- For supported `unsafe(raw)` cases, run positive gates that acquire an owned
  allocation, access it through View and raw-pointer routes, and observe one
  actual release after the final legal use across normal, branch, and early
  exits on both C and LLVM. A Slot-backed raw alias keeps the Slot as owner;
  a borrowed pointer must not trigger a second free. Negative gates refuse a
  missing capability, forged owner, illegal escape, invalid transfer, and
  statically visible use after release. Opaque foreign memory needs an
  explicit handoff contract before the compiler treats it as owned. Existing
  positive and refusal gates stay in force. Backend output and diagnostics
  must agree with the declared contract.
- External comparison may focus on Rust and Go. A claim that the View and
  raw-pointer layers share automatic ownership cleanup must name the
  supported boundary and executable evidence; this decision alone is not a
  claim of present capability or unique superiority.

This decision is a milestone target, not a statement that v0.05 has shipped or
that any currently open SoT row is closed.
