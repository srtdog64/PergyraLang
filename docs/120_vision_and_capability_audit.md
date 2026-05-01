# Vision and Capability Audit — Current vs Aspirational

Last updated: 2026-05-01

Anti-hype rule (2026-04-29):

- External wording must never be stronger than the narrowest implemented and
  tested contract.
- Avoid absolute claims unless the sentence names the exact scope and evidence
  source.
- Forbidden without scoped evidence: "Rust-equivalent memory safety", "zero-cost",
  "production-ready", "AI-first language", "mathematically proven",
  "memory safe", "100% complete", "fully deterministic", and
  "no runtime overhead".
- Prefer: "implemented for the frozen beta subset", "small-test-covered;
  real-program evidence pending", "runtime-validated", "compile-time rejected
  for the covered boundary cases", and "proof obligation documented;
  mechanized proof not claimed".

Related documents:

- `docs/19_design_philosophy.md` §0 — core identity (systems language with domain extensions); this doc separates *current* from *vision* against that identity
- `docs/00_vision.md` — vision narrative (intent-first, agent-readable contracts)
- `docs/118_slot_model_rigor_audit.md` §8 — sister negative-space (vocabulary)
- `docs/119_pergyra_lineage_positioning.md` §11 — sister negative-space (lineage)
- `docs/121_types_as_domain_medium.md` — sister positioning; type system as the syntactic machine of lost-meaning recovery (this doc's §1 forbidden-claim list governs §7 vision territory of docs/121)
- `docs/122_managing_intent_drift.md` — sister positioning; drift management discipline (this doc's §4 vision items appear as §5.3 in docs/122)
- `docs/100_beta_readiness_checklist.md` — formal closure status

This document is the **third negative-space pair**. Where `docs/118` §8 audits
vocabulary and `docs/119` §11 audits lineage, this doc audits **capability
claims** — what Pergyra *actually does at beta* versus what is *aspirational
trajectory*. The intent: stop external descriptions from drifting from "we
are working on this" into "we already do this."

The principle is from `docs/00_vision.md`: **"인간도 잘 쓰면 좋고"** — Pergyra
is intent-first, not AI-first. By extension: Pergyra is honest-first, not
ambition-first. Vision lives openly here so it cannot be quoted as if it
were a current feature.

## 0. The Audit Discipline — Three-Pair Negative-Space Protocol

Before any external description (README, blog, marketing, comparison glossary,
academic write-up), check all three:

| Pair | Concern | Anchor |
|---|---|---|
| Vocabulary | "Is this phrasing honest about static / runtime / proof?" | `docs/118` §8 |
| Lineage | "Is this comparison honest about which language we descend from?" | `docs/119` §11 |
| Capability | "Is this feature claim honest about current vs vision?" | `docs/120` (this doc) |

Failing any one of the three produces external description that either
overstates the language or misplaces it on the language tree. All three
must pass.

## 1. Current vs Vision — The Five Dangerous Claims

These five are the most likely overclaim drift surfaces. Each has a
**current** state and a **vision** trajectory. **External description must
cite the current state, not the vision, unless the vision is explicitly
labeled as future.**

### 1.1 "AI-first language"

| Layer | State |
|---|---|
| **Current (beta)** | No AI-specific compiler features. `intent` primitive supports agent-readable contracts (per `docs/00_vision.md`); that is intent-first, not AI-first. |
| **Vision (post-beta)** | `pgy.accel.spray` module reserved for AI/GPU surface. TPU support reserved (memory: `project_tpu_support_future.md`). LLVM → MLIR/StableHLO path candidate. |
| **Honest external phrase** | "Intent-first; agent-readable contracts; AI-first surface reserved for post-beta module." |
| **Forbidden phrase** | "AI-first language." (Reframed in `docs/00_vision.md`: "Pergyra는 AI-first가 아니라 intent-first다.") |

### 1.2 "Quantum-ready"

| Layer | State |
|---|---|
| **Current (beta)** | `QubitSlot` / `ClaimQubit` / `Measure` / `Entangle` exist as a partial v2/experimental surface. No quantum lowering, no quantum runtime. README/README_ko already mark this honestly. |
| **Vision (long-term)** | Slot model designed to *generalize* to qubit-era resource semantics (memory: `project_quantum_vision.md`). When real quantum hardware/lowering matures, the resource abstraction is in place. |
| **Honest external phrase** | "Slot model anticipates qubit-era resource semantics; current quantum surface is partial / experimental." |
| **Forbidden phrase** | "Quantum-ready language" / "Quantum-safe systems language." Both imply current capability. |

### 1.3 "Distributed safety"

| Layer | State |
|---|---|
| **Current (beta)** | `RemoteFuture<T>` and `Channel<T>` cross-World transfer rule are *designed and partially enforced* at semantic level (memory: `project_distributed_design_decisions.md` four design principles). |
| **Vision (post-beta)** | Distributed runtime, supervision integration, network protocol bindings — none implemented. |
| **Honest external phrase** | "Designed for cross-World transfer with `Result`-mandatory failure model; distributed runtime is post-beta work." |
| **Forbidden phrase** | "Distributed-safe by construction" without the runtime. |

### 1.4 "Industrial automation fitness"

| Layer | State |
|---|---|
| **Current (beta)** | C# heritage (Tier 0 father in `docs/119`) + language designer's industrial SW background (memory: `user_industrial_software_context.md`). No PLC / SCADA / Modbus / fieldbus integration. |
| **Vision (long-term)** | Industrial integration libraries (post-1.0). Killer use case alignment (memory: `project_killer_usecase_dungeon_crawler.md` — game and industrial control share Pergyra primitive shape). |
| **Honest external phrase** | "Designed with industrial control as a target workload; integration libraries are post-1.0." |
| **Forbidden phrase** | "Industrial-grade language" without the integration story. |

### 1.5 "Multi-paradigm OOP+FP+DOP"

| Layer | State |
|---|---|
| **Current (beta)** | OOP (subject/class), async/await, generics with monomorphization, pattern matching — implemented and tested. FP coverage (LINQ-style pipelines, `with` expressions, immutable composition) is partial. DOP (data-oriented design) is reserved as `pgy.compat.dop` post-beta module. |
| **Vision (post-beta)** | Full FP idiom coverage and `pgy.compat.dop` DOP module. |
| **Honest external phrase** | "Multi-paradigm trajectory: OOP and async-generic core implemented; FP idioms partially landed; DOP reserved as post-beta module." |
| **Forbidden phrase** | "Full multi-paradigm OOP+FP+DOP" without the partial-coverage caveat. |

## 2. Implementation Gaps — Already Honestly Framed (Do Not Re-Hype)

These are gaps the language already documents honestly. Do not let external
description re-inflate them.

| Gap | Honest source | Drift to avoid |
|---|---|---|
| WriteView<T> aliasing-XOR-mutability not enforced | `docs/118` §6.1 | "Rust-style aliasing safety enforced" |
| Bare-metal trajectory marked 🔴 | `docs/19` §0.3 | "Bare-metal capable" / "freestanding-ready" |
| Runtime optional 🟡 (fiber/arena default) | `docs/19` §0.3, §0.4 (위험 2) | "Zero-runtime" / "no runtime overhead" |
| Compile-time determinism unverified | `docs/19` §0.3 | "Fully deterministic codegen" |
| CFG body dataflow ~70% complete | `docs/118` §6.4, `docs/103` | "Full ownership flow analysis" |
| Multi-span diagnostic API: 0% | `docs/100` §0g | "Rich multi-span diagnostics" |
| Generic param ownership classifier conservative | `docs/118` §6.2 | "Full generic borrow inference" |

For each, the honest substitution is the source doc's own framing. When in
doubt, quote the source verbatim.

## 3. Evidence-Thin Surfaces — Designed but Not Stress-Tested

These are *not* hype; the design is real. But the *evidence* that the design
holds in real programs is thin. External description should cite design, not
field validation.

| Surface | Design state | Evidence state |
|---|---|---|
| `intent = reward` primitive | Specified in `docs/00_vision.md` and grammar | No real-program runtime impact measurement |
| AIR drift detection | Phase 1 implemented (Intent + Boundary nodes) | No documented case of AIR catching a real production bug |
| Security audit P0 (5 items) | Contracts written in `docs/security/` | Audits not yet executed |
| Dual-emit C/LLVM parity | Parity gate exists in test infra | Real-program drift behavior not measured at scale |
| Channel-only cross-World rule | Reject path in semantic + 132/132 regression | "Real distributed program survives" not yet demonstrated |

**Honest external phrase pattern**: "Designed and small-test-covered;
real-program evidence pending."

## 4. The Vision Section — Where Aspiration Lives Openly

This section exists so that external writers who *want* to talk about
Pergyra's ambition have a place to source it from, *labeled as ambition*.
Do not extract these into capability claims.

### 4.1 Long-term Vision (10-year horizon)

- **Same abstraction on every machine** — `intent` / `zone` / `world` /
  `authority` / `handoff` semantics that survive lowering to any backend
  Pergyra targets, including future ones (MLIR, StableHLO, qubit IR).
  Source: `docs/117` §1 abstraction portability thesis.
- **Agent-readable contracts** — `intent` blocks with `requires` / `authorized
  by` / `guard` / `compensate` clauses that an AI agent can both *issue* and
  *audit*. Source: `docs/00_vision.md`.
- **Industrial integration ecosystem** — PLC / SCADA / Modbus / fieldbus
  bindings as standard libraries. Source: language designer's industrial SW
  background.
- **Quantum resource generalization** — Slot model serving qubit-era
  resource semantics when hardware matures. Source: memory
  `project_quantum_vision.md`.
- **Mechanized proof for core safety** — Level 4-5 proof trajectory for
  Anchored Ownership Safety and related theorems. Source: `docs/118` §9.

### 4.2 Mid-term Trajectory (post-beta, pre-1.0)

- AIR Phase 2 — Constraint Node, Effect Node, Drift Fact (`docs/104` §3.3).
- Option C lift — WriteView aliasing-XOR-mutability enforcement
  (`docs/118` §6.1).
- AI/GPU surface — `pgy.accel.spray` module landing.
- DOP idiom — `pgy.compat.dop` module landing.
- Render/shader — `pgy.render.skia` module landing.
- TPU/MLIR backend exploration — memory
  `project_llvm_syscall_todo.md`,
  `project_tpu_support_future.md`.

### 4.3 Beta-target (current closure work)

See `docs/100_beta_readiness_checklist.md` for the authoritative list.
Do not list beta-target items here — they belong in the closure checklist,
not in vision, because they are *contractually scheduled*, not aspirational.

### 4.4 Post-BETA Trajectory — Self-Host (committed sequence, partial recommended)

**Status (2026-05-02):** Upgraded from *aspirational* to *committed
sequence*. BDFL declared the explicit progression: BETA closure → final
dogfood → BETA+ self-host start. Recorded in `TODO.md` "★ Core Goal" and
memory `project_no_self_host_decision.md`. The *partial self-host*
recommendation below remains the recommended scope; full self-host is
not assumed inevitable.

**Why post-BETA, not pre-BETA**

Pergyra's positioning (transactional saga, DDD primitive 1급) does *not*
require self-host for credibility with its target audience. The pre-BETA
position would distract from closure work; the post-BETA position lets
us *use BETA closure + dogfood as the entry qualification* — i.e., the
language must demonstrate it can express its target domains before being
asked to express its own compiler. C + LLVM dual-emit per `docs/117`
remains the parallel-stable foundation throughout.

**Why slot makes self-host tractable (2026-05-02 reflection)**

Rust's self-host journey was tax-loaded by lifetime annotation: every
function signature in the rustc compiler had to commit to a lifetime
proof, and the proofs cascaded. Pergyra intentionally removed lifetime
annotation as a user-facing surface, replacing it with the slot model
(generational refs, runtime-validated handles, Vale lineage). That
choice — recorded in `docs/118` §6 negative-space and
`docs/119` §10 substrate borrow — pays back specifically here:
self-hosting Pergyra in Pergyra does *not* require encoding a borrow
proof for every compiler internal. The compiler can express its data
flow with `slot` handles without paying the lifetime tax that rustc
paid. **The decision was scoped to user-language ergonomics; the
self-host ergonomics gain is a positive externality.**

**Prerequisite — current Pergyra has the building blocks**

Pergyra already supports the structural minimum for compiler self-host:

- payload-bearing `enum` (sum types) — see `examples/party_system_demo.pgy`
  `enum Effect { StatBoost(stat: String, amount: Int) ... }`
- exhaustive `match` with `case` patterns — see
  `examples/bsd_packet_server/main.pgy` `match pt { case Connect: ... }`
- `Result<T>` mandate matches every parse / type / codegen failure point
- `slot<T>` provides stable AST handles (generational refs, like Vale)
- `extern "C"` provides FFI to libllvm

Missing for full self-host (the actual gap, recorded honestly):

- Rich string slicing / interning stdlib (currently lean)
- Mature debugger story (Pergyra-debug-Pergyra requires non-trivial
  investment)
- Paired Pergyra↔C reference examples for compiler-shaped patterns
- Stable C escape hatch policy (so partial self-host is recoverable)

**Recommended trajectory — partial self-host first**

The natural sequence, *if and when* post-1.0 self-host begins:

| Stage | What moves to Pergyra | What stays in C | Estimated effort |
|---|---|---|---|
| **Stage 1** | Lexer + Parser | Type checker, IR, codegen | ~3 months |
| **Stage 2** | Type checker | IR, codegen | +5 months |
| **Stage 3** | HIR/MIR/AIR passes | Codegen, LLVM C API wrap | +4 months |
| **Stage 4** | C backend codegen | LLVM C API wrap | +2 months |
| **Stage 5 (full self-host)** | LLVM backend codegen + LLVM C API wrap | — | +4 months |

**Stage 1-3 is "partial self-host"**; Stage 5 is full. *Partial is
recommended as the long-term steady state.* Keeping LLVM C API wrap in C
preserves: (a) external tooling help that knows the LLVM C API directly,
(b) bootstrap robustness (codegen never depends on a Pergyra-built
compiler), (c) LLVM upgrade path (LLVM C API churn handled in C, not in
re-traversed Pergyra wrappers).

**Honest external phrase pattern**:

- ✅ "Self-host is a post-1.0 aspiration; current implementation is C +
  LLVM dual-emit per `docs/117`."
- ✅ "Pergyra has the structural minimum (payload `enum`, `match`,
  `Result`, `slot`, `extern "C"`) for partial self-host. Realization is
  post-1.0."
- ❌ "Self-hosted compiler" / "Pergyra is written in Pergyra" /
  "Self-hosting language" — *all forbidden as current claims*.

**Risks to record openly**

- **Bootstrap fragility** — once compiler depends on prior compiler
  build, regressions cascade. Stage 5 requires an N-1 stage equivalence
  protocol before merge.
- **External tooling degradation** — Claude / other LLM assistants have
  rich training data on C/C++/Rust compiler internals; have *zero*
  training data on Pergyra. Each session bootstraps Pergyra context from
  scratch. Self-host shifts the help equation toward the user.
- **Distraction cost** — same person-time spent on stdlib (server
  backend per TODO §0b, WebGL per TODO §0a) compounds into ecosystem
  value. Self-host compounds into *language credibility* but not *user
  value*. The trade-off must be made consciously.

**Trigger for entry timing (post-BETA, post-dogfood)**

Self-host work *begins* after BETA closure + final dogfood (committed
sequence). The *scope* (Stage 1-3 partial vs Stage 5 full) is decided
when at least *one* of these holds:

1. Real users (not core team) request compiler extension capability —
   plugins, custom passes, embedded DSLs.
2. C escape hatch becomes a maintenance liability (LLVM C API churn
   exceeds wrap maintenance budget).
3. A specific Pergyra-only feature emerges that *cannot* be expressed
   cleanly in C-host but *can* be expressed in Pergyra-host (no candidate
   currently identified — recorded as a possibility, not a forecast).

If none of the three hold by year 2 post-BETA, **partial self-host
remains the final form**. Full self-host is not assumed inevitable —
the slot model removes one of the largest costs (vs Rust's lifetime
tax), but other costs (debugger story, bootstrap protocol, external
tooling degradation) remain real. Partial is honest steady state.

## 5. The Three-Layer Composition Reminder

`docs/119` §10 frames Pergyra as a three-layer composition:

```
(Tier 0: C# shape) ∘ (Tier 1-5: systems substrate borrow) ∘ (DDD primitive 1급)
```

Each layer has a *current state* and a *vision*:

| Layer | Current (beta) | Vision (post-beta+) |
|---|---|---|
| Tier 0 — C# shape | Async/await + generics + property + pattern match implemented; full LINQ pipeline / records partial | Full multi-paradigm coverage matching modern C# 9+ surface |
| Tier 1-5 — Substrate | Smaller static subset than Rust; CFG ~70%; AIR Phase 1; runtime capability checks | Stronger Pergyra-specific proof story for one core theorem (post-1.0), not Rust equivalence |
| DDD primitive 1급 | intent / authority enforced via semantic + CFG; zone / world / handoff partially enforced | All five DDD primitives with full enforcement and AIR-tracked drift |

**Honest external phrase**: "Three-layer composition; each layer is at a
known maturity that you can read in `docs/120`."

## 6. Marketing / Comparison Phrases — Combined Negative-Space Index

This combines `docs/118` §8 + `docs/119` §11 + this doc § 1-2 into one
index. When publishing external description, search the publication text
for these phrases and substitute the honest version.

| Forbidden | Honest |
|---|---|
| "AI-first language" | "Intent-first; AI surface is post-beta module" (`docs/120` §1.1) |
| "Quantum-ready" | "Slot anticipates qubit-era; current quantum surface partial" (§1.2) |
| "Distributed-safe" | "Cross-World rule designed; distributed runtime post-beta" (§1.3) |
| "Industrial-grade" | "Industrial integration post-1.0; design-level fitness only" (§1.4) |
| "Full multi-paradigm" | "OOP+async core; FP partial; DOP reserved" (§1.5) |
| "Rust-equivalent memory safety" | `docs/118` §8 row 1 |
| "Slot is a borrow checker" | `docs/118` §8 row 2 |
| "Aliasing-XOR-mutability enforced" | `docs/118` §8 row 4 |
| "No data races possible" | `docs/118` §8 row 6 |
| "Rust-like language" | `docs/119` §11 row 1 (C# father, Rust substrate) |
| "C# clone with Rust syntax" | `docs/119` §11 row 2 |
| "Better C#" | `docs/119` §11 row 3 |
| "Pergyra = C# + Rust" | `docs/119` §11 row 4 (loses unique synthesis) |
| "Bare-metal capable" | "Bare-metal trajectory marked 🔴 in `docs/19` §0.3" |
| "Zero-runtime" | "Runtime optional 🟡; fiber/arena are default" (§2 row 3) |
| "Fully deterministic codegen" | "Compile-time determinism baseline; verification pending" (§2 row 4) |
| "Self-hosted compiler" | "Self-host is post-1.0 aspiration; current implementation is C + LLVM dual-emit" (§4.4) |
| "Pergyra is written in Pergyra" | "Compiler implementation in C with LLVM C API; partial self-host is post-1.0 ambition per §4.4" |
| "Self-hosting language" | "Post-1.0 trajectory; current state is C + LLVM dual-emit per `docs/117`" |

## 7. How To Use This Doc

For external writers (blog posts, comparison glossaries, academic write-ups,
README revisions, marketing copy):

1. Draft the description first.
2. Search for any phrase matching `docs/120` §6 forbidden column. Replace
   with the honest column.
3. For any specific feature claim, verify against `docs/120` §1 or §2 — does
   it describe current or vision? If vision, label it as "post-beta" or
   "post-1.0" or "long-term."
4. For any lineage / parent-language claim, verify against `docs/119` §2
   tier classification.
5. For any vocabulary about static guarantees, verify against `docs/118`
   §8.

If a claim cannot be sourced from current implementation per `docs/100`,
either (a) move it to §4 vision and label it as such, or (b) remove it.

## 8. Cross-References

- `docs/19_design_philosophy.md` §0 — core identity (this doc separates
  current from vision against that identity)
- `docs/00_vision.md` — intent-first vision narrative
- `docs/100_beta_readiness_checklist.md` — formal closure status (authoritative
  current state)
- `docs/118_slot_model_rigor_audit.md` §8 — vocabulary negative-space (sister)
- `docs/119_pergyra_lineage_positioning.md` §11 — lineage negative-space
  (sister)
- `docs/106_ownership_model_comparison.md` — Tier 1-2 ownership detail
- `docs/114_async_model_positioning.md` — Tier 3 concurrency detail
- `docs/117_backend_strategy_positioning.md` — Tier 1 backend detail
- `docs/104_air_compiler_architecture.md` — AIR Phase 1/2 split
- Memory: `feedback_marketing_language_drift.md`,
  `feedback_capability_overclaim_audit.md`,
  `project_systems_language_identity.md`,
  `project_lineage_synthesis.md`,
  `project_quantum_vision.md`,
  `project_tpu_support_future.md`,
  `project_distributed_design_decisions.md`,
  `project_killer_usecase_dungeon_crawler.md`
