# Backend Strategy Positioning — Abstraction Portability

Last updated: 2026-04-26

Related documents:

- `docs/19_design_philosophy.md` §0 — **core identity** (systems language with domain extensions); this doc is one of the layers above that baseline
- `docs/38_c_macro_deception_and_abi.md` — ABI integrity, why C macros mislead
- `docs/39_test_driven_abi_and_explicit_lowering.md` — backend-compare gating
- `docs/40_lowering_rules.md` — RIR → MIR mapping rules
- `docs/106_ownership_model_comparison.md` — sister positioning doc for ownership
- `docs/114_async_model_positioning.md` — sister positioning doc for concurrency
- `docs/113_memory_concurrency_model.md` — frozen beta concurrency contract
- `docs/118_slot_model_rigor_audit.md` — sister audit doc; Slot vs borrow-check rigor and marketing-language guide
- `docs/119_pergyra_lineage_positioning.md` — sister positioning doc for language lineage (C# father, Tier 1-5 substrate borrow, DDD unique synthesis)
- `docs/120_vision_and_capability_audit.md` — sister audit; capability negative-space + current-vs-vision separation
- `docs/121_types_as_domain_medium.md` — sister positioning; type system as the syntactic machine of lost-meaning recovery (carrier / coherence / negative-space)

This document positions Pergyra's backend strategy. It is a **positioning /
rationale** doc, not a contract. Concrete ABI freezes live in
`docs/38` / `docs/39` / `docs/40` and the `pgy_abi_spec.h` static_assert
table.

## 1. The Problem Pergyra Solves — Abstraction Portability

Most production languages solve one of two problems:

- **Performance portability**: same code runs fast on every platform
  (Rust, Zig, C++).
- **Surface portability**: same source compiles on every platform
  (Java, Go, .NET).

Pergyra solves a third, less commonly stated problem:

> **Abstraction portability**: high-level domain abstractions —
> `intent`, `world`, `zone`, `pin`, `effect`, `Channel`, `parallel` — must
> mean the **same thing** on every backend and every platform.

The phrasing matters. It is not enough that code *runs* on every
platform. It is not enough that code is *fast* on every platform. The
shape and observable behavior of `intent` step ordering, `pin` boundary
rules, `effect` propagation, and `Channel` happens-before must not
silently change when the program is lowered to a different backend.

Industrial control, distributed business systems, transactional domain
modeling, AI orchestration, and games all depend on this contract:
when the same Pergyra source produces different observable behavior on
two backends, the user-facing abstraction has been broken, regardless
of whether the program ran or how fast.

## 2. Why C + LLVM Dual Emit

The portability problem is unsolvable from a single backend, because the
language has no second reference to check itself against. Any single
backend is its own oracle: whatever it does is by definition the
language's behavior.

Pergyra emits **both C and LLVM IR** from the same MIR, and gates every
release on backend-compare regression: identical Pergyra source must
produce **byte-identical observable output** on both backends for the
frozen subset.

This makes each backend an oracle for the other. If a new abstraction
relies on a behavior that exists only in LLVM IR, the C backend
exposes it. If it relies on a C-only quirk, the LLVM backend exposes
it. The abstraction must be expressible in both — which is the same
as saying the abstraction does not depend on backend specifics.

The two backends are not interchangeable. They have different roles.

### 2.1 LLVM as Performance Primary

LLVM IR is the performance path:

- LLVM's optimization pipeline is the most mature in the industry.
- LLVM IR carries information that C cannot carry without UB tricks
  (aliasing facts, value provenance, function attribute granularity,
  precise calling conventions).
- High-level languages emitting LLVM IR directly typically run 5–15%
  faster than the same language emitting C and re-parsing it through
  a C compiler.
- LLVM target list covers x86_64, arm64, riscv, wasm, ppc, mips, and
  more.
- Debug info (DWARF, CodeView via PDB) is LLVM's responsibility.

Pergyra's LLVM backend is the default path on platforms where LLVM is
the natural toolchain (Linux, Windows MSVC/MinGW, macOS, WASM, server
ARM).

### 2.2 C as Compatibility Floor

The C ABI is the universal lingua franca of computing:

- Every operating system has a C compiler.
- Every language interoperates with other languages through the C ABI.
- Every embedded MCU toolchain emits C as the lowest common denominator
  (AVR, small Cortex-M, RISC-V tiny, exotic DSPs).
- Mainframes, retro/console SDKs, and obscure embedded environments
  often only have a C compiler, not an LLVM target.
- WebAssembly System Interface (WASI) and most embedded RTOS APIs
  expose C interfaces.

Pergyra's C backend is the **escape hatch** that keeps the language
viable on platforms where LLVM is incomplete, unsupported, or
politically unavailable. It is also the primary FFI surface: anything
that links against Pergyra from another language sees the C ABI.

### 2.3 Parity Gate as Abstraction Invariant Proof

Backend-compare regression (`make llvm-test-backend-compare`,
`make air-backend-nonimpact-full-test-smoke`) executes hundreds of
fixtures on both backends and rejects any divergence in observable
output. This gate is the executable form of the abstraction-portability
invariant. Without it, "the abstractions mean the same thing" is a
claim. With it, the claim is regression evidence on every PR.

The gate is load-bearing: when a future Pergyra abstraction is proposed
that cannot be expressed identically on both backends, the gate forces
the question — *"is this still the same language?"* — to be answered
explicitly, not absorbed silently.

## 3. What Dual Emit Enables

### 3.1 New Platforms for Free

A platform is supportable if it has either a C compiler **or** an LLVM
target. The intersection of those two sets is approximately "every
platform that runs code in 2026."

- LLVM-only platforms: WASM, exotic ISAs through LLVM Bitcode
- C-only platforms: AVR, small Cortex-M, retro consoles, mainframes,
  RTOS SDKs without LLVM ports
- Both: x86_64, arm64, riscv64, ppc, MIPS — Pergyra picks the faster
  path

Adding a new platform does not require adding a new backend, in
contrast to Zig (which writes its own per-target backend) or Go (which
maintains its own platform support matrix).

### 3.2 WASM and Embedded Become Reachable

WebAssembly is an LLVM target. Pergyra's LLVM backend lowers to WASM
without additional Pergyra-side work, modulo runtime ABI portability
(arena, fiber scheduler) which is a finite engineering task.

AVR / small Cortex-M / freestanding ARM with limited LLVM support reach
through the C backend. The abstractions remain the same; the runtime
ABI shrinks (no fibers in 4KB RAM, no `world` runtime) but `intent`,
`pin`, `effect` mask, and structured control flow keep their meaning.

### 3.3 Language Evolution Is Self-Checked

When a new language feature is proposed, the dual-emit constraint forces
a structural question: *can this be lowered to both C and LLVM IR with
identical observable behavior?* If not, the feature either:

- depends on a backend specific quirk (reject),
- needs a runtime ABI extension (acceptable, but documented as a
  language-level invariant, not a backend trick), or
- needs to be reduced to primitives that already lower cleanly
  (preferred).

This is an unconscious-bias filter. Without it, language designers
gravitate toward features that are easy on their primary backend.
With it, the language stays balanced.

### 3.4 User Promise Is Simple

> *Your code does not behave differently on different platforms.
> The abstractions mean the same thing.*

This is a one-sentence promise that industrial-control, financial,
distributed-systems, and game users care about more than they care
about peak performance or shortest compile time. Java sells the
"write once, run anywhere" version of this with a heavy VM. Pergyra
sells the same promise without a VM, by making the abstractions
themselves backend-invariant.

## 4. What Dual Emit Costs

### 4.1 LLVM Compile Speed Is Inherited

LLVM is slow. Optimized builds are dominated by LLVM optimization
passes. Pergyra inherits this slowness on its LLVM path. Game
developers and incremental-build workflows feel this. This is one of
the motivations Zig cites for moving off LLVM.

Mitigations:

- C backend can serve as a faster debug path on developer machines
  with a fast C compiler (tcc, clang -O0).
- A future Pergyra-native debug backend (post-1.0) is not ruled out,
  but is explicitly out-of-beta scope.

### 4.2 Dual Backend-Compare Regression Is Expensive

Every PR runs both backends, executes both binaries, and diffs
observable output. CI minutes are double what a single-backend
language pays. Test fixtures must be authored in a way that produces
deterministic output (no timestamps, no PIDs, no ASLR-sensitive
addresses).

This is the price of the abstraction-portability invariant. It is
worth paying because the alternative — discovering on a customer's
embedded board that `intent` step ordering differs from the developer's
laptop — is unrecoverable in a 1-year stable freeze.

### 4.3 Some Abstractions Get Rejected

Features that are natural in LLVM IR but awkward in C (e.g., precise
GC roots, aggressive zero-cost exceptions, certain SIMD intrinsic
patterns) get rejected or reshaped. Features natural in C but
awkward in LLVM IR (e.g., complex macro-driven layout) likewise.

The intersection is smaller than either backend's full capability,
but it is the set of abstractions that *actually* portable. The cost
is paid in language design discipline, not at runtime.

### 4.4 Compiler Binary Is Heavy

LLVM dependency adds hundreds of MB to the compiler distribution.
Pergyra's compiler is not a single-binary lightweight tool the way
the Go toolchain or Zig is. This is acceptable for the target users
(developer workstations and CI), and the C backend allows a
LLVM-free distribution slice for embedded deployment, but the
"heavy compiler" cost is real.

### 4.5 ABI Surface Must Be Static-Asserted, Not Hoped For

Because two backends emit the same runtime ABI, neither backend can
silently drift. Pergyra enforces this with `pgy_abi_spec.h` and
`static_assert` over every ABI struct, plus `make test-abi-spec`
gating. Without this discipline, dual-emit would itself be the bug
source (the parity gate would catch divergence but not before it
shipped to a fixture).

## 5. Comparison Map

| Language | Backend | Platform story | Abstraction depth | Same abstraction everywhere? |
|---|---|---|---|---|
| C | per-platform `cc` | Universal | Thin (no ownership/effects/intent) | Yes, but abstractions are too thin to matter |
| C++ | LLVM, GCC, MSVC | Most | Heavy | Multiple compilers means multiple subtly-different languages |
| Rust | LLVM (+ GCC WIP) | LLVM-supported | Heavy (ownership, lifetimes) | Yes on LLVM-supported, missing elsewhere |
| Go | Own backend (`gc`) | Go's supported list | Mid (channels, goroutines) | Yes on Go's list, no embedded/freestanding |
| Zig | LLVM + own backend (WIP) | Universal (Zig's goal) | Thin–mid (`comptime`) | "Be C, with comptime" — abstraction layer thin by design |
| Crystal | LLVM | LLVM-supported | Heavy (Ruby-like) | LLVM only, no C-only embedded |
| Nim | C | Universal-via-C | Mid | Yes on any C target, but no LLVM perf path |
| Swift | LLVM | Apple + Linux | Heavy | LLVM only, Apple-leaning ecosystem |
| Java / Kotlin | JVM | JVM-supported | Heavy | Yes on JVM, missing embedded/native floor |
| **Pergyra** | **LLVM + C dual-emit, parity-gated** | **LLVM-supported ∪ C-supported** | **Heavy (intent / world / zone / pin / effect / Channel)** | **Yes — invariant is regression-tested on every PR** |

The bottom row is unique. No production language combines (a) heavy
domain abstractions and (b) dual-emit with parity gates. Crystal and
Nim each pick one backend; Rust and Swift trust LLVM alone; Go and Zig
own their backends but keep abstractions thin.

This is Pergyra's strategic position. The cost is real (§4) but the
position itself is empty in the language design space, and the user
promise (§3.4) is one no other language is currently making with
regression evidence behind it.

## 6. Self-Host Decision

### 6.1 Decision

**Pergyra does not self-host through 1.0.** The core compiler stays in
C, with LLVM IR and a dedicated C source backend as the two emit
targets. This is recorded here as an explicit, deliberate decision —
not a default-by-inertia.

This decision concerns **compiler frontend self-hosting only** (parser,
semantic analysis, IR pipeline, code emission written in Pergyra
itself). Backend choice — LLVM IR plus C source dual-emit — is a
separate, orthogonal decision (§2) and remains in place regardless of
self-host status. Both backends are external to Pergyra and stay
through 1.0. The two decisions interact only at the bootstrap question
("how do we compile the Pergyra compiler if it is written in
Pergyra?"), which is addressed in §6.6 below.

### 6.2 Reasons

1. **Backend targets are expressive enough for the current language plan.**
   There is no beta-planned language feature that is known to require a
   backend beyond LLVM IR or C source.
   Self-hosting would not unlock any new abstraction; it is a *meta*
   migration with no user-facing semantic gain.

2. **C is the universal compatibility floor.** Pergyra targets every
   computer and device that runs code. The C backend is the escape
   hatch for every platform LLVM does not support cleanly (AVR, exotic
   embedded SDKs, mainframes, retro platforms, RTOS without LLVM port,
   bizarre ISAs). Self-hosting would *not remove* this need; it would
   add a third backend (the Pergyra-self) that would still have to
   coexist with C. Net cost increase, no portability gain.

3. **LLVM is performance primary, beats C-via-cc.** LLVM IR direct emit
   typically runs 5-15% faster than the same logic emitted as C source
   and re-parsed by a C compiler, because LLVM IR carries aliasing,
   provenance, and inlining facts that C cannot express without UB
   tricks. A self-hosted Pergyra backend would have to reproduce
   LLVM-class optimization to match, which is a multi-year project Zig
   is currently inside (and not yet finished after 8 years).

4. **Self-host doubles beta closure cost.** Rewriting ~148K LOC of core
   compiler in Pergyra, even with AI multiplier, is a 6-12 month
   additional project on top of remaining Stage 4 marshaling. It does
   not move beta closer; it pushes beta further. The user promise of a
   1-year stable freeze depends on shipping beta on time.

5. **Zig may become the C-replacement standard.** If Zig reaches 1.0
   with stable ABI and ecosystem traction, the C backend slot can be
   re-evaluated for replacement (or augmentation) by a Zig backend.
   This is post-1.0 ecosystem-evolution territory, not a beta blocker.

### 6.3 What This Decision Concedes

Honestly:

- Compile speed inherits LLVM's slowness on the LLVM path. Zig is
  trying to fix this by self-hosting; Pergyra accepts the cost for
  beta and 1.0.
- Compiler binary is heavy (LLVM dependency hundreds of MB). Pergyra
  is not a single-binary lightweight tool the way Zig and Go are.
- Pergyra cannot claim "fully self-bootstrapped" credibility against
  Rust 1.0 (self-hosted), Crystal 1.0 (self-hosted), Nim 1.0
  (self-hosted), or TypeScript 1.0 (self-hosted). This is a marketing
  weakness, not a technical defect.
- Some Pergyra perf will be measurably slower than equivalent Rust on
  hot paths (Slot indirection cost — see `docs/118` §4.4 and the
  ~5ns generation/index lookup). LLVM IR direct emit closes most of
  this gap but not all of it.

### 6.4 What This Decision Buys

- Beta closure stays on schedule.
- The 148K-LOC core compiler stays maintained as a single C codebase
  rather than a dual codebase during a multi-year transition.
- LLVM's optimizer applies for free to all Pergyra programs without
  reinventing it.
- The C backend stays as the universal compatibility floor with no
  per-platform porting effort.
- Language design discipline is not distorted by "what features make
  the self-hosted compiler easier to write" pressure (the trap that
  Rust narrowly avoided pre-1.0 by maintaining the OCaml bootstrap
  long enough).

### 6.5 Revisit Triggers

The decision is open for re-evaluation when one of the following
becomes true:

- Pergyra reaches 1.0 and the 1-year freeze closes without unresolved
  ABI / runtime stability issues.
- Zig reaches 1.0 with stable ABI and clear C-replacement traction.
- LLVM signals deprecation of major Pergyra-relevant target families
  (very unlikely).
- A committed multi-person engineering team materializes that can
  carry both compilers in parallel during transition.

Until at least one of these is true, the decision stands. Discussion
of self-host before 1.0 should reference this section and explain why
the trigger conditions have changed.

### 6.6 Future Migration Path — Soft Then Hard

Self-hosting can be staged in two layers, and the cost/benefit is
different for each. This section records the planned sequencing if
re-evaluation triggers fire.

#### Soft self-host (compiler-adjacent tools)

Tools that are not the compiler core but live in the same ecosystem
can be written in Pergyra without bootstrap pain:

- `pgyfmt` — formatter
- `pgylint` — additional lint rules beyond compiler diagnostics
- Test runner / harness orchestrator
- Example checker (`examples/*.pgy` validity gate)
- Documentation generator (signature → doc extraction)
- Package manager / resolver (post-1.0)
- LSP server (post-1.0, after the protocol surface stabilizes)

Each tool is 200–2000 LOC. None depends on the compiler core. They
validate that Pergyra can handle compiler-style work (AST traversal,
diagnostic formatting, file IO, structured output) and they grow the
contributor pool of "people who know Pergyra" — both benefits of
self-hosting without the migration cost.

**Estimated effort**: 3–6 months, post-beta during the 1-year freeze.
**Risk**: low. Each tool is independent; failure of one does not block
the rest.
**Recommendation**: yes, do this. Soft self-host captures ~70% of the
self-host benefit at ~10% of the cost.

#### Hard self-host (compiler core in Pergyra)

The compiler core is currently ~148K LOC of C
(lexer + parser + semantic + compiler IRs + codegen + runtime).
Migrating it to Pergyra is substantial work.

**Reference points:**

- Go: written in C originally, migrated to self-hosted Go in 1.5 (2015)
  using an automated C-to-Go translator. Took ~3 years with a team.
- Crystal: bootstrapped from Crystal-on-Ruby. Years of dual maintenance.
- TypeScript: planned migration with Microsoft team.
- Zig: 8+ years, still in progress with a small team + community.
- Rust: OCaml → Rust frontend transition (~2 years), but Rust was
  pre-1.0 and could break itself to make migration easier — an
  advantage Pergyra would not have post-1.0.

**Estimated effort for Pergyra**:

| Phase | Description | Estimate |
|---|---|---|
| 1 | Soft self-host (above) — proves viability | 3–6 months |
| 2 | One isolated module rewrite (e.g., parser) | 6–12 months |
| 3 | Automated C-to-Pergyra translator (AI-assisted) | 3–6 months |
| 4 | Mass translation + dual maintenance | 6–12 months |
| 5 | Cutover, drop C compiler, ship Pergyra-self-hosted | 3–6 months |
| **Total** | **Hard self-host completion** | **~21–42 months** |

That is 2–3.5 years dedicated, on top of beta and the 1-year freeze.

**Required Pergyra extensions** (likely):

- First-class function pointers / closures with stable ABI for visitor
  patterns and callback registries used pervasively in compiler code.
- Custom hash key types beyond `String`/`Int` (or a Slot-based map) for
  symbol tables and dedup data structures.
- Possibly some metaprogramming or codegen-time helpers for
  boilerplate reduction.

If 1.0 is frozen, these extensions become 1.x or 2.0 features —
self-host frontend cannot land before they do.

**Risk factors:**

- Solo + AI maintaining dual codebases during phases 4–5 is high
  cognitive load. Realistic only if at least one part-time
  collaborator has joined by then.
- Translation may surface latent bugs in the 148K-LOC C compiler that
  were silent under C semantics; each is a stop-the-world fix.
- A frozen language cannot evolve to make compiler-writing easier;
  Rust's pre-1.0 advantage is gone.
- Opportunity cost: 2–3 years on self-host is 2–3 years not on LSP
  polish, ecosystem libraries, user growth, research, or new language
  features.

**Recommendation:**

- **Do soft self-host** during the 1-year freeze. Low cost, high
  signal, contributor pool grows.
- **Defer hard self-host** until at least one revisit trigger from §6.5
  fires AND a multi-person team or strong community contributor base
  has materialized. Solo + AI completion of hard self-host is
  technically possible but the opportunity cost is severe.
- **Honest alternative**: stay on C indefinitely, like Nim does (Nim's
  compiler emits C and stays there). Pergyra's dual-emit + parity
  invariant means staying on C is not a marketing weakness once that
  is itself the documented strategy.

The migration is **possible**; the question for any future Pergyra
maintainer is whether it is the highest-leverage use of 2–3 years of
effort given Pergyra's stated targets (industrial / distributed /
games / AI orchestration). The current author's assessment is: not at
1.0, possibly at 2.0, and only if the team has grown.

## 7. Beta and 1.0 Promise

### 7.1 Beta Closure (Stage 4)

The frozen platform matrix for beta is:

| Platform | LLVM backend | C backend | Status |
|---|---|---|---|
| Linux x86_64 | ✅ regression-gated | ✅ regression-gated | Primary CI |
| Windows x86_64 | 🟡 regression-gated, partial fixtures | 🟡 regression-gated, partial fixtures | Stage 4 blocker |
| macOS arm64 | 🔴 best-effort | 🔴 best-effort | Post-beta lift |
| Linux arm64 | 🔴 best-effort | 🔴 best-effort | Post-beta lift |
| WASM | ⚪ design only | n/a | Post-1.0 |
| Embedded (C-only) | n/a | ⚪ design only | Post-1.0 |

Stage 4 promotes Windows to full Linux-equivalent regression. macOS
arm64 is best-effort during beta — supported in the sense that bug
reports get fixed, but not regression-gated.

### 7.2 1.0 Promise

For 1.0:

- Linux x86_64 × {LLVM, C} — full regression, primary support
- Windows x86_64 × {LLVM, C} — full regression, primary support
- macOS arm64 × {LLVM, C} — best-effort, community-tested
- Linux arm64 × {LLVM, C} — best-effort, community-tested

Backend-compare regression is the abstraction-portability invariant
proof. If the gate is green, the abstractions mean the same thing on
every gated platform. That is the 1.0 promise, expressible as a CI
status.

### 7.3 Out of 1.0

- WASM
- Native embedded (AVR / small Cortex-M / RISC-V tiny)
- Apple non-arm64 platforms
- BSD family
- Mainframe / exotic ISAs

These are reachable through the dual-emit architecture without
language-side work, but require runtime ABI porting (arena layout,
fiber scheduler, panic handler, syscall surface) and platform-specific
test infrastructure. Post-1.0 ecosystem work, not language work.

## 8. Summary

Pergyra's backend strategy is not "support every platform" (Zig) and
not "trust LLVM" (Rust, Crystal, Swift). It is:

> **Make the abstractions backend-invariant by emitting two backends
> and gating every release on parity.**

LLVM is the performance primary. C is the compatibility floor. The
parity gate is the executable proof that `intent`, `world`, `zone`,
`pin`, `effect`, `Channel`, and `parallel` mean the same thing on
both. The user promise — "your code does not behave differently on
different platforms" — is regression-tested, not aspirational.

This costs LLVM's compile speed, dual CI cost, abstraction-design
discipline, and a heavy compiler binary. It buys a language position
that no production competitor currently occupies: heavy domain
abstractions with dual-backend portability invariants. For Pergyra's
target workloads — distributed business systems, transactional
modeling, AI orchestration, games, and industrial control — that
position is the right one.
