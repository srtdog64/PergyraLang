# 14. AIR Erasure Measurement — the dashboard with numbers

This document turns the "zero-cost vs. deferred-to-runtime" debate into **measured
numbers**. The honest thesis Pergyra makes is *not* "loss = 0"; it is **bounded,
measured, attributed loss**. World / Zone / Intent / Slot (and the domain
Lifecycle axis) are source-level semantic coordinates; the question is what
*physically survives* in the optimized machine code, and which axis each
surviving primitive belongs to.

Companion to [07_air_abstraction_safety.md](07_air_abstraction_safety.md) and
[09_abstraction_loss_contracts.md](09_abstraction_loss_contracts.md), which give
the AIR model; this doc is the empirical instrument.

## 0. What the critique gets right, and what it conflates

A common critique: "`src/runtime/slot_manager.c`, `world_roster.c`,
`slot_security.c` exist, therefore the semantics are not erased — proof failure is
being paid back as runtime overhead." Two things must be separated:

- **Existence of a runtime symbol ≠ a program paying for it.** A runtime
  function in the library is only a cost if a given compiled program *links and
  calls* it. The right test is per-program, not per-repository.
- **Three distinct kinds of runtime residue** that the critique collapses into
  one (see §4): (A) irreducible runtime facts, (B) by-design fail-closed
  evidence, (C) genuine static-analysis incompleteness. Only (C) is improvable
  debt. AIR's job is to *separate and count* them.

## 1. Methodology

**Fixtures** (`tests/air_erasure/fixtures/`): one isolated program per axis, with
a *provable* and an *ambiguous/irreducible* variant where the contrast matters.

**Two metrics per fixture:**

1. **Emitted (logical)** — count of `call @pgy_<axis>` in the *pre-link* LLVM IR
   (`--emit-llvm`). "How many named axis runtime operations the codegen produces."
   A positive number is not a leak: naming the operation is the traceability-first
   feature. It is the *input* to erasure, not the residue.
2. **Physical (survived)** — the external symbols that survive `gcc -O2` inlining
   of the emitted C, categorized by `nm -u` on the `.o`:
   - `Axis` — an axis runtime call that survived as an out-of-line call.
   - `Sync` — `pthread_*` / fiber primitives (concurrency).
   - `Heap` — `malloc`/`free`/`calloc`/`realloc`.
   - `Abort` — `abort`/assert (a fail-closed guard path).
   - `IO` — `printf`/`puts`/... (explicit `Log`/output the programmer wrote).

The static-inline runtime (`pgy_runtime_*` slot/lifecycle/zone-authority helpers)
folds into the caller at `-O2`, so what remains in `nm -u` is exactly the
*irreducible primitive the axis was standing for* — a `pthread_mutex` for a
channel, an `abort` path for a fail-closed guard, nothing for a pure slot read.

**Reproduce:** `pwsh tests/air_erasure/measure.ps1` → `tests/air_erasure/results.csv`.
Measured with the committed `bin/pgy.exe`, mingw `gcc -O2 -I src -I src/runtime`,
`nm`/`size` from the same toolchain.

## 2. The dashboard (measured)

`Emitted` = pre-link LLVM IR axis calls. The remaining columns are **physical**
survivors after `-O2`.

| Fixture | Axis emitted | **Axis (phys)** | Sync | Heap | Abort | IO |
|---|---:|---:|---:|---:|---:|---:|
| `00_pure_value` | 0 | **0** | 0 | 0 | 0 | 1 |
| `01_slot_provable_with` | 2 | **0** | 0 | 0 | 0 | 1 |
| `02_slot_provable_claim` | 3 | **0** | 0 | 0 | 0 | 1 |
| `03_secure_slot` | 0 | **0** | 0 | 0 | 0 | 1 |
| `04_channel_parallel` | 8 | **0** | 11 | 2 | 1 | 1 |
| `05_zone_intent` | 6 | **0** | 2 | 1 | 1 | 2 |
| `06_lifecycle_branch` | 2 | **0** | 0 | 0 | 1 | 0 |
| `07_lifecycle_linear` | 0 | **0** | 0 | 0 | 0 | 0 |

## 3. Headline reading

1. **The axis vocabulary is 100% compiled out.** `Axis (phys) = 0` in **every**
   fixture: no `pgy_world/zone/intent/slot/lifecycle` symbol survives `-O2` as an
   out-of-line call. The Slot/Zone/Intent/World/Lifecycle *names* never reach the
   machine. This is the erasure claim, verified.

2. **Provable cases reach near-zero.** `01`/`02` (straight-line value-Slot) emit
   2–3 named slot calls, of which **0** survive: pure load/store, and the always-on
   slot safety check is **DCE'd** (`Abort = 0`) because the optimizer proves the
   slot is never used-after-release. `07_lifecycle_linear` is **all zeros** — taint
   keeps a fully-provable lifecycle at literally no residue.

