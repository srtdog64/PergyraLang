# Effect / Capability System and Resilience Modifiers (Design)

Status: accepted direction, staged implementation. This records two related
language extensions and the path to build them on the machinery Pergyra already
has, rather than as from-scratch type theory.

## Why these two, and why now

`if` / `for` give Turing completeness but not safety or expressiveness. The
patterns real programs reimplement endlessly and get wrong are: which functions
may touch authority / IO / allocation, and how calls retry or time out. Pergyra
already promotes domain orchestration to first-class (zone, intent, role,
authority, projection). These two extensions promote two more recurring patterns
that fit that thesis: a meta-primitive that tracks capabilities, and a
declarative resilience surface on intents.

The discipline for any promotion: it must give a guarantee a library cannot.
An effect set the compiler propagates and checks at zone boundaries is such a
guarantee; a `with retry(3)` the compiler lowers deterministically is another.

## Part A — Effect / capability system

### Flavor chosen

Annotation-based capability tracking, not full inferred row-polymorphic effects.
Effects are a checked property carried as declaration metadata, not an inferred
type. No effect handlers as a new construct: the zone authority boundary is the
handler. This is a generalization of what `authority` already does, not a new
subsystem.

### Effect lattice (start small)

First members, chosen because each already has a leaf or a discharge point in the
codebase:

- `authority(role)` — discharged by a zone authority grant. The runtime check
  `PGY_ZONE_AUTHORITY_CHECK(self, ...)` is already emitted at zone entry
  (transpiler_mir_func_emit.c); the static effect closes onto that existing
  dynamic check.
- `io` — introduced by capability-bound primitives. `ReadFile` is already
  capability-bound to `PGY_PROJECT_ROOT`, so the leaf introduction point is free.

Later members slot into the same frame: `alloc(lane)` (allocator lanes already
exist), `panic` (unrecoverable path per the error-handling articles), `async`.
`reactive` (a write that triggers `refresh`) is aspirational and is deliberately
deferred until the io / authority core is proven end to end.

### Annotation surface

Default is pure: a function with no clause has the empty effect set and needs no
annotation. Effects are declared only at boundaries:

    func ReadConfig(path: String) -> String uses io { ... }
    func Promote(u: User) -> Void uses authority(Admin) { ... }
    func Both() -> Void uses io, authority(Admin) { ... }

Grammar: an optional `uses <effect> {, <effect>}` clause after the return type in
a function signature. Parse point is parser_decl.c immediately after the
`TOKEN_ARROW` return-type parse (line ~177); the clause is also valid when there
is no `->` (Void). Stored on `func->data.func_decl` as an effect-name list
(empty = pure).

### Propagation

Within a function body the effect set is inferred: it is the union of the
declared effects of every callee plus the effects of any leaf primitive used.
Across the public boundary it is checked against the annotation: the inferred
set must be a subset of the declared set (an under-declared function is a
diagnostic; an over-declared one is allowed but lint-worthy).

Implementation reuses the transitive frontier propagation graph (the worklist
fixpoint already built over the zone/world call graph). Effect-set propagation is
the same algorithm: union callee effect sets to a fixpoint. The result is stored
as MIR declaration-header metadata, the same pattern used for ability metadata
and the source_ast retirement, so codegen and the checker read it from MIR.

### Discharge and boundary check

- A zone that grants authority for a role discharges `authority(role)` for code
  running inside it. Effects flow up the call graph until a zone discharges them.
- A leaf primitive (`ReadFile`) introduces its effect (`io`) at the bottom.
- At a zone / intent boundary the rule is: required effects must be a subset of
  the effects the boundary grants; otherwise emit a diagnostic. This is expressed
  as an AIR invariant so it is checked in the existing verifier discipline.

### Effect polymorphism

Higher-order functions (`ArrayMap(f)`) have an effect that depends on `f`. This
is made explicit at the collection builtin boundary rather than inferred
globally. The monomorphic first slice is live: `ArrayMap` and `ArrayFilter`
type-check the callback argument, read its function effect mask, and join it
into the caller's derived effect set. A function contracted `with effects local`
that maps with an `io` callback is rejected with the same missing-effect
diagnostic as a direct `ReadFile`.

