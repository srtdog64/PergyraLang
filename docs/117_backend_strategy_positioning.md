# Backend Strategy Positioning — Abstraction Portability

Last updated: 2026-04-26

Related documents:

- `docs/38_c_macro_deception_and_abi.md` — ABI integrity, why C macros mislead
- `docs/39_test_driven_abi_and_explicit_lowering.md` — backend-compare gating
- `docs/40_lowering_rules.md` — RIR → MIR mapping rules
- `docs/106_ownership_model_comparison.md` — sister positioning doc for ownership
- `docs/114_async_model_positioning.md` — sister positioning doc for concurrency
- `docs/113_memory_concurrency_model.md` — frozen beta concurrency contract
- `docs/118_slot_model_rigor_audit.md` — sister audit doc; Slot vs borrow-check rigor and marketing-language guide

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
modeling, AI orchestration, and games all live or die on this guarantee:
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

## 6. Beta and 1.0 Promise

### 6.1 Beta Closure (Stage 4)

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

### 6.2 1.0 Promise

For 1.0:

- Linux x86_64 × {LLVM, C} — full regression, primary support
- Windows x86_64 × {LLVM, C} — full regression, primary support
- macOS arm64 × {LLVM, C} — best-effort, community-tested
- Linux arm64 × {LLVM, C} — best-effort, community-tested

Backend-compare regression is the abstraction-portability invariant
proof. If the gate is green, the abstractions mean the same thing on
every gated platform. That is the 1.0 promise, expressible as a CI
status.

### 6.3 Out of 1.0

- WASM
- Native embedded (AVR / small Cortex-M / RISC-V tiny)
- Apple non-arm64 platforms
- BSD family
- Mainframe / exotic ISAs

These are reachable through the dual-emit architecture without
language-side work, but require runtime ABI porting (arena layout,
fiber scheduler, panic handler, syscall surface) and platform-specific
test infrastructure. Post-1.0 ecosystem work, not language work.

## 7. Summary

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
