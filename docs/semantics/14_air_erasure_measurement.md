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

## 0a. Decision Point

The semantic erasure decision is made once: AIR classifies each intent and
boundary with `compression_budget`, `compression_reason`, and `retain_cause`
after MIR/RIR/DAG evidence exists and before C/LLVM emission. Backends consume
that classification; they do not choose whether a source-level axis is erased,
summarized, retained, or forbidden.

The measurement harness is deliberately separate. `tests/air_erasure/measure.ps1`
observes physical residue after C emission and optimization, and
`tests/air_erasure/gate.ps1` compares that residue against AIR's declared A/B/C
facts. The harness is an oracle for whether the AIR decision matched reality,
not a second owner of the decision.

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
| `04_channel_parallel` | 8 | **0** | 13 | 2 | 1 | 1 |
| `05_zone_intent` | 6 | **0** | 2 | 1 | 1 | 2 |
| `06_lifecycle_branch` | 2 | **0** | 0 | 0 | 1 | 0 |
| `07_lifecycle_linear` | 0 | **0** | 0 | 0 | 0 | 0 |

## 2a. 2026-07-04 refresh — substrate floor + CI promotion (WO-A2)

The table above is the 2026-06 snapshot. The 2026-07-04 re-measurement found
every fixture (including `00_pure_value`) gained `Sync +2, Abort +1`:
the **R6 wall-time watchdog** (task #41, `pgy_budget_wall_watchdog` in
`pgy_runtime_budget.h`) now inlines `pthread_create`/`pthread_detach` plus a
fail-close `abort` into every emitted program. That is designed bucket-B
substrate — the quantitative sandbox axis every program carries — not an
erasure regression: **`Axis (phys)` stayed 0 in every fixture.**

Consequences, encoded in the gate (`tests/air_erasure/gate.ps1`):

- **Substrate-floor pin (contract 0, hard):** `baseline.json` names the floor
  symbols (`abort`, `pthread_create`, `pthread_detach`) and the control fixture
  must match them exactly. Floor growth is RED until a human commits a new
  attributed baseline — the floor itself is now bounded·measured·attributed.
- **Floor-adjusted erasure contract:** provable fixtures must carry *nothing
  beyond the floor*. The §3 claim "the always-on slot check is DCE'd" survives
  in floor-adjusted form: the provable slot fixtures measure **identical** to
  the no-slot control, so slot machinery contributed zero physical residue.
- **CI promotion:** `make air-erasure-gate` re-measures and gates on every run,
  wired into ci-windows' runnable block (instrumentation = powershell + mingw
  `nm`, so Windows CI is its home). RED verified in both directions (injected
  axis symbol / injected floor symbol). Baseline updates are explicit commits.

