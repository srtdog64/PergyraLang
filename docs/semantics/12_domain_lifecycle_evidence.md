# 12. Domain-Lifecycle Evidence (the Rust-outside class)

Status: **PARTIAL IMPLEMENTATION / RUNTIME GUARD SEED.** The parser accepts the
`lifecycle <Subject> { Op: From -> To; }` surface, semantic analysis collects
those declarations into one lifecycle registry, and the static analyzer rejects
known invalid transitions. Ambiguous joins now produce lifecycle guard verdicts
(`LC_GUARD_SET` / `LC_GUARD_CHECK`) instead of a semantic fallback; MIR lowering
copies those verdicts into `MIRInstruction` lifecycle guard facts, and
MIR-active C/LLVM lowering consumes those MIR facts uniformly.

Companion to the slot model ([[project_slot_safety_consistency]],
`08_slot_capability_calculus.md`) and the witness/evidence triad
(`10_ability_witness_evidence.md`). This doc extends the *same* risk-scaled
evidence model from memory safety to **domain lifecycle correctness** - a class
Rust does not natively frame.

## 0. The class

A resource carries a **domain lifecycle**: a set of states with valid
transitions. Examples:

- `Vessel`: `Empty -> Filled -> Sealed` (read only when Filled, seal only once).
- A payment: `Pending -> Authorized -> Captured` (capture only after authorize).
- `QubitSlot`: `Allocated -> Entangled -> Measured` (measure collapses; no re-measure).

**Domain-lifecycle UB** = applying an operation in a state where it is
meaningless: read an `Empty` vessel, capture a `Pending` payment, measure a
collapsed qubit. This is not C-level memory UB - it is *domain* UB. It is exactly
the lost-meaning the research thesis targets: the "until-when" (boundedness) and
"eligibility" (qualification) axes of a resource's relationship, which plain
types erase.

## 1. Why this is the Rust-outside differentiation

Rust **does not frame this class**. To prevent "capture before authorize" in
Rust you hand-roll *typestate*: phantom type parameters, `self`-consuming
transition methods, and a rebuilt value at every step
(`Payment<Pending> -> Payment<Authorized>`). The cost is the same debt that makes
Rust heavy in the large: the type puzzle pushes people toward `.clone()`,
`Rc`/`Arc`, and builder rebuilds *to satisfy the checker*, not the domain. The
lifecycle becomes a type-system riddle instead of a stated fact.

Pergyra already has the missing primitives as **first-class domain vocabulary** -
`subject`/`vessel` (a stateful entity) and `intent`/`contract` (a declared,
checked operation precondition). So the lifecycle can be **declared once as
domain data** and checked, rather than re-encoded in phantom types. No
borrow-checker cognitive load; the annotation is meaningful domain information
("this op requires state Filled"), not a lifetime proof.

## 2. The evidence model (risk-scaled, mirrors slots)

The slot model is the template: **static where the analysis can prove it, a
runtime evidence tag (fail-closed) where it cannot, and the cost paid only where
a violation would be UB or silent meaning-loss.** Applied to lifecycle:

1. **Contract = the rule, stated once.** A subject/vessel declares its states and
   the precondition of each operation:
   `intent Capture on Payment requires state Authorized` (surface sketch). The
   contract *is* the evidence specification - "intent checked by contract" made
   concrete.

2. **Static (zero cost) where the state is provable.** When control flow makes
   the state determinable (linear `Authorize(); Capture();`), the checker tracks
   the subject's state like it tracks a slot's claimed/released state and rejects
   an invalid op at compile time. This is the always-on, no-cost layer.

3. **Runtime evidence (fail-closed) where it is not.** When the state depends on
   a runtime condition (authorized only if an external call succeeded), the
   subject carries a **state tag** and the operation checks it, panicking
   `class=invalid-lifecycle-state reason="Capture on Payment in state Pending;
   requires Authorized"`. The failure carries the *domain* meaning, not a generic
   "invalid state" - lost-meaning recovery at the failure site.

4. **Consistency, not build-mode.** Like slot checks
   ([[project_slot_safety_consistency]]), the runtime tag is on by default in
   every build; perf is an explicit, local opt-out, never a hidden global
   default. A divergent (debug-only) lifecycle guard would repeat the anti-pattern
   we already rejected.

