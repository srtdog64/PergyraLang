# Managing Intent and Abstraction Drift

Last updated: 2026-04-30

Anti-hype rule (per `docs/120` §0):

- "Drift management" is a *design discipline*, not a feature claim. Some
  drift kinds (semantic, time) cannot be solved by language tooling alone.
- Current tools cover four of the five management dimensions; the fifth
  (recognition) is permanently methodological, not automatable.
- Vision territory items in §6 are not current capability; cite from
  `docs/120` §4 when discussing them externally.

Related documents:

- `docs/19_design_philosophy.md` §0 — core identity (systems language baseline)
- `docs/00_vision.md` — intent-first vision narrative
- `docs/104_air_compiler_architecture.md` — AIR drift detection (static
  drift's primary tool)
- `docs/106_ownership_model_comparison.md` — sister positioning (ownership)
- `docs/114_async_model_positioning.md` — sister positioning (concurrency)
- `docs/117_backend_strategy_positioning.md` — sister positioning (backend)
- `docs/118_slot_model_rigor_audit.md` §8 — sister negative-space (vocabulary)
- `docs/119_pergyra_lineage_positioning.md` §11 — sister negative-space (lineage)
- `docs/120_vision_and_capability_audit.md` — sister negative-space (capability)
- `docs/121_types_as_domain_medium.md` — sister positioning (type system mandate)
- Memory: `project_research_program_thesis.md` — root motivation; this doc is
  the *operational discipline* for when recovery primitives drift in real use

This document is a **discipline doc**: how to think about, observe, and
respond to the inevitable breakage of intent and abstraction in Pergyra
programs. It is the operational counterpart to `docs/121` (which states
*what types should carry*); this doc states *what to do when carriers
fail*.

## 0. Thesis — Drift Management Is Not Drift Prevention

Every abstraction leaks. This is established in software engineering:
Joel Spolsky's "Law of Leaky Abstractions" (2002), Frederick Brooks'
"No Silver Bullet" (1986), and twenty years of post-mortems from
production systems. Pergyra's research program (memory:
`project_research_program_thesis.md`) is not "build abstractions that
do not break" — that target is impossible. The program is **build
abstractions whose breakage is *manageable*.**

Treating breakage as an exception means no management system is built.
Treating it as the *normal* operating state means tools are designed for
visibility, containment, evidence, and recovery. Pergyra's primitives
(intent / zone / world / authority / handoff / Slot / Channel / AIR) are
**drift-aware infrastructure**, not drift-immune fantasy.

One-line statement:

> **Pergyra manages drift across five dimensions: visibility, boundedness,
> evidence, recoverability, and recognition.** Language-level tooling
> covers the first four. The fifth is permanently methodological — it
> requires a human reading evidence and deciding whether to patch or
> redesign.

## 1. Five Kinds of Drift (Each Requires Different Management)

| Kind | What breaks | Pergyra mechanism | Coverage |
|---|---|---|---|
| **Static drift** | Code does not match declared intent | `PGY_SEM_*` deterministic diagnostics; AIR drift facts; semantic rejection of contract violations | ✅ Strong (AIR Phase 1 deployed; Phase 2 expands) |
| **Runtime drift** | Execution behavior diverges from declared intent | `Result<T, E>` mandatory; Slot generation panic on stale handle; structured logs; supervision (Erlang-derived pattern) | ✅ Strong for in-process; 🟡 distributed supervision is vision territory |
| **Semantic drift** | Code matches intent literally; *the intent itself is wrong* | None at language level. Falsification criteria + redesign. *Methodology only.* | 🔴 No language tool. Cannot exist — semantic correctness is not type-checkable. |
| **Layer drift** | Abstraction breaks but program continues with corrupted assumptions | Zone / world boundary discipline; pin block scope; Channel-only cross-World; ownership classifier conservative rejection | 🟡 Partial. "Abstraction failure mode" vocabulary not yet defined. |
| **Time drift** | Intent was right at design time; the world changed | None at language level. Schema evolution / intent versioning not yet designed. | 🔴 Explicit gap. Intent versioning is a `docs/120` §4 vision item. |

**Honest summary**: Pergyra has strong tools for static and runtime
drift, partial tools for layer drift, and no tools for semantic and time
drift. The last two are addressed by methodology (§4) and evidence
gathering, not by the language.

## 2. Management Has Internal Structure — Five Dimensions

"Manage drift" is not one operation. It is five distinct operations,
each requiring different tooling. When asking *how do I manage this
drift*, the productive question is *which of the five am I missing*.

### 2.1 Visibility — Make Drift Detectable

The primary rule: *unseen drift is the worst drift*.

Compile-time errors > runtime panics > silent corruption. The earlier in
the cycle a drift is caught, the cheaper its management. Pergyra's
deterministic diagnostic family (`PGY_SEM_*`) and AIR drift facts exist
to make as many drift kinds as possible *visible at compile time*.

For drifts that escape compile-time detection (timing, environmental,
distributed), runtime tooling provides the second visibility line: Slot
generation panic, structured `Result` errors, and structured logs
(per `CLAUDE.md`).