Declared-side deltas in the same window: `01_slot_provable_with` now pins one
semantic execution-boundary retain with no physical excess, and
`04_channel_parallel` pins both boundary retain causes and the program-wide
concurrency summary (`A_inh=6`). The hard C-monotone rule was never violated
(`C_unprov` total stable at 1).

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
   abstraction leaking.** `04_channel_parallel` keeps **13 `Sync` + 2 `Heap`**:
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
| **B — by-design fail-closed evidence** | always-on safety tag kept for consistency/traceability | `05` Sync=2/Heap=1 = intent-trace machinery; slot gen-counter check (here DCE'd) | Only when statically dischargeable or moved behind a distinct explicit ABI owner |
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

## 5a. Evidence-Amortized Hot Path

The performance target is not "zero cost everywhere." Pergyra's target is
evidence cost that is visible, attributed, and amortized away from repeated hot
path operations when MIR/AIR facts prove the region.

The canonical shape is Slot Pin/Lease:

```text
preflight owner/generation/capability/layout evidence once
-> materialize a typed ReadView<T> or WriteView<T>
-> run the hot loop over that view
-> invalidate at the MIR cleanup edge
```

This optimization path is cacheable, but only under the same source-of-truth
rule as every other AIR decision. A cached evidence view is an acceleration over
MIR/AIR/ABI facts; it is not a second proof. Its cache key must carry the facts
that make it valid: slot identity, generation or epoch, access mode, payload
layout, authority/capability token for secure slots, and the MIR pin-region /
cleanup-edge owner. A missing or mismatched fact fails closed or routes through a
declared retain path; it must not use a stale cached pointer.

Allowed cache scopes:

- local typed view inside a lexical pin block;
- loop/region-local preflight reused across repeated reads/writes when
  `mir_block_has_pin_guard_amortization_region(...)` is present;
- compile-time proof cache of AIR/MIR classifications keyed by input/fact graph
  hashes, as long as the cache is invalidated by any owner-fact change.

Disallowed in beta:

- cross-call or cross-intent runtime view caches;
- async/parallel view caches;
- persistent slot pointer caches;
- cache hits that bypass authority/capability evidence;
- backend-local caches that rediscover source facts instead of consuming MIR/AIR
  facts.

`benchmarks/perf_guard_amortization.c` is the seed Track-A fixture for this
claim: it compares per-access guard checks with a one-time preflight evidence
view over the same data, plus a repeated-preflight no-cache path so the cache
effect is measured directly. `make evidence-guard-amortization-test-smoke` keeps
the source/codegen shape gated and reports internal benchmark-process timings so
shell launch and scheduling noise do not dominate the signal. The pass/fail
threshold is applied to the best paired guard ratio and cache-effect ratio on
supported toolchains. Treat its numbers as a hot-path microbenchmark, not as a
whole-language performance claim.

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
- **Host-toolchain residue must be normalized.** Windows/MinGW object files may
  expose CRT/thread/abort imports that are not Pergyra axis residue. The harness
  must classify or subtract those host baseline symbols before treating them as
  compression drift.
- This is a **seed** dashboard (8 fixtures). The intended end state is a CI gate:
  the per-program contract of §5 enforced as a regression, and the A/B/C counts
  tracked over time so bucket C only ever shrinks.

## 7. AIR-native integration (implemented)

The A/B/C decomposition is **not a new vocabulary** — it is the `reason` taxonomy
on AIR's existing `retain`/`summarize`/`erase`/`forbid` disposition, now made
machine-readable. AIR already distinguished "runtime-visible coordination"
(inherent) from "semantic provenance only" (erased); the addition names the
cause and surfaces the one bucket AIR previously could not express — *unproven*.

**Declared side (in AIR, `--air-json`):**

- `AIRRetainCause { none, inherent, policy, unproven }` (`air.h`). Each boundary
  emits `retain_cause`: a `retain` budget → `inherent` (A), a `summarize` budget
  → `policy` (B). Derived from the budget, no new decision logic.
- `unproven_retain_count` (program-level): the count of lifecycle CHECK guards —
  retains the static analyzer *could not erase* (bucket C). Collected in
  `air_collect_mir_evidence` from the MIR lifecycle guard facts
  (`mir_instruction_lifecycle_guard_kind == CHECK`), so the "I could not prove
  it" verdict now reaches AIR instead of being invisible to it.
- `slot_capability_retain_count` (program-level): the count of
  SecureSlot/DeviceSlot resource operations declared from MIR type-layout facts
  (bucket B). These checks may survive across method or ABI boundaries even
  when there is no intent-step boundary node.

**Measured side (out-of-band, `tests/air_erasure`):** the harness holds the
physical `nm` facts AIR cannot see at compile time and **joins** them to the
declared A/B/C per fixture (`measure.ps1` → `results.csv`).

**The seam (`AIR_DRIFT_COMPRESSION_RESIDUE_MISMATCH`):** when AIR declares a
program fully compressed (A+B+C = 0) yet physical residue survives, that is a
drift in AIR's existing drift vocabulary — populated by the harness, because the
ground truth is post-codegen. The independence is deliberate: a compiler grading
its own erasure is not evidence.

**Measured join (the proof it works):**

| Fixture | AIR: A_inh | AIR: B_pol | AIR: C_unprov | phys Sync | phys Abort | reading |
|---|---:|---:|---:|---:|---:|---|
| `slot_provable_with` | **1** | 0 | 0 | 2 | 1 | straight-line with-slot retains a semantic execution boundary but has no physical residue beyond floor |
| `lifecycle_branch` | 0 | 0 | **1** | 2 | **1** | declared C=1; current symbol oracle has no abort excess beyond floor |
| `zone_intent` | **2** | 0 | 0 | 4 | 1 | inherent boundaries declare runtime sync residue; floor-excess Sync=2 |
| `channel_parallel` | **6** | 0 | 0 | **15** | 1 | boundary retains plus program-wide concurrency summary; floor-excess Sync=13 |
| `secure_slot_method` | 0 | **2** | 0 | 2 | 1 | secure/device capability residue is declared as policy |

The `lifecycle_branch` row is the load-bearing bucket-C result: AIR *declares*
exactly one unproven retain. The current `nm -u` oracle is symbol-level, so the
abort symbol is pinned by the substrate floor rather than counted per callsite.
The `channel_parallel` and `secure_slot_method` rows are the bucket-A/B closure
points: residue can remain physical, but it is now declared by AIR instead of
appearing as an undeclared compression mismatch.

**The gate (`tests/air_erasure/gate.ps1` + `baseline.json`):**

1. **Erasure contract (hard):** every provable fixture must hold
   `phys_Axis = 0, phys_Abort = 0`. A regression is a build failure.
2. **Bucket-C monotonicity (hard):** total `unproven_retain` must not exceed the
   committed baseline. C may only shrink — growth means the analysis weakened.
3. **Declared-vs-measured drift (hard unless expected):** a new
   `compression_residue_mismatch` fails the gate; only rows listed in
   `expected_drifts` are reported as documented modeling gaps.
4. **Retained-runtime attribution (hard):** every fixture with a nonzero A/B/C
   retain declaration or physical residue beyond the substrate floor must match
   `baseline.json` `retained_runtime_attribution` exactly, including the human
   reason.

## 8. Gaps closed

The three gaps §6/§7 recorded are now filled:

- **Concurrency declared (bucket A).** `air_collect_mir_evidence` counts
  parallel/async/spawn blocks and channel send/recv as inherent concurrency
  retains (`inherent_concurrency_count`, `air_mir_routine_inherent_concurrency_fact_count`),
  so a bare `parallel{}`/`channel` — not an intent-step boundary — still declares
  its irreducible residue. `04_channel_parallel` now reports `A_inh = 6` because
  the gate joins boundary retain causes with the program-wide concurrency
  summary; the previous `compression_residue_mismatch` drift is gone.
- **Runtime-authority fixture (bucket B is declared).**
  `08_secure_slot_method` reads a secure-slot token across a method boundary.
  AIR declares the policy retain via `slot_capability_retain_count`, computed
  from MIR resource-op type-layout facts. The current physical oracle reports no
  floor-excess abort for this row, so the retained semantic fact is pinned in
  `retained_runtime_attribution` rather than inferred from raw symbol count.
  Findings: a slot whose release is behind a runtime branch is **statically
  rejected** (fail-closed at compile time), not deferred to runtime — so the
  always-on slot check rarely survives `-O2` in compilable code.
- **C == LLVM behavioral parity (`parity.ps1`).** Faithful LLVM *physical*
  residue (runtime-linked, `opt -O2`, `llc`) is **tooling-gated**: this LLVM
  install ships only libLLVM + `clang` (no `opt`/`llc`/`llvm-link`). Since both
  backends share an ABI-identical runtime (`pgy_runtime_lib.bc` is built from the
  same source as the C static-inline runtime, `scripts/build_runtime_bc.sh`),
  residue parity follows from behavioral parity, which `parity.ps1` checks: every
  fixture must yield the same outcome on both backends. 9/9 match, including
  `03_secure_slot` tuple-destructure `ClaimSecureSlot` and
  `06_lifecycle_branch` (both `panic:invalid-lifecycle-state`). New LLVM
  unsupported fixture entries must be explicit regressions, not hidden defaults.

## 9. Remaining work

- When `opt`/`llc` are available, add the faithful LLVM *physical* `nm` pass and
  check residue-count parity, not just behavioral parity.
- Add a host-baseline normalization pass for Windows/MinGW physical residue so
  compiler-axis residue is separated from CRT/toolchain imports.
- Extend the evidence-amortization cache fixtures from Slot read to write,
  SecureSlot local-token, rejected stale-generation, and rejected async/parallel
  escape cases.
- Cross-check against `docs/semantics/proofs/IRMinimality.v`: that proof bounds
  the *codegen IR layering* (HIR→RIR→MIR is minimal, not over-decomposed); it does
  **not** by itself bound machine-code residue. This dashboard is the empirical
  complement — do not cite IRMinimality as evidence of runtime erasure.
