# Types as Domain Medium — Pergyra Type System Positioning

Last updated: 2026-04-30

Anti-hype rule (per `docs/120` §0):

- "Type system is the medium of recovery" is a *design mandate*, not a feature claim.
- Current implementation depth varies by coordinate (see `docs/120` §5).
- This doc states what types *should do* in Pergyra; `docs/100` and `docs/118`
  state what they *currently do*.

Related documents:

- `docs/19_design_philosophy.md` §0 — core identity (systems language baseline)
- `docs/00_vision.md` — intent-first vision narrative
- `docs/106_ownership_model_comparison.md` — sister positioning (ownership)
- `docs/114_async_model_positioning.md` — sister positioning (concurrency)
- `docs/117_backend_strategy_positioning.md` — sister positioning (backend)
- `docs/118_slot_model_rigor_audit.md` §8 — sister negative-space (vocabulary)
- `docs/119_pergyra_lineage_positioning.md` §11 — sister negative-space (lineage)
- `docs/120_vision_and_capability_audit.md` — sister negative-space (capability)
- `docs/122_managing_intent_drift.md` — sister positioning; the *operational counterpart* (this doc states what types should carry; docs/122 states what to do when carriers fail)
- Memory: `project_research_program_thesis.md` — root motivation (lost-meaning
  recovery); this doc is the *type-system-level expression* of that thesis

This document positions Pergyra's type system. It is a **design mandate**:
what the type system is *for*, what it must carry, and what to resist.
Concrete contracts (ABI, semantic rules, generic resolution) live in
`docs/100`, `docs/107`, `docs/semantics/`, and `pgy_abi_spec.h`.

## 0. Thesis — Type System Is the Syntactic Machine of Lost-Meaning Recovery

The research program (memory: `project_research_program_thesis.md`) is to
recover seven domain meanings — *who / why / how-far / qualification / world
/ failure-responsibility / state-transition* — that mainstream languages
push outside the code into README / comments / Jira / senior heads.

The type system is the **primary syntactic vehicle** of that recovery. Types
in Pergyra are not computational targets (Zig `comptime`) and not propositions
to prove (Haskell, Curry-Howard). They are **coordinate carriers**: each
type expresses *where the value lives* in the recovered domain semantic space.

One-line design statement:

> **Pergyra's type system = the syntactic machine of lost-meaning recovery.**
> Types *carry* domain coordinates (carrier), *check* domain coherence
> (coherence), and *reject* domain violations (negative space). They are
> neither computational targets nor proof targets.

This thesis governs every type-system decision: a feature is admitted if and
only if it strengthens carrier / coherence / negative-space. Features that
add type-level expressiveness *without* serving recovery are out of scope by
design, no matter how technically attractive.

## 1. Three Languages, Three Relationships With Type

| Language | Relationship | "Operator of..." |
|---|---|---|
| Zig | Types are first-class compile-time *values*; `comptime` makes you compute on/with them imperatively | type system |
| Haskell | Types are *propositions* to prove via unification and type-class resolution | type system as logic |
| **Pergyra** | Types are **carriers** of recovered domain coordinates | **domain vocabulary** |

All three shift the programmer from "user" to "operator," but of different
substrates. Pergyra does not compete with Zig on type-level metaprogramming
or with Haskell on type-level logic; it operates on a different axis
entirely. Comparisons that flatten the three onto one axis are categorical
mistakes.

## 2. The Seven Lost Meanings ↔ Type-Level Coordinate Axes

Each of the research program's seven recovered meanings corresponds to one
or more type-level coordinate axes. The type system's job is to make these
coordinates *visible, checkable, and rejectable*.

| Lost meaning | Type-level coordinate | Carrying mechanism |
|---|---|---|
| Who acts | `subject` / `party` / `roster` | The *kind* of type — `subject` is type-level distinct from `class`; party / roster are type-level group containers |
| Why act | `intent` | Contracts attached to type — `requires` / `authorized by` / `guard` / `compensate` |
| How far allowed | `ability<T>` / `authority<T>` | Capability moves with the type as a parameter |
| Qualification | `role` / `Token<Capability>` | Capability witnesses embedded in type |
| What world | `zone` / `world` annotation | "T in zone Z" — type-level residence of the value |
| Failure responsibility | `Result<T, E>` + intent compensate clause | Failure made unavoidable by type signature |
| State transition | `Slot<T>` / `tobject` lifecycle | Resource lifecycle tracked at type level |

Pergyra's current design attempts to **carry these seven coordinates together
at type level**. Other languages carry related pieces: Rust (qualification,
via ownership), Erlang (world, via process isolation), Pony (qualification,
via reference capabilities), Verona (world, via region/cown). The Pergyra
type-system mandate is to aggregate those coordinates into one vocabulary, but
implementation depth remains bounded by the beta stable subset.

