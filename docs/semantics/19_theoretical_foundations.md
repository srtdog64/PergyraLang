# 19. Theoretical Foundations And Synthesis Boundary

Last updated: 2026-06-22

Status: `theory-lineage; not whole-language proof`

## Purpose

Pergyra should not pretend that a citation proves the language. A citation is a lineage anchor, not an implementation theorem.
This document records which established theories each source-level axis is
borrowing from, and which proof obligations remain before the language can claim
a unified semantic foundation.

The answer to "is this a groundless new DSL?" is therefore precise:

- No: the vocabulary is not arbitrary. The axes map to established theories.
- Also no overclaim: those theories are separate. Pergyra still needs its own
  small abstract machine or core calculus showing that the axes compose.

This file is a bibliography and synthesis boundary. It is not a whole-language
soundness proof.

## Lineage Map

| Pergyra axis | Theory lineage | Use in Pergyra | Boundary / correction |
| --- | --- | --- | --- |
| `world` / `zone` | Ambient calculus, pi-calculus, actor model, authorization contexts | Location, isolation, participant boundary, and controlled crossing. | Ambient and pi-calculus are direct only when mobility or name-passing matters. Actor and authorization-context models are more direct for isolated zones with explicit authority. |
| `channel` | Session types and multiparty session types | Protocol ordering and communication discipline target. | `Channel<T>` alone is not a session type. It becomes one only when duality, protocol state, ordering, and endpoint roles are enforced. |
| `effect` / `capability` | Effect systems, algebraic effects and handlers, object-capability, CHERI-style capability machines | `effect` records what may happen; `capability` records who is allowed to make it happen. | Keep the axes separate. An effect declaration is not a capability, and a runtime capability check is not an effect system. |
| `authority` | Authorization logic, ABLP access-control calculus, dependency/control calculi | Principals, delegation, evidence, and "authorized by" obligations. | Runtime checks alone are not authorization logic. The proof obligation is explicit principal/evidence flow. |
| `slot` / lifecycle | Linear and affine types, typestate, regions, separation logic | Ownership, lifecycle state, release/consume, and resource safety. | This is the strongest current spine. The open work is composition with zone/effect/authority, not the isolated idea of resource state. |
| `intent` | Dataflow, Kahn process networks, Petri/workflow nets, saga compensation | Readiness, ordering, purpose, failure, and compensation over the other axes. | Dataflow alone is too thin. Intent needs coordination plus compensation, linked to effect, authority, zone, and slot facts. |
| declare-not-analyze | Rice's theorem and decidability limits | Lost domain meaning must be declared instead of recovered by whole-program analysis. | This is a limit argument, not a syntax proof. |
| vocabulary | Domain-driven design and game/world modeling | Ubiquitous terms for world, zone, role, party, roster, and intent. | Vocabulary helps humans model systems; semantics still require owner facts and verifier gates. |
| synthesis precedent | Effects as Sessions, propositions-as-sessions, RustBelt | Evidence that separated theories can be connected or formalized after implementation exists. | Precedent is not proof. Pergyra still needs its own core calculus and backend simulation argument. |

Negative sanity anchors:

- Channel<T> alone is not a session type.
- Dataflow alone is too thin for intent.

## Intent Decomposition

`intent` is not a single magic primitive. It is a composition form:

```text
intent = coordination + compensation
         over zone + effect + authority + slot/lifecycle facts
```

The new intent-specific pieces are coordination and compensation. The other
facets should link to their existing owners. This matters because AIR/MIR should
not flatten all six facts into one opaque node. A future backend, proof checker,
or machine-neutral projection must be able to consume each fact from its owner.

## Pergyra Abstract Machine Obligation

To graduate from theory-lineage to language theory, Pergyra needs a small
abstract machine or core calculus with at least these pieces:

```text
State   = Values + Resources + AuthorityEvidence + EffectLog + ZoneGraph
Program = IntentGraph + FunctionBodies
Step    = pure
        | resource operation
        | authority check
        | effect emit
        | boundary transfer
Failure = typed failure + compensation rule
```

The proof obligations are:

- Preservation: a well-typed step either produces a well-typed next state or a
  typed failure.
- Progress/fail-closed: missing owner facts, missing authority evidence, invalid
  lifecycle state, and invalid zone crossing fail before backend execution.
- Effect isolation: effects cannot appear without an owner fact and authority
  evidence path.