The rule for any new feature: *if it can drift, it must be visible when
it does*. Silent corruption is the only unmanageable failure mode.

### 2.2 Boundedness — Contain Drift Blast Radius

Drift that escapes its origin contaminates everything reachable. The
language's primary tools for bounding blast radius are:

- **Zone boundary**: drift inside a zone cannot leak outside without
  crossing a declared exit point.
- **World isolation**: drift in one world is invisible to other worlds
  except through `Channel<T>` transports.
- **Compensate scope**: an `intent`'s compensate clause localizes
  failure recovery to the intent's responsibility.
- **Slot generation**: a stale handle's drift is bounded to its own
  generation; subsequent allocations see fresh state.
- **`pin` block**: the typed-view scope contains pinned state's
  visibility window.

Each of these is a *blast radius reducer*. When a drift occurs, the
radius is the boundary of the smallest containing primitive.

This is the **load-bearing reason** intent / zone / world / authority
exist as language primitives rather than convention. Convention does
not bound blast radius reliably; type-level enforcement does.

### 2.3 Evidence — Record Drift for Future Iterations

Drift that is bounded but not recorded is *learned only locally*. The
next program will hit the same drift in a new shape. The language must
preserve drift as data so future iterations of the language and program
designs can use it.

- **AIR EvidenceNode provenance** — records where each evidence flag
  came from in HIR / DIR / RIR / MIR. Drift facts are *traceable*.
- **`PGY_SEM_*` diagnostic family** — deterministic codes survive bug
  reports, version diffs, and replay.
- **Structured logs (`CLAUDE.md` §3.2)** — JSON with `event`, `stage`,
  `requestId`, `policy`, `error.code`, `meta`. Drift events are
  machine-parseable.
- **`docs/security/`** — adversarial counterexample audits preserve
  exhaustion claims as testable artifacts.

Each drift recorded with evidence is *one input* to the next iteration.
A drift seen and forgotten is a drift that recurs.

### 2.4 Recoverability — Drift Should Allow Degraded Operation

Crash is one valid response. Degraded operation is another. The choice
is a *policy decision* that the language requires to be explicit.

- **`compensate` clause** — declared at intent definition; runs when the
  intent fails partway.
- **Supervision** — Erlang-derived restart pattern for failed
  computations.
- **`Result` fallback** — explicit alternative path in the type system.
- **Retry policy** (with idempotency contract per `CLAUDE.md` §4) —
  retry is an explicit policy with `maxAttempts`, `backoffMs`,
  `retryOn`, `idempotencyKey`. Implicit retry is forbidden.

A program without a recovery policy treats every drift as fatal. A
program with a wrong recovery policy hides drift indefinitely. The
language exists to make recovery policy *visible and reviewable*.

### 2.5 Recognition — Distinguish Patchable Drift From Wrong-Abstraction Drift

This is the **most expensive** management dimension. It cannot be
automated. It requires a human reading evidence and judging whether the
drift is a *bug* (the abstraction is right, the implementation is
wrong) or a *modeling error* (the abstraction itself is wrong).

Diagnostic table for recognition:

| Signal | Diagnosis | Response |
|---|---|---|
| Same kind of drift recurs in multiple places | Abstraction is short. Patching one site is theatrics. | **Redesign the abstraction.** |
| Drift does not align with zone boundaries | Zone primitive is unfit for this domain. | **Reconsider zone coordinates.** |
| Users circumvent the abstraction in normal use | Abstraction misses real domain shape. | **The abstraction is wrong.** |
| Drift occurs at a predicted location | Abstraction is right; implementation has a bug. | **Patch.** |
| Drift occurs at an unpredicted location | Modeling gap; new coordinate candidate. | **Audit the modeling.** |

The 1-year freeze post-beta is the **recognition window**: a year of
real-program drift evidence to feed this judgment before 1.0 lock-in.
After 1.0, schema-migration cost makes redesign expensive. Before 1.0,
redesign is free. **The freeze window's primary work is recognition.**

## 3. Mapping Drift Kinds Onto Management Dimensions

Each drift kind needs a different combination of management dimensions.
The matrix below shows which dimensions Pergyra currently provides for
each drift kind.

| Drift kind | Visibility | Boundedness | Evidence | Recoverability | Recognition |
|---|---|---|---|---|---|
| Static | ✅ AIR + `PGY_SEM_*` | ✅ scope | ✅ provenance | n/a | methodology |
| Runtime | ✅ panic + logs | ✅ zone/world/Slot | ✅ logs | ✅ compensate/supervision | methodology |
| Semantic | 🔴 not detectable | n/a | 🟡 only via post-hoc audit | n/a | **methodology only** |
| Layer | 🟡 partial | ✅ boundary | 🟡 partial | 🟡 partial | methodology |
| Time | 🔴 not detectable | n/a | n/a | n/a | **methodology only** |

The two columns of red are the two drift kinds language tooling cannot
solve. They are not gaps to fix; they are *categories the language is
the wrong layer for*. Methodology fills these.

## 4. Methodology — What the BDFL (Or Future Maintainers) Must Do