### 2.1 Subject, Authority, Projection: Do Not Collapse The Axes

The most dangerous modeling drift is to treat `subject` as "important
information" and `authority` as "importance level." That is not the model.

Pergyra separates three questions:

| Question | Construct | Meaning |
|---|---|---|
| Is this value a domain actor with identity-bearing state transitions? | `subject` | Semantic center of action and lifecycle |
| May this action cross or mutate a protected boundary? | `authority` / `role` / `Token<T>` | Permission to perform a state transition or boundary crossing |
| Which part of a subject may be observed outside its home boundary? | `projection` / visibility | Selective information exposure |

Consequences:

- A type is not a `subject` merely because its data is important.
- Important information does not automatically require authority.
- Unimportant-looking information may require authority if it crosses a
  protected boundary or causes side effects.
- Selective information exposure belongs to `projection` and visibility,
  not to `authority`.
- `authority` guards mutation, handoff, external effect, and boundary
  crossing; it is not an information-ranking mechanism.
- `intent` may infer and consume an authority edge for compressed orchestration,
  but it does not become the canonical authority owner. The owner stays on the
  `zone` / resource layer, and inferred approval must carry provenance back to
  that owner.

This distinction prevents the language from turning every rich data model
into a permission-heavy object. The stable modeling rule is:

```text
shape-only data                         -> struct / object / vessel
identity-bearing state transition host  -> subject
boundary mutation or handoff            -> subject + authority
selective outward view                  -> projection / visibility
execution or lifecycle residence        -> zone / world
```

If a design cannot decide whether a value is a `subject`, ask whether the
value owns domain transitions that must be audited or rejected at compile
time. If the answer is "no", use a passive shape (`struct`, `object`, or
`vessel`) and expose it through projection only when needed.

### 2.2 Graph-Shaped Reality, Not Tree-Shaped Ownership

Pergyra must not become a Rust-style lifetime language for business objects.
Rust's lexical lifetime model is strong for pointer/reference safety, but many
real domains are graph-shaped: UI component trees plus event graphs, workflow
DAGs, commit history DAGs, caches, observer links, authority relations, and
projection networks. Forcing those graphs into an owning tree makes code serve
the compiler rather than the business intent.

Identity statement:

> Pergyra does not statically predict the lifetime of all business objects. It
> statically rejects unsafe boundary transitions and dynamically validates
> resource handles.

Consequences:

- Do not force every business object into an owning tree.
- Do not require compile-time prediction of every business object's creation
  and destruction point.
- Do not model circular or DAG-shaped domain relations only through `own/ref`.
- Do not treat `zone` / `world` as lexical lifetime blocks for all contained
  business objects.
- Use `own/ref` for narrow anchored boundaries; use `relation`, `effect`,
  `projection`, `zone` / `world`, `authority`, and `SlotHandle` for
  graph-aware domain structure.

Static checks answer boundary questions:

- May this value cross this world/zone/task boundary?
- Who authorizes this state transition?
- Is this projection fresh enough for the declared contract?
- May this handle escape its task, world, or pin boundary?

Runtime checks answer resource-existence questions:

- Is the handle generation still current?
- Is the token valid?
- Did the slot release or tombstone before this access?
- What recoverable failure or hard-fail state should the runtime expose?

This is the intended split: compile time rejects unsafe *transitions*; runtime
validates dynamic *existence*. Pergyra's domain model should preserve business
intent instead of turning a graph into a lifetime puzzle.

## 3. Three Design Axes — How Types Operate as Medium

### 3.1 Carrier — Type Declarations Visibly Carry Coordinates

A type declaration is a *coordinate label* in the recovered domain space.
Reading a type tells you *where* the value lives, not just what shape it
has:

```pergyra
subject Order
    in zone OrderZone
    authorized_by OrderingRole
{
    slot state: OrderState
    relation customer: Customer
    intent submit { ... }
}
```

The reader sees: *who* (subject Order), *where* (zone OrderZone), *qualification*
(OrderingRole), *state lifecycle* (slot OrderState), *relations* (customer),
*intents* (submit). All seven coordinate axes co-present.

By contrast, `class Order { ... }` in mainstream languages carries shape
only; coordinates leak to README / comments / Jira.

**Mandate**: every Pergyra type declaration should make as many recovered
coordinates *visible* as the domain warrants. Implicit coordinates are
allowed but explicitness is the default.

### 3.2 Coherence — Compile-Time Checks Domain Coherence