- Capability soundness: a backend-visible operation must be reachable only
  through a capability/evidence path accepted by the verifier.
- Coordination determinism: the same input fact graph produces the same ready
  step order, trace shape, and compensation order.
- Backend simulation: C, LLVM, and future targets simulate the same AIR/MIR/ABI
  owner facts. A backend must not re-infer semantic facts from source or AST.

## Reference Anchors

- Ambient calculus: Luca Cardelli and Andrew D. Gordon, "Mobile Ambients".
- Pi-calculus: Robin Milner, Joachim Parrow, and David Walker, "A Calculus of
  Mobile Processes".
- Actor model: Carl Hewitt, Peter Bishop, and Richard Steiger; Gul Agha,
  "Actors: A Model of Concurrent Computation in Distributed Systems".
- Session types: Kohei Honda; Honda, Vasconcelos, and Kubo; Honda, Yoshida, and
  Carbone on multiparty asynchronous session types.
- Effects: Lucassen and Gifford, "Polymorphic Effect Systems"; Plotkin and
  Pretnar, "Handlers of Algebraic Effects"; Leijen's Koka work.
- Capabilities: Dennis and Van Horn, "Programming Semantics for
  Multiprogrammed Computations"; Mark Miller, "Robust Composition"; CHERI.
- Authorization: Abadi, Burrows, Lampson, and Plotkin, "A Calculus for Access
  Control in Distributed Systems"; Abadi et al. on dependency calculi.
- Lifecycle: Girard linear logic; Wadler linear types; Strom and Yemini
  typestate; Tofte and Talpin regions; Reynolds separation logic.
- Coordination and compensation: Dennis dataflow; Kahn process networks; van
  der Aalst workflow nets; Garcia-Molina and Salem sagas.
- Synthesis precedent: Effects as Sessions; Caires and Pfenning
  propositions-as-sessions; RustBelt.

## Current Status

Pergyra has useful formal seeds under `docs/semantics/proofs/`, but those files
model fragments and compiler obligations. They do not prove the whole language.

The next honest closure step is not adding more vocabulary. It is turning the
abstract-machine obligation above into a minimal checked fragment, then binding
that fragment to live AIR/MIR/ABI owner facts with golden tests.

### First fragment landed (2026-06-22)

`docs/semantics/proofs/ZoneCrossingCore.v` is the first piece of the abstract
machine above: the **boundary-transfer step** (zone crossing) with a `ZoneGraph`
and `AuthorityEvidence` state. It is mechanized in Rocq/Coq and CI-checked under
`make formal-semantics-test-smoke` (coqc). It discharges, for this one facet, the
obligations listed above:

- **Capability soundness** — `crossing_capability_sound`: residence in a
  crossed-into zone witnesses the entry capability the `ZoneGraph` requires.
- **Progress / fail-closed** — `fail_closed_crossing`: a crossing whose entry
  capability is not held is not a derivable step (no ambient-authority rule), so
  the machine is stuck and must fail before backend execution.
- **No ambient authority** — `no_ambient_authority` / `reaches_authority_stable`:
  crossing never grants a capability; authority evidence is invariant along a run.

This is the `world`/`zone` corner only (ambient-calculus lineage).

### Second fragment landed (2026-06-22)

`docs/semantics/proofs/EffectAuthorityCore.v` adds the **effect-emit Step form** to
the *same* state (now `held` authority + `here` zone + `elog` effect log) and proves
the composition the synthesis flags as the hard part — two capability disciplines
(movement, effect) on one authority evidence, neither weakening the other. CI-checked
under `make formal-semantics-test-smoke`. Theorems:

- **Effect isolation** — `step_effect_authorized`: an effect cannot enter the log
  without an authority path; every newly emitted effect was authorized.
- **Crossing soundness** (reused) — `crossing_capability_sound`: any zone change
  witnesses the entry capability.
- **Progress / fail-closed** — `fail_closed_emit`: an emission whose capability is
  not held is not a derivable step.
- **No ambient authority** — `step_preserves_authority` /
  `steps_preserves_authority`: authority is invariant under *either* step.

### Third and fourth corners landed (2026-06-22)

