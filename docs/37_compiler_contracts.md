# Pergyra Compiler Contracts

Last updated: 2026-05-15

This document fixes the compiler source-of-truth contracts. It is not a claim
that every implementation path is complete. It defines the direction each path
must move toward during beta closure.

## 1. IR Layer Contracts

### AST

AST is the raw parse tree plus source span carrier. AST may be used for
user-facing diagnostic context, but it must not be the backend semantic
source-of-truth.

Forbidden:

- backend semantic rediscovery by walking AST payloads again
- ownership, authority, effect, or zone safety decided by AST helper traversal
- C and LLVM consuming different AST-carried declaration inventories

### HIR

HIR owns sugar-free typed language structure and the function/body CFG view.
All-path return, unreachable flow, and CFG-owned control lowering start here.

### DIR

DIR owns declaration and domain graph facts: `subject`, `ability`, `role`,
`party`, `zone`, `world`, `relation`, `effect`, `projection`, and `intent`
relations as graph edges.

### RIR

RIR owns resource-flow facts. Slot operations, projection state, authority
evidence, relation/effect propagation, rollback, invalidation, and runtime
resource facts belong here.

### MIR

MIR owns backend-executable control and cleanup facts. It carries CFG blocks,
instructions, cleanup roots, rollback/invalidation roots, pin cleanup facts,
direct-call facts, and terminator provenance.

### AIR

AIR is not a codegen IR. AIR is an abstraction-boundary verification layer. It
collects HIR/RIR/MIR/DAG evidence and checks intent, zone, world, authority, and
effect boundary drift. AIR must not change backend text. If AIR changes emitted
C or LLVM, that is a design violation.

### Loss Contracts

Every abstraction boundary has a loss budget. A pass may discard source shape or
representation details only when the boundary names the allowed loss, the facts
that must be preserved, the owner that keeps the original truth, and the
downstream reads that become forbidden after the cutover. `docs/semantics/09_abstraction_loss_contracts.md`
is the proof-pack owner for this rule.

Compression uses the same owner discipline. World, Zone, Intent, Slot, Role,
Roster, and related source-level axes are semantic axes, not automatic backend
artifacts. AIR may classify an intent or boundary as `retain`, `summarize`, or
`erase`, but C/LLVM must not turn a source-level axis into a physical carrier,
padding, barrier, or runtime authority check unless AIR/MIR/ABI evidence owns
that cost. The beta-visible vocabulary is `compression_budget` plus
`compression_reason` in `pgy.air.graph.v1`.

## 2. Compiler-Facing Orthogonality Rule

Compiler-facing orthogonality rule:

> Each semantic axis is decided once by its owner IR, then later phases consume
> that fact.

Concrete rules:

- backend semantic rediscovery from AST is forbidden.
- authority/effect/zone decisions are fixed by DIR/RIR/AIR evidence.
- Slot / Pin vs Static Lifetime: Slot is a runtime-validated handle model;
  pin/cleanup safety is backed by MIR cleanup facts plus CFG/AIR verification.
- If a document or test only refers to a domain keyword as `TOKEN_IDENTIFIER`,
  that is lexer/parser contract drift. Keyword contract documents should name
  the semantic axis, not just the token spelling.

## 3. Cleanup And Pin Contract

MIR cleanup fact names are owned by `src/compiler/mir_cleanup_fact_names.h`.
Consumers must use that vocabulary instead of duplicating string literals.

Required facts:

- normal cleanup edge: `MIR_CLEANUP_FACT_EDGE`
- rollback cleanup edge: `MIR_CLEANUP_FACT_EDGE_FROM_ROLLBACK`
- invalidation cleanup edge: `MIR_CLEANUP_FACT_EDGE_FROM_INVALIDATION`
- pin cleanup edge: `MIR_CLEANUP_FACT_PIN_UNPIN_EDGE`

MIR validation must reject topology-only cleanup claims when the matching fact
is missing. AIR may audit MIR cleanup evidence, but MIR remains the cleanup
owner.

## 4. Type Resolution Contract

DAG metadata is the beta source-of-truth for type dependency ordering.
Recursive resolver fallback is retired for the frozen beta surface.

Allowed:

- metadata lookup
- owner-local materialization through the central metadata API
- explicit DAG dead-end diagnostics

Forbidden:

- direct `resolve_type_node(...)` use outside the metadata owner
- hidden recursive fallback
- declaration-order-only type success for frozen subset paths

## 5. Backend Contract

C and LLVM may use different implementation techniques, but for the frozen
subset they must consume the same MIR/DIR/RIR inventory and produce equivalent
behavior.

The beta rule is not "LLVM is fully refactored." The beta rule is:

> frozen subset parity is locked, and declaration/top-level inventory seams are
> narrowed until backend truth drift is no longer observable.

## 6. Diagnostics Contract

Every user-facing diagnostic should carry:

- source span when available
- stable diagnostic code
- severity
- `Reason:` when the failure is semantic or contractual
- `Fix:` when an actionable rewrite exists
- layer classification (`syntax`, `type`, `resource`, `concurrency`,
  `domain`, `driver`, or `backend`)

Layered diagnostics are not cosmetic. They are how Pergyra avoids making
Resource, Execution, Domain, and Type/Contract failures look like one generic
semantic error.
