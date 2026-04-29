# Pergyra Lineage Positioning

Last updated: 2026-04-28

Related documents:

- `docs/19_design_philosophy.md` §0 — **core identity** (systems language with domain extensions); this lineage doc is the *external coordinate* of that identity
- `docs/106_ownership_model_comparison.md` — sister positioning doc for ownership (Tier 1-2 detail)
- `docs/114_async_model_positioning.md` — sister positioning doc for concurrency (Tier 3 detail)
- `docs/117_backend_strategy_positioning.md` — sister positioning doc for backend strategy (Tier 1 C+LLVM detail)
- `docs/118_slot_model_rigor_audit.md` §8 — sister audit; vocabulary negative-space (this doc §11 is its lineage-level pair)
- `docs/120_vision_and_capability_audit.md` — sister audit; capability negative-space + current-vs-vision separation (third pair)
- `docs/121_types_as_domain_medium.md` — sister positioning; type system as the syntactic machine of lost-meaning recovery (the type-system-level expression of this lineage's research program)
- `docs/104_air_compiler_architecture.md` — Tier 5 MLIR sibling-IR pattern

This document is a **positioning / lineage** doc, not a contract. It exists so
that anyone asking "which language is Pergyra in the lineage of?" gets the
honest answer rather than a marketing reduction. The frozen contracts live
elsewhere (`docs/19` core identity, `docs/118` rigor audit, `docs/106/114/117`
sister positioning).

## 0. Thesis — C# Is the Father; The Rest Is Substrate Borrowing

The honest one-line answer to "which language is Pergyra in the lineage of":

> **C# is the father.** Pergyra *aspires to* the shape and spirit of C#
> (multi-paradigm versatility, OOP+FP+DOP fusion, async/await, properties,
> generics, records, partial class, pattern matching, LINQ-style pipelines) and
> *rewrites that target shape on a systems-language substrate* (C universal
> substrate + Rust-1.0-comparable static safety layer + Vale-style
> generational handles + Pony/Verona-inspired capability/region patterns +
> Erlang/Koka-style concurrency decomposition + OCaml-style type/Result
> discipline + MLIR-style sibling verification IR concept). On top of this,
> **DDD primitives** (intent / zone / world / authority / handoff) are
> introduced as *first-class syntactic constructs*, with enforcement depth
> varying by primitive (see `docs/120_vision_and_capability_audit.md` for
> current-vs-vision separation).

The "aspires to" is load-bearing. Pergyra at beta is *not yet* fully
C#-shaped at every surface, *not yet* fully Rust-static-equivalent, and
*not yet* MLIR-class in verification depth. Each tier names a *direction
and pattern borrowed*, not a *feature parity claim*.

Three layers, three roles:

1. **Tier 0 (Father)** — C#. The *target shape and feel*. What Pergyra wants
   to *look and read like* in the developer's eye.
2. **Tier 1-5 (Substrate borrow)** — C, Rust, Vale, Pony, Verona, Erlang,
   Koka, OCaml, Haskell, MLIR, F\*, Dafny. The *implementation borrowings*
   that let the C# shape run on a systems-language substrate without GC, CLR,
   or runtime dependency.
3. **Unique synthesis** — intent / zone / world / authority / handoff. The
   *DDD-lifted* layer that exists in **none** of Tier 0 or Tier 1-5.

If a description compresses this into a single parent — "Rust-like",
"C# clone", "Go competitor" — it has lost the synthesis and become a lie.
This doc is the anchor against that compression.

## 1. Why a Lineage Doc Exists

Who reads this:

- External writers comparing Pergyra to other languages
- Beta evaluators asking "is this just another Rust?"
- Reviewers asking "what is this in the lineage of?"
- Internal contributors deciding which lineage's idiom to draw on

Who does not read this:

- New users looking for surface guides (those are `docs/22`, `docs/74`)
- Implementers looking for ABI / contract (those are `docs/38-40`,
  `docs/113`, `docs/semantics/`)

This doc pairs with `docs/118` §8 marketing-language audit. `docs/118` §8 is
the *negative-space* (phrases to avoid). This doc is the *positive-space*
(honest coordinates). Use both together when writing external description.

## 2. Six-Tier Lineage Classification

| Tier | Domain | Parent(s) | How it entered Pergyra |
|---|---|---|---|
| **0 (Father)** | **Identity / Shape / Spirit** | **C#** | Multi-paradigm aspiration (OOP+FP+DOP), async/await syntax, properties, generics, records, partial class, pattern matching, LINQ-style. *What Pergyra aspires to read and feel like.* |
| 1 | Systems substrate | C, Rust 1.0 | C is universal substrate borrow (ABI / FFI / predictable memory). Rust 1.0 is the *target reference* for the static safety 5-component layer (ownership classifier + CFG + pin + Channel + Token); current strength is *comparable* to Rust 1.0, not equivalent (see `docs/118` §7). |
| 2 | Resource model | Vale, Pony, Verona | Vale-style generational references → Slot. Pony-style reference capabilities → Token / authority. Verona-style region / cown → World / Zone. Pattern borrowed; full feature parity not claimed. |
| 3 | Concurrency | Erlang/Elixir, Koka/Effekt | Erlang-style Channel-isolated parallel + supervision-tree DNA. Koka-style effect / handler decomposition → coloring split. **Syntax surface aspires to C# (Tier 0).** |
| 4 | Type / Data discipline | OCaml/ML, Haskell (partial) | ADT, Result, pattern matching (OCaml/ML). Parametric polymorphism (Haskell). Functor / HKT explicitly **rejected**. Joins Tier 0 records / pattern matching. |
| 5 | Verification | MLIR, F\* / Dafny / RustBelt | AIR borrows the *sibling-IR concept* from MLIR (not dialect-ecosystem scale; AIR Phase 1 has 2 node types). Level 2-4 proof trajectory (F\*, Dafny, RustBelt) — beta ships at Level 2. |

**Tier 0 vs Tier 1-5 distinction is load-bearing**: Tier 0 is the *target
shape*; Tier 1-5 are *implementation pattern borrowings used to realize that
shape on a systems substrate*. Pergyra is **not** "a language that looks
like Rust" — it *aspires to* "a language that reads like C# and runs like
Rust + Vale at the parts already implemented." For the gap between
aspiration and current implementation, see
`docs/120_vision_and_capability_audit.md`.

## 3. Tier 0 — C# (Father / Shape)

Twelve specific points where C# is directly formative for Pergyra:

1. **Multi-paradigm versatility (다재다능)** — OOP + FP + DOP simultaneously.
   C# 9+ records + LINQ + class + struct + interface 4-way maps directly to
   Pergyra's `subject` / `class` / `intent` / `zone` 4-way.
2. **async/await syntax surface** — C# (2012) was the first mainstream
   pioneer; JS/Python adopted later. Pergyra's `async` / `await` syntax is C#
   direct lineage. The *decomposition* of coloring is Koka substrate; the
   *syntax* readers see is C#.
3. **Properties as language primitive** — get / set blocks at language level
   come from C#.
4. **Generics with monomorphization** — `class Pair<T>`. C# generics are
   reified at JIT; Pergyra monomorphizes at compile time (Rust/C++ pattern).
   Syntax is C#, specialization timing is Rust.
5. **records and `with` expressions** — C# 9+ functional sugar for immutable
   value composition.
6. **partial class / modular composition** — C# language-level support for
   splitting one type across files, mirrored in Pergyra's modular trait /
   capability composition.
7. **pattern matching** — C# 7+. `match` direct lineage.
8. **subject / class distinction** ↔ C#'s class / struct / record / interface
   four-way decomposition. Pergyra extends to subject (host) / class (nominal
   value) / intent (contract) / zone (boundary).
9. **nullable reference types (NRT)** — C# 8+ opt-in safety. Evolved in
   Pergyra into Result-mandatory + Option pattern (per
   `CLAUDE.md` Result-First).
10. **LINQ-style expression-bodied** — functional pipelines inside an OOP
    shell. The "FP within OOP" attitude that Pergyra inherits.
11. **Industrial SW fitness** — C# is the daily driver of the language
    designer (factory equipment SW). C# dominates industrial automation
    (Siemens TIA Portal, Rockwell, .NET PLC interfaces). Pergyra's
    killer-usecase frame (web dungeon crawler + industrial control) aligns
    with C#'s versatility ground.
12. **Microsoft Research DNA pool** — F\* / Verona / Project Verona share
    research lineage with C#. Tier 0 (father) and Tier 5 (verification) are
    naturally connected through this DNA pool.

**Key**: C# is the *target shape*, not a *substrate*. Pergyra reads and
writes like C#, but it does not depend on GC, CLR, or runtime. That
substrate place is filled by Tier 1-5.

## 4. Tier 1 — C + Rust 1.0 (Systems Substrate)

- **C** — universal substrate borrow, not identity. Source of ABI / FFI /
  predictable memory layout. `docs/19` §0 anchor: "C is substrate borrow,
  not identity."
- **Rust 1.0** — static safety is an influence, not an equivalence claim.
  Pergyra's 5-component layer (ownership 5-class + CFG dataflow + pin block
  boundary + Channel-only cross-World + Token transport reject) covers a
  narrower beta subset and relies on runtime Slot validation for the remaining
  handle/capability checks. Do not describe this as Rust-level safety; see
  `docs/118` §7 and `docs/120` for the precise gaps.