The state tag is the lifecycle analogue of the slot generation counter: cheap,
domain-meaningful evidence, checked fail-closed.

## 3. What it is NOT

- Not a full session-type / behavioural-type system - those carry the same
  cognitive load we are avoiding. The contract states *per-operation
  preconditions*, not whole-protocol types.
- Not novel wholesale - typestate (Plaid), session types, and the Vale/Pony
  lineage all touch this. The contribution is the **synthesis**: domain-stated
  lifecycle + risk-scaled static/runtime evidence + fail-closed observability,
  fitting Pergyra's identity.

## 4. Implemented seeds vs proposed

**Already present** (the model generalizes these, it does not start from zero):

- Slot state transitions (claimed/released) - `slot_analyzer.{h,c}`, the static
  lifecycle tracker that this doc's section 2.2 generalizes.
- Zone state requirements - `type_checker_builtins_query.c` ("requires a valid
  zone state", "contract originates from state").
- Intent step inherited requirements - `type_checker_intent_action_contract.c`
  (`intent_step_inherited_requires`).
- Lifecycle / state / vessel diagnostic codes - `diag_codes.h`.

**Implemented seed:**

- Surface declaration: `lifecycle <Subject> { <Op>: <FromState> -> <ToState>; }`
  is parsed as `AST_LIFECYCLE_DECL`.
- Semantic owner: `lifecycle_analyze.c` is the single consumer that lowers those
  declarations into `lifecycle_state.c` registry entries. The parser does not
  populate the registry.
- Static tracker: function bodies track governed locals through straight-line
  code and merge branch exits through `lc_merge`. Known invalid transitions are
  semantic errors.
- Runtime-evidence verdict: `lc_apply_op` may return `LC_NEEDS_RUNTIME_CHECK`
  for ambiguous states. The analyzer records guard annotations keyed by the
  construction/call AST nodes; fully static paths remain zero-cost, while
  ambiguous variables get `LC_GUARD_SET` construction/proven-transition facts
  and `LC_GUARD_CHECK` fail-closed check facts.
- Type owner: governed local discovery consumes semantic type-resolution facts
  before source spelling, so namespace/alias resolution stays behind the type
  metadata owner instead of being reimplemented in lifecycle analysis.
- Runtime state tag (layer 3, **implemented, C == LLVM**): the MIR guard facts
  are lowered by both backends to runtime calls -
  `pgy_runtime_lifecycle_set_export` (record a proven transition) and
  `pgy_runtime_lifecycle_guard_export` (fail-closed check). State lives in a
  process-wide side-map keyed by the governed local's storage address - keeping
  the subject struct ABI untouched. The semantic side-table is an input to MIR
  lowering only; MIR-active C and LLVM emission consume
  `mir_instruction_has_lifecycle_guard(...)` and the associated guard fields,
  so the two lowerings are uniform by construction and do not rediscover the
  lifecycle verdict from source AST nodes. A violated precondition panics with class
  `invalid-lifecycle-state` and a traceable `op=/subject=/state=/permitted_mask=`
  record. The two runtime helpers are stripped from the inlined runtime bitcode
  (kept external) so both calls resolve to the one runtime object that owns the
  single side-map and the correctly-lowered abort - the same bitcode-exclusion
  policy the checked-arithmetic and panic families use.
- Taint scope: only variables that actually reach an ambiguous op are
  instrumented; fully statically-proven variables stay zero-cost.

**v1 limits (documented, not silent):** the side-map is fixed-capacity, single
-threaded, and keyed by local storage address (no cross-alias / cross-frame
identity). These bound only the runtime backstop for the ambiguous minority, not
the always-on static layer. Taint-based elision and aliasing identity are the
next refinements (see section 5).

## 5. Open questions

- Aliasing: a subject's state under shared/aliased access reuses the slot
  exclusivity story (pin/view, aliasing-xor-mutability) - the state read must see
  a consistent value. Defer to the witness model.
- Cost: an N-state tag is one small field + one compare per gated op - the same
  scale as the slot occupied flag, accepted as the fail-closed default.
- Surface ergonomics: keep the contract a *stated fact* on the subject, not a
  per-call-site annotation, to preserve the "no cognitive load" property.