General higher-order effect parameters remain a later stage; the collection
slice is intentionally narrow so it does not slide into unbounded inference.

### Why this is feasible on Pergyra specifically

1. The propagation graph already exists (transitive frontier propagation).
2. MIR is the single source of truth; effects are decl metadata like ability
   metadata.
3. The discharge mechanism already exists (zone authority grant + runtime check).
4. AIR is already the verifier; effect-safety is one more invariant.

The main risk is annotation burden, mitigated by pure-by-default plus
infer-internally / annotate-boundaries.

## Part B — Resilience intent modifiers

### Surface

A declarative modifier clause on an intent:

    intent ChargeCard with retry(3), timeout(2s), backoff(exponential) { ... }

`retry(n)` re-runs the intent body up to n times on a recoverable failure;
`timeout(d)` bounds wall-clock duration; `backoff(...)` shapes the delay between
retries. These compose: retry wraps the body, timeout bounds each attempt (or the
whole, a semantic choice to pin in the doc), backoff sequences them.

### Semantics to pin

- Retry triggers only on recoverable failures (the `Result` Failure path per the
  error-handling articles), never on a panic.
- `timeout` scope: per-attempt by default (a slow attempt is abandoned and
  retried) — to be confirmed against the runtime's cancellation surface.
- Determinism: backoff uses the runtime clock; the lowering is deterministic code,
  only timing varies.

### Lowering

Codegen wraps the intent body: a bounded retry loop, a timeout guard around each
attempt, a backoff delay between iterations. Emitted on the C backend first, then
mirrored byte-identical on LLVM, consistent with every other feature's two-backend
parity discipline. The intent AST gains a modifier list; parse point is the intent
declaration parser.

## Staged plan (mirrors the task list)

Effect system: design (this doc) → parser `uses` clause → MIR effect metadata →
propagation via frontier graph → leaves + zone discharge + boundary check →
effect polymorphism for HOFs.

Resilience: design (this doc) → parser `with` clause → AST modifiers → C codegen
wrapper → LLVM mirror → both-backend verification.

Every step is gated so programs without `uses` / `with` are byte-identical to
before, and each lands behind the full 16-gate self-host parity suite on both
backends, with dedicated tests for the new behavior.

## Reality check (discovered during implementation)

The effect system is not greenfield. Most of Part A already exists and is live:

- Lattice `EffectMask` in `type_system.h`: secure, remote, nondeterministic,
  collapse, unsafe (security / distribution / determinism oriented).
- Declaration surface: `func F(...) -> T with effects secure, remote { ... }`,
  parsed in `parser_decl_clause.c` / `parser_decl_function_clause.c`, stored on
  `func_decl.has_effects_clause` + `declared_effects`.
- Body derivation: `semantic_record_effect(ctx, mask)` is called by leaf
  constructs (channels and devices record `remote`, secure builtins record
  `secure`, DirWalk records `nondeterministic`).
- Boundary check: `type_checker_func_decl.c` closes the derived set and compares
  it to the declared set; an under-declaration raises `PGY_SEM_EFFECT_CONFLICT`
  ("missing declared effects ...").

So "generalize authority into effects" is not building a system; it is adding
effect families and their leaves to a pipeline that already declares,
propagates within a function, and checks at the boundary. The chosen first
member here is `io`: added as `EFFECT_IO`, named `io` in both the parser name
table (`parser_decl_clause.c`) and the semantic name/print table
(`type_checker_helpers_effects.c`), introduced as a leaf by the filesystem
builtins `ReadFile` / `WriteFile`. Verified end to end: a function declaring
`with effects io` that reads a file compiles; one declaring only `with effects
secure` that reads a file fails with "missing declared effects: io (derived from
body: io)"; a function with no effects clause is unaffected. The full 16-gate
self-host parity suite stays green on both backends.

A second family, `alloc`, was then added the same way to prove the pattern
generalizes: `EFFECT_ALLOC`, named `alloc` in both name tables, introduced as a
leaf by the explicit `Allocator*` constructors (`type_check_allocator_builtin`),
deliberately scoped to explicit allocators rather than every array literal to
avoid annotation burden. Verified end to end and shown to compose with `io`: a
function declaring `with effects alloc, io` that allocates and reads a file
compiles, while declaring only one of the two raises the missing-effect
diagnostic for the other. Self-host parity stays green on both backends.