The compile-time check on Pergyra types is **not type unification** (Haskell)
and **not arbitrary comptime computation** (Zig). It is **domain coherence**:
a finite, declarative, decidable set of questions about the recovered
coordinate space.

Examples of domain coherence questions:

- *Role coverage*: "Does this party have all roles required by its intent?"
- *Compensate completeness*: "Does this intent's compensate clause cover
  every failure class declared by its requires clause?"
- *Zone exit cleanup*: "Does this zone's exit point release every slot
  acquired at entry?"
- *Authority transport*: "Does this subject hold the authority required by
  this intent at this call site?"
- *Boundary discipline*: "Does this Channel transport only types that the
  destination world admits?"

These are *decidable in finite time* by walking declarative facts. They are
not Turing-complete. They do not require unification with backtracking.
They do not require imperative comptime evaluation. They are the type
system's **bread and butter**, and they correspond directly to AIR drift
detection (`docs/104`).

**Mandate**: domain coherence checks are the type system's primary
compile-time work. Type unification serves only the structural backbone
(generic monomorphization); domain coherence carries the weight.

### 3.3 Negative Space — Rejection Is the Power

The recovered meanings are *most leveraged* when types **reject** uses that
mainstream languages permit. Examples:

- `Token<TransferAbility>` — rejects every call site that does not declare
  TransferAbility
- `Slot<DeviceMemory>` — rejects general-purpose memory operations
- `Symbolic<Bool>` — rejects `Symbolic<Int64>` operations
- `subject Order in OrderZone` — rejects cross-zone use without explicit
  Channel transfer
- `intent SubmitOrder requires HasCustomerSession` — rejects invocation
  without an active session

The mainstream-language alternative is *runtime check* or *convention*. Both
leak into README / comments / Jira / senior heads. The Pergyra alternative
is *type-level rejection*: the program does not compile.

**Mandate**: when faced with a design choice between "permit an observable
domain-boundary violation" and "reject at type level," prefer rejection.
Negative-space is the strongest recovery surface.

This mandate is intentionally narrower than "reject everything that could fail
at runtime." Pergyra does **not** try to statically predict every business
object's lifetime (§2.2). The semantic split is:

```text
static rejection  = unsafe transition across a known boundary
runtime validate  = dynamic existence/state of a resource handle
```

Reject at type level when the compiler can see a coordinate violation:

- unsupported world/zone/task handoff
- missing authority for a declared state transition
- token transport through an unsupported boundary
- pin/view crossing a suspension or transport boundary
- projection kind/source/target mismatch

Do not turn dynamic graph existence into a compile-time puzzle. Slot generation,
token validity, TTL cleanup, runtime registry presence, and tombstone state are
runtime facts unless an enclosing boundary rule makes the escape statically
visible. This preserves the "graph-shaped reality, not tree-shaped ownership"
identity: the type system rejects unsafe *transitions*; the runtime validates
dynamic *existence*.

## 4. What to Resist — Off-Axis Temptations

The type system is small and load-bearing. Adding off-axis features
weakens the medium. The following are explicitly out of scope, regardless
of technical attractiveness.

| Temptation | Why off-axis | Pergyra alternative |
|---|---|---|
| Zig-style imperative `comptime` (types-as-values, `inline for`) | Computational, not coordinate-carrying. Different research program. | Extend coordinate vocabulary (more abilities, zones, roles). Computation stays out. |
| Haskell HKT / Functor / type-class hierarchy | Already explicitly rejected (memory: `project_functor_hkt_stance.md`). Pulls type system toward logic-prover. | Per-container concrete operations. |
| Full dependent types (Idris/Agda) | Types-as-values pulls the type system into computation. Recovery thesis dissolves. | Type-level *coordinates* (zone, ability, role) without value-dependence. |
| Type-as-pure-shape framing | "Types are just labels" undermines recovery. | Every type declaration carries visible coordinates. |
| Type-level expressiveness as a goal in itself | Causes drift away from research program. | Add expressiveness only when it strengthens carrier / coherence / negative-space. |
| User-customizable compile-time error messages | Dilutes deterministic diagnostic family (`PGY_SEM_*`). | Domain-vocabulary diagnostics (zone / authority / intent) on top of deterministic codes. |

The test for any proposed feature: *does it strengthen carrier, coherence,
or negative-space?* If no, reject. If yes, the proposal moves to design
review with `docs/100` beta scope as the next gate.

## 5. Comparison Snapshot — Pergyra vs Zig vs Haskell at the Type System Level