`docs/semantics/proofs/SlotLifecycleCore.v` adds the **resource-operation Step
form** (slot lifecycle, affine/typestate lineage): typestate-gated acquire/use/
release with `acquire_requires_capability`, `use_requires_filled`,
`release_requires_filled`, and the affine-safety theorem `no_op_after_release`
(use-after-release and double-release are not derivable). `docs/semantics/proofs/
AuthorityDelegationCore.v` adds the **authority-check Step form** (delegation,
authorization-logic/ocap lineage): `delegation_requires_holding` (you can only
grant what you hold) and `no_privilege_escalation` (delegation creates no new
capability — the transitive no-ambient-authority property for the authority axis).

**Milestone: all four base axes of the abstract machine now have a mechanized
soundness / fail-closed theorem** — zone crossing, effect emit, slot lifecycle,
authority delegation — each sharing the same authority-evidence discipline (every
step leaves authority invariant, so the four disciplines compose). The whole proof
pack is `coqc`-checked under `make formal-semantics-test-smoke` (11/11).

### Unified machine landed (2026-06-22)

`docs/semantics/proofs/UnifiedCore.v` unifies the four corners into ONE abstract
machine: a single `config` (`actor`, `holdings`, `here`, `elog`, `store`) and a
single `step` relation carrying all six Step forms (Cross / Emit / Acquire / Use /
Release / Delegate), each capability- or typestate-gated. It proves the
cross-cutting capstone the corners cannot prove in isolation:

- **`authority_conservation`** — *no Step form anywhere creates a capability*: any
  capability in circulation after any step was already in circulation before it.
  Delegation only redistributes existing authority; the zone/effect/slot steps do
  not touch `holdings`. This is the whole-machine no-ambient-authority theorem.

So the four capability/authority disciplines provably coexist on one state without
interference. The full proof pack is `coqc`-checked under
`make formal-semantics-test-smoke` (12/12).

### Compensation corner landed (2026-06-22)

`docs/semantics/proofs/CompensationCore.v` adds the **compensation / rollback Step
form** — the intent-specific facet docs/19 flags as the hard coupling, since a
rollback is sound only when it names *both* the effect to undo and the typestate
to restore (`comp_target : eff -> slot` is the explicit coupling). Saga lineage.
Theorems: `rollback_requires_log` (fail-closed: cannot undo what was never done),
`rollback_restores` (rolling back effect `e` restores its target slot to `Empty`),
`rollback_pops_log` (removes exactly the compensated effect), and the saga
round-trip `do_then_rollback_restores` (forward-then-compensate is the identity on
the touched slot). This is where the effect facet and the slot/lifecycle facet are
shown to agree — the synthesis point.

### Status of the calculus

The Pergyra core calculus now has **six `coqc`-checked files (13/13 with the
existing proofs)**: four base-axis corners (zone, effect, slot, authority), the
unified machine (`authority_conservation`), and compensation (the effect↔slot
coupling). "Is this a groundless DSL?" is, for these facets, answered by checked
theorems rather than narrative.

### Coordination corner landed (2026-06-22) — intent decomposition complete

`docs/semantics/proofs/CoordinationCore.v` adds the **coordination Step form** —
the step dependency graph (dataflow / Kahn Process Network lineage), replacing the
position-ordered "sequence" view of intent steps (the CPU-sequential assumption
docs/18 flagged) with an explicit readiness/partial-order model. Theorems:
`run_requires_deps` (fail-closed: a step runs only when every dependency is done)
and `reachable_dep_closed` (any reachable schedule is dependency-closed — a
completed step always has all its dependencies completed, the KPN/dataflow core
invariant, regardless of which ready step is picked).

**With this, the full `intent` decomposition is mechanized**: the four base axes it
composes (zone, effect, slot, authority) plus its two own facets (coordination,
compensation), each with a checked soundness/fail-closed theorem, and the unified
machine showing the base axes compose. Seven `coqc`-checked core-calculus files
(14/14 with the existing proofs).

The remaining work is now:
1. **Preservation/progress over a typing judgment** for whole programs (the current
   theorems are per-step fail-closed/soundness lemmas; the full pair is the depth).
2. **Bind** the model's graphs/holdings terms to the live AIR/MIR owner facts —
   the same plumbing that turns the `make machine-neutral-status` gate (docs/18)
   from RED to GREEN. The calculus and the fact-ownership fix are one workstream;
   this is the theory↔implementation bridge. The calculus now names exactly which
   facts AIR must own (a `ZoneGraph`, an `effect_graph`, an `acquire_graph`, a
   `holdings` map, a `deps` graph, and a `comp_target` map).
