# IR-Layer Architecture: Audit and Minimality

This is the durable record of the architectural question "is the IR decomposition
(HIR/DIR/RIR/MIR/AIR) over-decomposed, or is it minimal?" — the audit that
answers it, the machine-checked minimality proof
([`IRMinimality.v`](IRMinimality.v)), and the resolution of the single open
boundary (the RIR←HIR edge).

The short answer: **the codegen pipeline is minimal (3 IRs), DIR and AIR are
verification IRs off the codegen path, and the one collapsible boundary is tied
to a named design invariant — not incidental complexity.**

Additional answer: **AIR is the minimal verification witness surface for
intent/effect/authority/coordination composition.** HKT/Functor is not the
smaller core for this problem because it describes type-constructor composition,
not authority, effect, boundary, deterministic coordination, or provenance
evidence.

## 1. The question

A latecomer language knows the failure cases of early ones, yet a multi-IR
backend feels heavy next to a flat, AST-centric compiler (D). The fear: is the
layering *overfit to the current situation* — elaborate machinery around a bet
(introducing AIR instead of a Rust-style single MIR / Swift SIL) that the author
can't independently audit, because they work the front end, not the back?

"Overfit" is not unfalsifiable. The test: does each layer carry a fact a neighbor
*structurally cannot*, and is the layer **count** the minimum the dependencies
allow? Those are answerable.

## 2. The real architecture — two tracks

Grounded in `driver_app.c` and a grep of `src/codegen` (which references neither
DIR nor AIR — **zero**):

```
  codegen track  :  AST ──► HIR ──► RIR ──► MIR ──► {C, LLVM}
                              │       ▲        ▲
                              └───────┘        │   (RIR enriched with HIR flow;
                                      └────────┘    MIR = mir_lower(MIRLowerRequest))

  verification   :  AST ──► DIR ──┐
  track (off the                  ├──► AIR  (air_synthesize/air_verify; drift,
  codegen path)    HIR/RIR/MIR ───┘         evidence — NOT consumed by codegen)
```

So the "five layers" are really **three codegen IRs (HIR/RIR/MIR) plus a separate
verification track (DIR/AIR)**. AIR is *not* a codegen IR competing with SIL/MIR
(docs/104: "verification-only synthesis IR, not a codegen IR"); MIR is unchanged
and AIR sits beside codegen. That alone dissolves half the "AIR gamble" framing.

The other half is the HKT/Functor comparison. AIR is also not a substitute for a
Functor hierarchy. HKT/Functor laws describe shape-preserving maps over type
constructors. AIR proves the domain facts Pergyra composition must preserve:
intent order, boundary, authority, effect, coordination, and provenance. Those
are different questions. AIR, not HKT/Functor, is the minimal verification
witness for this language axis.

## 3. Earns-its-place audit (each layer's unique fact)

| Layer | Owns (a neighbor structurally cannot carry it) |
| --- | --- |
| HIR | control-flow graph: basic blocks, dominance, phi-skeleton |
| DIR | domain-ontology graph: intent→step→zone→effect/authority edges |
| RIR | resource state machines: state lattice (OWNED/MOVED/RELEASED/…), per-scope summaries |
| MIR | fusion: HIR-CFG + RIR-ops → SSA, cleanup/rollback blocks, ABI layout |
| AIR | cross-layer verification evidence (off codegen) |

Each is non-redundant. But non-redundancy ≠ minimality — that needs the next step.

## 4. Minimality proof (`IRMinimality.v`, coqc-checked)

Minimality is made precise as: with respect to the real **reads-from** edges, no
*valid* layering (an IR sits in a strictly earlier layer than anything reading
its completed output) uses fewer layers. By Mirsky's theorem the minimum equals
the longest reads-from chain.

The codegen chain is `HIR → RIR → MIR` (RIR reads HIR flow; MIR fuses both):

- `codegen_needs_three`: any valid layering forces `HIR < RIR < MIR` — three
  distinct layers; they cannot be carried in fewer (lower bound).
- `three_layers_suffice` + `dir_colocates_with_hir`: three layers suffice and DIR
  shares HIR's layer (off-path, adds none) — upper bound.
- `codegen_minimum_is_three`: therefore the minimum is **exactly 3**. The codegen
  decomposition is **minimal, not over-decomposed.**
- `two_layers_suffice_when_deferred`: the **only** collapsible boundary is the
  `RIR ← HIR` edge; drop it and 2 layers suffice. Every other boundary is forced.

So "could it be smaller?" has a complete answer: removable layers = **at most
one**, and only at the RIR←HIR edge.

## 5. The pivot, resolved — what the RIR←HIR edge *is*

The edge is not a convenience. `rir_enrich_scope_with_hir_flow` requires
`hir_routine->has_cfg` and runs a real RPO-fixpoint dataflow over the HIR CFG;
`rir_validation.c` then **merges resource states across control flow and detects
conflicts** (`rir_merge_states_for_kind`), and `rir_validate` runs **before**
`mir_lower`. The flow facts are consumed by RIR's own validation *and* by
`mir_cleanup.c`.