Together, Tier 1 fills the place C# (Tier 0) resolves through GC + CLR.
C# uses managed memory + JIT + reflection. Pergyra uses C substrate + Rust
static layer + Vale runtime handle. Same shape, different substrate.

## 5. Tier 2 — Vale + Pony + Verona (Resource Model)

- **Vale generational references** ↔ Slot's runtime-validated handle. Direct
  lineage. The generation counter + token capability + TTL cleanup in
  Pergyra's Slot is the Vale pattern, audited adversarially in
  `docs/security/`.
- **Pony reference capabilities** (iso / val / ref / box / tag) ↔
  Token / authority capability flow. The Channel-only cross-World rule is the
  Pony capability-flow idea applied to World boundaries.
- **Verona region / cown** ↔ World / Zone isolation. Region semantics from
  Verona inform how Pergyra reasons about zone-bounded resource sets.

## 6. Tier 3 — Erlang/Elixir + Koka (Concurrency Semantics; Syntax Is C#)

- **Erlang / Elixir** ↔ World isolation + Channel-only cross-World + spawn
  pattern. Supervision-tree DNA enters as zone / world failure isolation.
- **Koka / Effekt** ↔ effect / handler systems. Coloring decomposition
  (effect / intent / pin / parallel / Channel / Result split) is in the
  effect-system family.