| Capability | Zig comptime | Haskell type system | Pergyra type system |
|---|---|---|---|
| Types as compile-time values | ✅ | partial (kind-level) | ❌ (intentional) |
| Type-level functions (T → T) | ✅ | ✅ (type families) | ❌ (intentional; only generic monomorphization) |
| User-customizable type errors | ✅ | partial (TypeError class) | ❌ (deterministic `PGY_SEM_*`) |
| Domain coordinate carrier (subject / zone / authority) | ❌ | ❌ | ✅ |
| Domain coherence check (role coverage, compensate completeness) | ❌ | ❌ | ✅ (via AIR + semantic) |
| Negative-space rejection of capability misuse | ❌ | partial (newtype + phantom) | ✅ |
| Generic monomorphization | ✅ | ❌ (erasure) | ✅ |
| Ability / capability bounds | partial (manual via `comptime if`) | ✅ (type classes) | ✅ (ability + multi-bound) |
| HKT / Functor / type-class hierarchy | partial (manual) | ✅ | ❌ (intentional reject) |

Pergyra's columns are deliberate. The ❌ rows are not gaps; they are
boundaries set by the research program. The ✅ rows in the bottom three
sections are where Pergyra differentiates: domain coordinate machinery that
neither Zig nor Haskell attempts.

## 6. Beta Closure Implications

This mandate is the sanity check for every type-system task in beta scope:

| Task | Sanity check | Verdict |
|---|---|---|
| Ability bound expressiveness expansion | Strengthens carrier? | ✅ in scope |
| WriteView<T> aliasing-XOR-mutability (Option C lift) | Strengthens negative-space? | ✅ in scope |
| AIR domain coherence inventory (Phase 1+) | Strengthens coherence? | ✅ in scope |
| Generic param ownership classifier | Strengthens carrier? | ✅ in scope |
| Zig-comptime-style type-level computation | Off-axis | ❌ out of scope |
| HKT / type-class hierarchy revival | Off-axis (already rejected) | ❌ out of scope |
| Dependent types in full generality | Off-axis | ❌ out of scope |
| User-customizable compile-time error messages | Dilutes deterministic diagnostic family | ❌ out of scope |

For tasks in scope, the next gate is `docs/100` beta-readiness checklist.
For tasks out of scope, the next gate is `docs/120` §4 vision section
(post-1.0 candidacy with research-program audit).

## 7. Post-1.0 Vision Territory

The following are *not* claims for current capability — they live in
`docs/120` §4.2 mid-term / §4.1 long-term vision space, gated by the
research-program audit:

- **Zone-conditional ability composition** — `ability A unless in_zone Z`,
  `ability A & B`, etc. Strengthens carrier within recovery axis.
- **Role-coverage typed parties** — compile-time enforcement that every
  declared role of a party has a binding before the party becomes
  constructible.
- **Cross-world type provenance** — Channel transports type-level provenance
  metadata so the destination world can prove receipt with the same
  coordinates the sender attached.
- **AIR Phase 2 evidence integration** — `EvidenceNode` extended to carry
  type-level domain coordinates as drift facts.
- **Sbv-style symbolic execution library port** — postponed to post-1.0
  (BDFL declared 2026-04-30). Beta type system is intentionally not built
  for this; the port is a *test* of how far the recovered-coordinate frame
  carries into solver-DSL territory.

- **FP compatibility module track** — any Zig-comptime-inspired symbolic or
  type-level operator work belongs under `pgy.compat.fp`, not `pgy.core`.
  This keeps the beta generic contract honest: ability/multi-bound/default
  type arguments are core, but arbitrary compile-time type functions are not.

These are tracked here so external description does not extract them as
beta features.

## 8. Cross-References

- `docs/19_design_philosophy.md` §0 — core identity (systems language
  baseline; this doc is the type-system-level expression)
- `docs/00_vision.md` — intent-first vision narrative
- `docs/106_ownership_model_comparison.md` — sister positioning (ownership)
- `docs/114_async_model_positioning.md` — sister positioning (concurrency)
- `docs/117_backend_strategy_positioning.md` — sister positioning (backend)
- `docs/118_slot_model_rigor_audit.md` §8 — vocabulary negative-space
- `docs/119_pergyra_lineage_positioning.md` §11 — lineage negative-space
- `docs/120_vision_and_capability_audit.md` — capability negative-space
- `docs/104_air_compiler_architecture.md` — AIR is the runtime of §3.2
  coherence checks
- `docs/107_beta_stable_subset.md` — what type-level surface is frozen at
  beta
- Memory: `project_research_program_thesis.md` (root motivation),
  `project_functor_hkt_stance.md` (HKT reject lineage),
  `feedback_capability_overclaim_audit.md` (don't extract §7 vision into
  current capability claims)