A third family, `authority`, was then added as a first-class declarable effect:
`EFFECT_AUTHORITY`, named `authority` in both name tables, and bridged into the
existing authority machinery by extending `type_effect_mask_requires_authority`
(`type_effects.c`) so that a function carrying `authority` is treated the same
way `secure` already was -- an action within a zone that requires authority and
has no `authorized by` clause raises the existing action-contract diagnostic.
This unifies the surface: authority now participates in the `with effects`
contract like io and alloc, while the role-precise enforcement stays in the
existing `authorized_by` / `within_zone` / `PGY_ZONE_AUTHORITY_CHECK` path.
Verified: `with effects authority` parses, compiles, and composes with io and
alloc in a single contract; self-host parity stays green on both backends.

Corrected remaining work: a body-level derivation leaf for authority (calling an
`authorized_by` function or touching a zone authority should auto-derive
`authority`, so the effect is inferred and not only declared); role-precise
effect (today `authority` is a single coarse bit -- per-role precision still
rides the separate `authorized_by` metadata); transitive cross-function effect
propagation via the frontier graph if within-function derivation proves
insufficient; and general higher-order effect parameters beyond the live
`ArrayMap` / `ArrayFilter` collection slice.

## io leaf coverage (complete)

The `io` effect is now introduced by every filesystem builtin, not just
`ReadFile` / `WriteFile`: `FileExists`, `FileOpen`, `FileRead`, `FileWrite`,
`FileClose`, and `DirWalk` all call `semantic_record_effect(ctx, EFFECT_IO)`
(`DirWalk` records `io` and `nondeterministic`). A function contracted
`with effects local` that touches any of these is rejected with the derived set
named in the diagnostic. All 16 self-host parity gates stay green on both
backends, and the three-family composition (`io`, `alloc`, `authority`) plus the
MPaC `measurePartition` port and its P3 contract continue to pass.

Effect families implemented and verified: `io`, `alloc`, `authority` (declarable
and bridged to zone authority), on top of the pre-existing `secure`, `remote`,
`nondeterministic`, `collapse`, `unsafe`.

## Resilience intent modifiers: implementation path (partially live)

Unlike the effect system, resilience modifiers are not executable yet. The
first syntax slice is live only as parser/MIR-owned declaration metadata:
`intent X(...) with retry(n) { ... }` parses, prints as `IntentRetry: n`, and is
captured on `MIRDeclHeader.intent_retry_count`. `timeout(...)` and
`backoff(...)` remain parser-reserved errors. Semantic checking rejects a
non-zero retry count until both C and LLVM wrap the same MIR intent body, so the
feature cannot silently degrade into a no-op.

The remaining implementation path is therefore:

1. Lower retry in intent codegen by wrapping the MIR-owned intent body in a bounded
   retry loop with a per-attempt timeout guard and a backoff delay, on the C
   backend first, then mirrored byte-identical on LLVM.
2. Gate so intents without the clause are byte-identical; verify on both
   backends with dedicated retry/timeout behavior tests.

The codegen wrapper is the substantive, higher-risk part and is the reason this
is sequenced after the effect-family work rather than batched with it. The
declaration surface today is `with effects ...`, not a separate `uses` keyword;
that spelling is already the language's effect contract.

## Authority: one concept, two representations (intentional)

Authority appears in two places: the legacy `secure` capability flag and the
`EFFECT_AUTHORITY` effect-family bit. This is layering, not duplication. The
effect bit is the tracking surface -- it propagates through the declared-vs-
derived closure so a function that transitively touches an authority-bearing
leaf carries the obligation outward. The `secure` flag is the capability gate at
the boundary. The two are unified at enforcement:
`type_effect_mask_requires_authority` folds `EFFECT_SECURE | EFFECT_AUTHORITY`,
so either representation trips the same authority requirement and there is no way
for one to be satisfied while the other leaks. Keeping both lets existing
`secure` code stand unchanged while new effect-tracked authority flows through
the same closure machinery as `io` and `alloc`. Collapsing them into a single
token would either drop the propagation surface or force a rewrite of every
`secure` site for no behavioural gain.