**Important distinction**: the *syntax surface* of `async` / `await` is Tier
0 C#. Tier 3's Erlang / Koka contribution is *semantics* — what the syntax
*means* and how it decomposes. A reader sees C#-style `await`; the compiler
treats it as a Koka-style effect with Erlang-style isolation.

## 7. Tier 4 — OCaml/ML + Haskell (Type / Data Discipline)

- **OCaml / ML** ↔ ADT, Result, pattern matching. The language designer
  anchored OCaml as fundamental ("OCaml is also rooted in C").
- **Haskell** ↔ parametric polymorphism (partial). **Functor / HKT
  explicitly rejected** (memory: `project_functor_hkt_stance.md`). What was
  refused is more identity-defining than what was borrowed: per-container
  `map` instead of typeclass abstraction.

This tier joins Tier 0's records and pattern matching naturally. C# 9+
already pulled OCaml-style ADT-ish records and pattern matching into the OOP
shell; Pergyra extends that direction with stronger Result discipline.

## 8. Tier 5 — MLIR + F\* / Dafny / RustBelt (Verification)

- **MLIR** ↔ AIR borrows the *sibling-IR concept* from MLIR — sits *beside*
  the codegen path as a verification-only IR rather than *on* the codegen
  path. AIR is **not MLIR-scale**: Phase 1 has two node types (Intent +
  Boundary). Pattern borrowed, not capability claimed. See `docs/104` §1.
- **F\* / Dafny / RustBelt** ↔ Level 2-4 proof trajectory. Pergyra ships at
  Level 2 (theorem statements + judgments + evidence) for beta and reserves
  Level 3-4 (paper proof, mechanized small-step) for the post-1.0 freeze.
  See `docs/118` §9.

Tier 5 connects naturally to Tier 0: F\* and Verona are Microsoft Research
projects that share DNA with C#'s research arm. The verification lineage
and the father lineage share an ecosystem.

## 9. Rejected Lineages — Common Mistakes in Comparison

