# Evidence-Driven Guard Amortization

Status: `research / measurement-first`. Owner doc for a candidate
Pergyra-leaning optimization family. Nothing here is a current capability claim
until the measurement gates below are green (per `docs/120` anti-hype rule).

Related:

- `docs/118_slot_model_rigor_audit.md` — slot/guard model.
- `docs/14` + `tests/air_erasure` — erasure dashboard (loss = bounded·measured·attributed).
- Memory: `project_slot_safety_consistency` (checks are always-on by design),
  `project_string_alloc_perf_workstream` (the only *known* structural perf gap),
  `project_machine_neutral_falsification` (backend consumes MIR, not AIR).

## 0. The Intuition

Rust/C++ chase **zero-cost**: safety checks compile away entirely (you pay with
borrow-checker friction instead). Pergyra deliberately keeps **always-on**
runtime checks (slot generation/token, lifecycle valid-from guard, capability
gate) — consistency over zero-cost.

So zero-cost is not our game. The positioning this doc explores is the
alternative: **amortized-cost safety**. We do not erase the check; we **pay it
once at the boundary where the evidence is established, and not N times inside a
hot path.**

> Per-access guard × N (in loop)  →  one boundary check  +  backend assumptions
> that let the per-iteration guards fold away.

The evidence we already compute for *safety* (interprocedural `own`/`ref`,
lifecycle state machine, capability mask, zone grants, slot generation) is also
a source of *optimization facts* — facts the backend cannot reconstruct from
lowered IR.

## 1. Why This Could Be a Pergyra Edge (and where it is not)

LLVM already hoists loop-invariant checks **that it can see** (LICM, check
elimination). The edge is **not a new pass**; it is feeding the backend
high-level facts it cannot derive:

| Evidence (already computed for safety) | Optimization fact | Note |
| --- | --- | --- |
| interprocedural `own` (unique) | `noalias`, no-reload | Rust already does `&mut`→noalias; our novelty is breadth, not this |
| lifecycle state machine (valid-from mask) | "state constant in this block" → hoist guard + specialize body | typestate-flavored; Rust dropped typestate |
| capability mask + `zone` grant | coalesce per-effect-site gates to one zone-entry check | zone is both safety *and* optimization boundary |
| slot generation, no release in region | generation check is loop-invariant → hoist | |

The defensible claim is the **synthesis**: explicit boundaries (`zone`/`pin`/
`intent`/`with`) that are simultaneously the safety scope and the optimization
scope, fed by a richer evidence vocabulary than ownership alone, with a
fail-closed residual. Not any single row above.

## 2. The Make-or-Break Constraint (soundness)

**Only *proven* evidence may be lowered to a hard backend assumption**
(`llvm.assume`, `noalias`, range metadata). **Heuristic evidence must stay a
runtime guard** (fail-closed).

Our verifier is incomplete (red-team R3). If the backend *assumes* a fact the
verifier could not actually prove, a wrong fact becomes silent UB/miscompile —
the worst outcome, trading safety for a fake zero-cost. Therefore:

- **provable subset** (interproc-complete `own`/`ref`, runtime-tagged lifecycle
  state, zone-granted capability) → may lower to assume;
- **everything else** → stays the existing per-access guard (already fail-closed).

Facts flow to the backend through **MIR** (the backend consumes MIR, not AIR).
The residual (where invariance can't be proven) must auto-fall-back to the
per-access check — and that fall-back is a required parity test, not an
afterthought.

## 3. Two Experiment Tracks (measurement-first)

Each track is gated on *measuring a real win first*. "No measurable win" is a
valid, publishable result — it tells us the guard wasn't hot.

### Track A — safety-guard hoisting

Hoist a per-access runtime guard out of a hot loop when the evidence proves it
invariant over the loop region.

1. Fixture: hot loop calling a guarded operation N times with no state change.
2. Measure per-iteration guard cost (baseline) vs a hand-hoisted equivalent.
3. If the delta is real: lower "region-invariant guard" to one boundary check +
   `llvm.assume`; C backend emits the equivalent hoist.
4. Parity: a loop that *does* mutate the guarded state must auto-fall-back to the
   per-access guard. Verify C==LLVM both ways.

### Track B — domain-state specialization

Resolve a domain-state comparison (e.g. `Vessel` Empty/Filled, payment
Pending/Authorized) at the boundary and specialize the loop body to the proven
state, removing the per-iteration state dispatch.

1. Fixture: hot loop dispatching on a subject's lifecycle state that is constant
   in the loop.
2. Measure per-iteration dispatch cost (baseline) vs hand-specialized.
3. If real: when the state is proven constant in the region, emit the
   state-specialized body once + a single boundary check.
4. Parity: a loop that transitions state must keep per-iteration dispatch.

## 4. Honest Doubts (must survive measurement)

- **Is the guard actually hot?** The only *known* structural gap today is string
  allocation, not guard overhead. The guard-hoisting win is a hypothesis until a
  workload shows guards in a hot loop with measurable cost.
- **LLVM may already hoist some** after inlining. Only the *measured increment*
  over what LLVM does on its own counts.
- **Cost of the evidence path** (assume machinery, the invariance proof) is not
  zero; the win must exceed it.

## 4a. Measured Results (2026-06-24, first pass)

Faithful microbenchmarks (gcc -O3, replicating the real guard hot path and the
state-dispatch shapes; `.tmp/perf/`). 2e9 iterations each.

**Track A — lifecycle guard hoisting: WIN, real now.**
- per-guard call = **2.66 ns**; on a hot loop with a trivial body the per-access
  guard added **+183%** wall time (guarded 8.22s vs hoisted 2.90s).
- The guard is a non-inlined export + a **linear-scan** side-map lookup, so the
  cost also *grows with the number of tracked subjects* (separate runtime issue).
- Scope reality: the guard is only emitted on **ambiguous** paths (provable
  straight-line is already erased to zero calls, per `tests/air_erasure/.../
  07_lifecycle_linear.pgy`). So the win applies when an ambiguous-state guard
  lands in a hot, transition-free loop. Narrow but real, and the magnitude scales
  with the body/guard ratio (183% is the trivial-body ceiling).
- Soundness: taint already knows whether the loop transitions the state, so
  "no transition in region → guard is loop-invariant → check once" is provable;
  a transitioning loop falls back to per-access. Implementable on existing infra.

**Track B — domain-state specialization: win is conditional, mostly moot today.**
- Invariant branch with an inlinable body: **no win** (−2.2%; predicted/cmov'd,
  both versions vectorize).
- Invariant **indirect call** (vtable-like dispatch): **4.20x** (1.25 ns/iter)
  via devirtualization + inlining once state is proven constant.
- But Pergyra is **static-dispatch only** in beta (`dyn ability` out-of-beta), so
  there is almost nothing to specialize now. Track B is a **post-dynamic-dispatch**
  optimization: revisit when role/effect/`dyn` dispatch lands.

**Verdict:** implement Track A (measured win, existing evidence). Defer Track B
until dynamic dispatch exists; it has no current target.

## 5. Definition of Done (per track)

- A perf fixture + harness showing baseline vs optimized, with the delta and the
  fail-back case both measured.
- Soundness: only proven evidence lowered; heuristic residual stays a guard;
  C==LLVM parity in both the optimized and the fall-back path.
- Honest write-up of the measured number (including "no win" if so).