Therefore the RIR←HIR edge encodes a **named design invariant**:

> *flow-sensitive resource checking happens at the resource (RIR) layer, before
> fusion — so resource errors are caught and localized there, separate from
> domain (DIR) and execution (MIR) concerns* (docs/36).

This closes the pivot. The 3-vs-2 choice is now an explicit, named trade, not a
haunting ambiguity:

- **3 layers (current):** resource validation is flow-sensitive and lives at RIR;
  errors localized to the resource layer. MIR reuses RIR's flow facts.
- **2 layers (collapse):** RIR becomes AST-structural only and the flow lattice
  moves into MIR (which already has HIR CFG + RIR ops). Achievable, but it
  *relocates* flow-sensitive resource checking into MIR — changing where errors
  are caught and making MIR recompute the lattice. Not a free simplification.

**Verdict:** the codegen decomposition is minimal **up to one named invariant**.
There is no incidental layer; the single removable boundary is the price of
resource-checking-at-the-resource-layer. That is as "proven minimal" as an
architecture gets.

**Decision (2026-06-20, BDFL): keep RIR as a separate layer — 3 codegen IRs.**
Rationale: *extensibility*. The language's growth axis is resource/domain richness
(new primitives, new resource kinds, new ownership/authority/effect semantics). A
separate flow-sensitive RIR means a new resource semantic is added as a new state
lattice *at RIR*, validated there, without touching MIR's fusion. Collapsing to 2
would couple every future resource-model extension to MIR surgery. So keeping RIR
is exactly the design that passes the live overfit test (§7) — new primitives slot
into RIR rather than requiring surgery. The accepted cost is that this third layer
carries the SoT/parity discipline (the source-payload-retirement work); it is paid
knowingly, in exchange for extension without fusion-layer surgery. The pivot is
therefore *settled*, not open.

## 6a. AIR vs HKT/Functor: minimal verifier, not bigger abstraction

The tempting objection is: if intent composition should be mathematically clean,
why not make the minimum surface Functor/HKT and derive composition laws there?

That is the wrong minimum for Pergyra. Functor/HKT can state shape laws over
containers or type constructors. It does not say:

- which authority discharged a step;
- which effect boundary is crossed;
- which zone/world/transfer boundary supplied provenance;
- which coordination path is deterministic;
- which failure or compensation path is preserved.

Those are not library-level map laws. They are compiler-owned evidence axes.
The Coq model now names this as `VerificationRequirement` and proves:

- `air_witness_adequate`: AIR carries every required witness axis.
- `functor_hkt_not_adequate`: a Functor/HKT witness model is not adequate,
  because it lacks authority/effect/boundary/coordination/provenance evidence.
- `air_is_minimal_witness_set`: any adequate surface that is a subset of AIR's
  witness vocabulary is extensionally equal to AIR; removing an AIR witness
  removes a required proof axis.

So the claim is not "AIR is more expressive than HKT." The claim is sharper:
for Pergyra's intent/effect/authority/coordination contract, **AIR is the
minimum verifier surface**, while HKT/Functor is the wrong abstraction class.

## 6. The D comparison

D is flat because D is *conventional* — expression/type/control-flow map onto
AST+LLVM, and D has no domain-ontology layer (no DIR) and no flow-sensitive
resource-state layer (RIR), because D has neither in its surface. Pergyra's extra
layers are the **shadow of its front-end thesis** (domain meaning + resource
semantics). Judging Pergyra by D's flatness compares across problem categories.
The right aesthetic for Pergyra is not parsimony (D's) but *necessity* — and the
proof above shows the codegen layers are necessary up to the one named trade.

## 7. Honest scope and the remaining real risks

- The proof is minimality **w.r.t. the current fact-sets and their real
  dependencies**. It does not quantify over all conceivable IR designs; a
  different fact factoring could in principle have a shorter chain. What it rules
  out: carrying *these* facts in fewer layers, and any *unexplained* boundary.
- The pivot trade is **settled** (§5): keep RIR (3 layers), for extensibility.
  It is no longer an open question — recorded as a decision, not a risk.
- One risk remains, **beyond** "is the architecture right" (which is now
  answered):
  1. **AIR's product value** — AIR is correctly placed (off-codegen verification),
     so its cost buys *verification of domain-meaning preservation*. Whether that
     is worth verifying is a **product** question (does anyone need
     meaning-preserving compilation?), not an architecture one. This is the bet to
     actually watch.
- Live overfit test: when a new backend (WASM/MLIR) or domain primitive is added,
  watch whether it *slots in* (principled) or *needs surgery* (situation-overfit).

## 8. Verify

```sh
coqc docs/semantics/proofs/IRMinimality.v   # exit 0 == all minimality theorems check
make formal-semantics-test-smoke            # runs it in the CI coqc loop
```