| Rejected parent | Why rejected | The honest frame |
|---|---|---|
| Zig | Dual-emit comparison frame is explicitly rejected (memory: `project_backend_strategy.md`). Pergyra is not C-replacement. | Abstraction portability (`docs/117` §1). |
| Go | Channel inspiration only; coloring / intent / Token diverge sharply. | DDD primitive lifted to language first-class (Tier 0 C# father; not Go). |
| Java / Kotlin | OOP gestures only; GC / runtime model rejected. | Systems language baseline (`docs/19` §0.3). C# is the OOP father, not Java. |
| C++ | Frequent comparison on expressiveness; substrate is different. | C is substrate borrow. C++ is *not* a Pergyra ancestor. Influence is incidental, not lineage. |
| Swift | Swift surface comparison (init / let / var) is incidental. | Tier 0 father is C#, not Swift. Swift's reference counting model is rejected. |

This table pairs with `docs/118` §8 marketing-language audit. `docs/118` §8
is *vocabulary* negative-space; this section is *lineage* negative-space.
Use them together.

## 10. Unique Synthesis — Layers That Exist in No Parent

Pergyra's `intent` / `zone` / `world` / `authority` / `handoff` are **not in
any parent** — neither in Tier 0 C# nor in Tier 1-5 substrates. They come
from Domain-Driven Design and Hexagonal Architecture, lifted into language
primitives.

This means Pergyra is a three-layer composition:

```
(Tier 0: C# shape) ∘ (Tier 1-5: systems substrate borrow) ∘ (DDD primitive 1급)
```

The third layer is what's genuinely new. When asked "what is Pergyra
different from X for?", the honest answer points to this third layer, not to
Tier 0 or Tier 1-5.

This is also why marketing should not say "Pergyra = C# + Rust". That
formula leaves out the third layer entirely, and it is the third layer that
exists for the language to exist at all.

## 11. Lineage Statement — Phrases That Are Accurate

Use the right phrasing for the right context. This is the positive-space
pair to `docs/118` §8 (negative-space audit).

| Context | Accurate phrasing |
|---|---|
| 1-second elevator | "Looks like C#, runs like Rust + Vale, with DDD primitives as first-class language constructs." |
| 30-second pitch | "Pergyra rewrites C#'s versatile multi-paradigm shape on a systems-language substrate (C + Rust 1.0 + Vale + Erlang/Koka + OCaml + MLIR), and lifts intent / zone / world / authority into first-class primitives." |
| 5-minute depth | §0 thesis, verbatim. |
| Technical comparison | §2 six-tier table, verbatim. Tier 0 = C# father; Tier 1-5 = substrate borrow; §10 = unique synthesis. |
| Academic comparison | §3-§8 tier-by-tier evidence with citations to specific design papers. |

**Phrasings to avoid** (extension of `docs/118` §8):

| Wrong phrasing | Why wrong |
|---|---|
| "Rust-like language" | False. C# is father, Rust is substrate. Inverts the relationship. |
| "C# clone with Rust syntax" | False. C# is shape (not clone target), Rust is substrate (not syntax). |
| "Better C#" | Marketing puff. Pergyra is *different* from C# (systems-grade + DDD 1급), not "better". |
| "Pergyra = C# + Rust" | Loses §10 unique synthesis (DDD primitives). The third layer is the language. |
| "Like Go but typed" | Go is rejected lineage (§9). Channel similarity is incidental. |

## 12. Cross-References

- `docs/19_design_philosophy.md` §0 — core identity anchor; this lineage doc
  is the external coordinate of that identity
- `docs/106_ownership_model_comparison.md` — Tier 1-2 ownership detail
- `docs/114_async_model_positioning.md` — Tier 3 concurrency detail
- `docs/117_backend_strategy_positioning.md` — Tier 1 C+LLVM detail
- `docs/118_slot_model_rigor_audit.md` §8 — sister negative-space (vocabulary)
- `docs/104_air_compiler_architecture.md` — Tier 5 MLIR sibling-IR pattern
- Memory: `project_systems_language_identity.md`,
  `project_backend_strategy.md`, `project_functor_hkt_stance.md`,
  `feedback_async_coloring_framing.md`,
  `feedback_marketing_language_drift.md`,
  `project_lineage_synthesis.md`