For drift kinds the language cannot manage automatically, the
discipline is:

1. **Declare falsification criteria *before* writing the program.**
   Examples:
   - "If the same `PGY_SEM_*` code recurs in three unrelated files, the
     associated abstraction is wrong, not those three sites."
   - "If users add `unsafe { }` blocks to bypass `WriteView` discipline
     more than rarely, the WriteView model has missed a real shape."
   - "If `intent` step counts exceed five in routine business flows,
     the intent primitive is too granular for this domain."

   Without falsification criteria declared in advance, every drift is
   reflexively patched and the abstraction never gets redesigned even
   when wrong.

2. **Preserve every drift with evidence.** Drift facts (AIR), diagnostic
   reports, structured logs, and user-reported workarounds. None of
   these are noise — each is an input to recognition.

3. **Decide patch-vs-redesign on evidence, not gut feeling.** The
   §2.5 signal table is the discipline. "Feels like a small bug" is
   not evidence; "this is the third time `PGY_SEM_AUTHORITY_DRIFT`
   appeared in unrelated party definitions" is evidence.

4. **Treat the 1-year freeze as the abstraction validation window,
   not a marketing pause.** Beta is not "feature complete and ready
   to lock"; beta is "structurally complete enough to gather drift
   evidence." Real programs in beta are the *experiments* whose
   results inform 1.0.

5. **"Drift = data, not failure."** A drift caught is a learning input.
   A drift hidden is a future production incident. Reframing drift
   from "failure mode" to "data source" changes everything about
   how the team responds to it.

## 5. The Real Tooling Inventory

For external description and internal sanity check, the management
inventory is:

### 5.1 Implemented at beta

- `PGY_SEM_*` deterministic diagnostic family
- AIR Phase 1 (Intent + Boundary nodes; drift facts; evidence
  provenance non-empty invariant)
- `Result<T, E>` mandatory failure-typing
- Slot generation runtime validation (panic on stale handle)
- `pin` block scope
- Zone / world / Channel boundary discipline (semantic-level enforcement)
- `compensate` clause (intent-level recovery)
- Structured logs (`CLAUDE.md` §3.2 schema)
- `docs/security/` adversarial counterexample audit framework
- `docs/118` §8 + `docs/119` §11 + `docs/120` 3-pair anti-hype audit
  (recognition methodology backing)

### 5.2 Beta-gated, partial

- Generic param ownership classifier (conservative; `docs/118` §6.2)
- Branch / join lattice (sealed; `docs/118` §6.3)
- CFG body dataflow (~70%; `docs/118` §6.4)
- WriteView aliasing-XOR-mutability (Option C lift pending; `docs/118` §6.1)

### 5.3 Vision territory (per `docs/120` §4)

- AIR Phase 2 (Constraint / Effect / Drift Node; expands static drift
  recognition)
- Distributed supervision runtime
- Intent versioning syntax (time drift management)
- Abstraction failure mode vocabulary (layer drift visibility)
- Schema migration tooling (time drift recovery)

External description must distinguish 5.1 / 5.2 / 5.3. `docs/120` §1
forbids citing 5.3 as current capability.

## 6. The Honest Limits

This doc states what *can* be managed; the limits are equally important.

- **Semantic drift is not addressable by any language.** If the modeling
  is wrong, no tool finds it; only programs and evidence do. The
  language can make this drift *visible faster* by making the modeling
  consequences fail early, but it cannot detect "the modeling itself
  is wrong" because that is a category the language is the wrong
  abstraction for.
- **Time drift requires versioning machinery the beta does not have.**
  A program written against 1.0 Pergyra primitives, running against
  2030 Pergyra primitives, may break when intent semantics evolve.
  Migration tooling is a 1.0+ task. Beta programs implicitly accept
  this risk.
- **Recognition cannot be automated.** Even with perfect AIR Phase 2,
  the patch-vs-redesign decision requires human judgment on evidence.
  Tools provide evidence; humans recognize patterns.
- **No drift management is free.** Every visibility check, boundary
  enforcement, evidence record, and recovery policy has cost (compile
  time, runtime overhead, code surface, mental load). The mandate is
  *manage what matters*, not *manage everything*.

## 7. Cross-References

- `docs/19_design_philosophy.md` §0 — core identity (this doc operationalizes
  drift management for the substrate that doc establishes)
- `docs/121_types_as_domain_medium.md` — type system mandate (types are the
  carriers; this doc handles when carriers fail)
- `docs/104_air_compiler_architecture.md` — AIR drift detection (the primary
  static drift tool)
- `docs/118_slot_model_rigor_audit.md` §6 — known danger zones (drift kinds
  the static layer is conservative on)
- `docs/120_vision_and_capability_audit.md` §4 — vision territory (drift
  management items not in beta)
- Memory: `project_research_program_thesis.md` (root motivation),
  `feedback_capability_overclaim_audit.md` (5.3 vision items must not be
  cited as current capability), `feedback_marketing_language_drift.md`
  (vocabulary discipline)
- `CLAUDE.md` §3.2 (structured log schema), §4 (retry policy contract)