3. **Irreducible residue = exactly the primitive the axis stood for, not the
   abstraction leaking.** `04_channel_parallel` keeps **11 `Sync` + 2 `Heap`**:
   that is `pthread_mutex`/`cond` — what *any* implementation of "run these in
   parallel and pass a value over a channel" must cost. The `parallel{}` / `<-`
   syntax erased; the concurrency it denotes did not, because it cannot.

4. **The fail-closed guard is one attributed `Abort`, only at the genuine
   ambiguity.** `06_lifecycle_branch` keeps exactly `Abort = 1` — the
   `pgy_runtime_lifecycle_guard_export(..., "Capture", "Payment")` path, surviving
   because the state at the branch join is *genuinely* unknown at compile time.
   `07` (no branch) keeps 0. The residue is precisely scoped to the unprovable
   minority and labelled with `op`/`subject`.

## 4. A/B/C attribution (the three buckets, with evidence)

| Bucket | Meaning | Measured evidence | Erasable? |
|---|---|---|---|
| **A — irreducible runtime fact** | information that does not exist at compile time | `04` Sync=11, Heap=2 (concurrency); `world_roster` = who-is-alive | No — no compiler erases it; Rust pays the same (Arc/Mutex/std sync) |
| **B — by-design fail-closed evidence** | always-on safety tag kept for consistency/traceability, opt-out via raw mode | `05` Sync=2/Heap=1 = intent-trace machinery; slot gen-counter check (here DCE'd) | Yes — by policy (`PGY_RAW_SLOTS`) or when statically dischargeable |
| **C — static-analysis incompleteness** | a runtime check a stronger analysis could remove | `06` Abort=1 = lifecycle guard at a branch join | Partially — better dataflow shrinks it; the *true* ambiguous remainder is bucket A/B |

**Two cases worth the honesty caveat:**

- **`03_secure_slot`**: the emitted C *does* use the secure path
  (`pgy_secure_write_Int(&hp, 100, &token)`), yet it erases to `196`-byte
  load/store with `Abort = 0`. The token check folds because the token's
  provenance is **statically traceable** in this fixture. A token validated
  against a *runtime* authority (hardware register, external input) would retain
  the check — that is the genuine bucket-B residue, and needs a dedicated fixture
  to measure (see §6).
- **`05_zone_intent`**: the zone authority check is emitted as
  `pgy_zone_authority_validate_flags_export((battle!=NULL),(hero!=NULL),...)` and
  **folds** because the participants are statically non-null. The surviving
  `Sync=2/Heap=1` is the *intent trace/cleanup* machinery
  (`pgy_mir_cleanup_op_export`), i.e. bucket-B observability, not the zone axis.

## 5. The falsifiable per-program erasure contract

The strong "delete `slot_manager.c` and everything must still build" test is the
*wrong* test: it denies buckets A/B (a secure/device/cross-boundary program
*should* break without its runtime, because that runtime is irreducible). The
correct, falsifiable contract is per-program:

> **A program that uses only statically-provable value-Slots emits zero surviving
> axis calls and zero slot-safety abort paths in its `-O2` object.**

Rows `01`, `02`, `07` **satisfy** this (`Axis = 0`, `Abort = 0`). That is the
honest, checkable form of "the abstraction is erased": not *"the runtime does not
exist"* but *"a program that does not need it does not link or pay for it."*

## 6. Limits of this dashboard (not silent)

- **`.text` size is not cross-program comparable** (different programs do
  different work); only the categorized survivor counts are comparable and
  attributable. Within a controlled ±axis pair the `.text` delta is meaningful.
- **`secure_slot` and `zone_intent` fold** because the fixtures are statically
  dischargeable. They under-report buckets B for the *runtime-authority* case.
  Next fixtures: a secure slot whose token comes from external input; a zone whose
  participant nullity is runtime — to measure the residue that genuinely survives.
- **Physical column is single-backend (C → `-O2`).** A parallel LLVM-backend
  physical pass (post-`.bc`-link `opt`) should be added and checked for parity
  with the C numbers (C == LLVM is the standing contract).
- This is a **seed** dashboard (8 fixtures). The intended end state is a CI gate:
  the per-program contract of §5 enforced as a regression, and the A/B/C counts
  tracked over time so bucket C only ever shrinks.

## 7. Further work (the gauge becomes a gate)

- Promote §5 to an automated check in CI: provable-value-Slot fixtures must hold
  `Axis = 0, Abort = 0`; a regression that introduces a surviving axis call is a
  build failure.
- Add the runtime-authority fixtures (§6) so buckets A/B have non-zero measured
  baselines, not just the dischargeable cases.
- Cross-check against `docs/semantics/proofs/IRMinimality.v`: that proof bounds
  the *codegen IR layering* (HIR→RIR→MIR is minimal, not over-decomposed); it does
  **not** by itself bound machine-code residue. This dashboard is the empirical
  complement — do not cite IRMinimality as evidence of runtime erasure.
