# Minimal Verification Position — UB-Completeness As The Proof Obligation

Status: `beta-proof-obligation`
Mechanized spine: `docs/semantics/proofs/GuardCalculus.v` (coqc-checked; also
synthesis fragment #2 after the zone↔ambient fragment).

## 1. The Claim, Stated Precisely

Pergyra and Rust deliver the same KIND of end guarantee — well-typed programs
do not exhibit undefined behavior — but they discharge it with different proof
obligations and pay different prices:

- **Maximal-static discipline (idealized Rust position)**: prove at compile
  time that no operation can ever be out of its domain, so the runtime needs
  no safety transitions. The obligation is GLOBAL: flow-sensitive lifetime and
  aliasing analysis over the whole program, and a heavyweight metatheory to
  justify it.
- **Minimal-verification discipline (Pergyra position)**: define the runtime
  semantics itself as fail-closed — every operation class whose lowering could
  exhibit UB is either statically `Proven`, dynamically `Guarded` (an
  out-of-domain instance IS a panic transition, by definition), or `Rejected`.
  The obligation is LOCAL: a finite per-operation coverage table plus the
  promises of the (small) `Proven` set. No lifetime analysis, no aliasing
  proof, no global flow reasoning appears in the obligation.

"Minimal" is not marketing; it has formal content. `coverage_is_local` in
GuardCalculus.v proves the entire obligation is decided by a fold over the
finite op universe — obligation size is O(|op classes|), independent of
program size and flow structure.

## 2. The Three Mechanized Theorems

| Theorem (GuardCalculus.v) | What it pins down |
| --- | --- |
| `no_silent_ub` | coverage + kept `Proven` promises ⟹ no execution reaches silent UB; every out-of-domain step is a Panic transition, never corruption. |
| `coverage_is_local` | the whole obligation is a finite per-op table check — the formal content of "minimal". |
| `guarded_more_permissive_at_equal_safety` | a sound static-only discipline must reject every program containing an op that could misbehave on SOME input; the guarded policy accepts and runs them, panicking exactly on bad dynamic instances. Same no-UB guarantee, strictly larger accepted set. |

Plus the umbrella corollary `pergyra_no_silent_ub`: the concrete policy
(divide/index/overflow/secure-token/lifecycle `Guarded`; pure ops and own/ref
release `Proven`) instantiates the theorem for every fail-closed axis at once.
That single-umbrella shape is why this file doubles as synthesis fragment #2:
slot, lifecycle, arithmetic, and token safety are instances of ONE guard
structure, not four unrelated stories.

## 3. The Honest Ledger (what the position costs)

The model is deliberately honest about the price; none of these are hidden:

1. **A broken `Proven` promise is silent UB** — modeled exactly so in `run`
   (Proven × bad ⟹ UB). The static layer's honesty is a real obligation, not
   bookkeeping. Pergyra keeps the `Proven` set small (pure ops, own/ref
   release tracking) precisely to keep this trusted base small.
2. **Guards cost cycles.** Amortization is a measured workstream (guard
   hoisting, docs/136), not an assumption. The position is
   *amortized-cost*, never "zero-cost".
3. **Failure is late.** A guarded program can abort in production where a
   maximal-static program would have been rejected at compile time. The
   residual risk is an abort, never corruption — the right trade for
   traceability-first domains (factory software), and the wrong one for
   abort-is-a-hazard domains (this is stated, not hidden).
4. **Guard implementations must themselves be correct.** They are tiny,
   local, and twin-gated (C and LLVM legs byte-compared); their correctness
   is a separately discharged obligation, not covered by GuardCalculus.v.
5. **Concurrency is out of this fragment's scope.** Data-race freedom rests
   on the channel-only cross-world contract and is a separate, staged
   obligation (see WitnessDataRace.v for the current fragment).

## 4. Fairness Note — Rust As Shipped Is Already Hybrid

The comparison target in theorem 3 is the IDEALIZED static-only discipline.
Rust as shipped already concedes runtime guards for bounds and (in debug)
overflow; its heavyweight static treatment is reserved for aliasing and
lifetimes. So the real disagreement is narrower and more interesting: **which
operation classes deserve the global static treatment?** Rust answers
"aliasing/lifetimes, at annotation cost". Pergyra answers "none of them — keep
statics where they are cheap and local (own/ref), guard the rest, and spend
the saved complexity budget on domain primitives". This document does not
claim Pergyra's answer dominates; it claims the answer is coherent, mechanized,
and honestly priced. Per the marketing-language audit (docs/118 §8), never
phrase this as "Rust-equivalent safety" — the correct phrase is:
**"the same no-UB end guarantee, delivered as a smaller, local proof
obligation plus fail-closed runtime guards; the residual is an abort, never
corruption."**

## 5. Empirical Witnesses (the `coverage` hypothesis, discharged)

The theorem consumes `coverage` as a hypothesis; the repository discharges it
per operation class with always-on gates — this table IS the coverage table of
GuardCalculus.v, made executable:

| OpClass (model) | Real lowering | Panic class | Gate |
| --- | --- | --- | --- |
| OpDiv | `/`, `%`, checked div/mod | divide-by-zero, division overflow | memory_safety_failclosed, CheckedArith.v |
| OpIndex | `arr[i]`, ArraySet | out-of-bounds | memory_safety_failclosed |
| OpAddMul | CheckedAdd/CheckedMul | arithmetic-overflow | checked_arith + mem-safety reject leg |
| OpSecureToken | secure slot read/write | invalid-secure-token | secure_token_reuse_failclosed |
| OpLifecycle | state-gated method | invalid-lifecycle-state | memory_safety_failclosed |
| OpSlotRelease | own/ref release | (statically proven) | semantic suite (interprocedural UAF cases) |

A new UB-capable operation class MUST land with its row in this table — an
unfilled row is exactly the `Unhandled` verdict the theorem forbids.

## 6. Relation To The Rest Of The Proof Corpus

- `docs/semantics/11_arithmetic_ub_model.md` — the 2-layer UB model this
  position generalizes (surface UB vs backend-inherited UB).
- `docs/118_slot_model_rigor_audit.md` §2/§4.4/§7 — slot is not a borrow
  checker; the runtime tier is Vale/Verona-pattern; comparison to Rust over
  time.
- `docs/semantics/19_theoretical_foundations.md` — the per-primitive theory
  correspondences this fragment starts stitching together.
- `docs/semantics/14_air_erasure_measurement.md` — loss is bounded, measured,
  attributed; guards are the C-bucket made principled.
