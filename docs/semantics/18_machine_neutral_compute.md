# 18. Machine-Neutral Compute Contract

Last updated: 2026-06-22

Status: `long-term-contract`

This document records a design contract that is already shaping the compiler:
Pergyra source semantics must not collapse into a von Neumann CPU model. C and LLVM are the first validation projections, not the language's final execution ontology.

## Core Claim

Pergyra's source-level axes are:

- `intent`: the declared purpose and step graph.
- `effect`: the observable change being requested.
- `authority`: the evidence that a participant may perform that change.
- `coordination`: the dependency and ordering facts for execution.
- `slot` / capability: the resource handle and access boundary.
- `world` / `zone`: state, isolation, and authority boundaries.

These are language facts. They are not CPU instructions, stack frames, C
struct carriers, LLVM calling conventions, or pointer-layout facts.

The lowering rule is:

```text
source concept -> owner fact -> verifier/evidence -> backend projection
```

If the owner fact is missing, the compiler must fail closed. If the evidence is
present, a backend may retain, summarize, erase, or specialize the source-level
axis according to its own execution substrate.

## Current Projection

The current production projections are C and LLVM. They exist to prove the
stable subset is not accidentally defined by one backend's quirks:

```text
same source -> same AIR/MIR/ABI facts -> C output ~= LLVM output
```

This is not the end state. It is the first executable oracle pair. C and LLVM
must never become the source of truth for `intent`, `effect`, `authority`,
`coordination`, `slot`, `world`, or `zone`.

## Future Execution Models

Pergyra should stay expressible on future compute substrates because its core
facts are not CPU-specific:

| Execution model | Pergyra fact that can project to it |
| --- | --- |
| Dataflow architecture | `intent` dependency graph, readiness facts, effect ordering. |
| Actor model | `world` / `zone` boundaries, participant identity, message-like intent dispatch. |
| Graph reduction | pure intent subgraphs, effect-free expressions, proof-gated erasure. |
| Systolic / tensor architecture | bulk slots, deterministic layout facts, data-parallel coordination. |
| Capability machine | slot handles, authority evidence, capability-gated external effects. |
| Reconfigurable computing | static intent/effect graph, layout facts, capability gates as circuit boundaries. |
| Neuromorphic / event-driven systems | event-triggered intent nodes and sparse boundary activation. |

This table is a projection contract, not a support matrix. It says what the
language must preserve so that these backends can be added later without
changing source semantics.

## Non-Negotiable Rules

1. AIR/MIR/ABI facts own execution meaning.
2. C/LLVM ABI details are backend projection facts, not source semantics.
3. A backend must not physicalize `world`, `zone`, `intent`, or `slot` into a
   runtime carrier, padding, barrier, pointer, or check unless an AIR/MIR/ABI
   fact requires it.
4. A backend may erase a source axis only through proof-gated erasure or an
   explicit compression budget.
5. A backend may specialize a source axis only if the specialized form keeps
   the same authority, effect, failure, and observable ordering contract.
6. If a future substrate cannot represent the required facts, it is an
   unsupported projection, not a reason to weaken the language.

## Relation To Existing Work

This is not a new direction. It is the reason the current closure work exists:

- AIR is an intent/effect/authority/coordination evidence layer, not a CPU IR.
- MIR is being closed as the backend semantic source of truth, not as an AST
  convenience cache.
- Abstraction compression says source axes can disappear only when evidence
  proves that runtime structure is unnecessary.
- Proof-carrying IR starts from live AIR/MIR facts rather than backend output.
- Backend parity keeps C and LLVM as validation anchors, not language owners.

The long-term name for this direction is:

```text
machine-neutral fact ownership
```

Pergyra should compile to CPUs today, but it should not make the CPU the shape
of the language.

## Current Reality vs The Contract (2026-06-22 falsification)

The Core Claim above is the target. It is not yet the implemented reality, and
this section records the gap honestly so a future reader does not mistake the
contract for the state.

A falsification experiment funded the strongest-correspondence substrate - the
capability machine (row 5: slot handles, authority evidence, capability-gated
effects) - and tried to project it from **AIR-only facts**. The result:

- AIR is an **intent-topology + proof-obligation + erasure-attribution** IR. It
  owns intent steps (as a *sequence*, not a dependency graph), zone/world
  boundary topology, authority participant names, evidence provenance, A/B/C
  erasure attribution, and the capability-machine projection surface.
- AIR owns the facts a capability-machine projection needs: named `effects`,
  per-operation `effects_by_op[].capability_mask`, slot identity through `slots`
  rows carrying slot/op/routine, and authority contract requirements through
  boundary `required_abilities`.
- Measured on the capability-machine fixtures (`03_secure_slot`,
  `05_zone_intent`, `01_slot_provable_with`, and `cap_random_demo`), these AIR
  facts are present in `--air-json` and validated by AIR graph invariants.

So machine-neutral **fact ownership** has moved from aspirational to partially
owned by AIR for the capability-machine row. It is still not a backend claim:
the intent axis can still carry a CPU-sequential assumption (steps are a
position-ordered array; there is no surface syntax for a partial order between
steps), so the dataflow row (1) remains separate work.

### The inheritance: a falsification gate, not a narrative

`tests/machine_neutral/capability_projection_gate.py` (run via
`make machine-neutral-status`) is the first executable exercise of the Acceptance
Rule below. It consumes only `--air-json` and reports a four-row checklist:

```text
[OK] effect_inventory
[OK] capability_mask
[OK] slot_identity
[OK] authority_contract_binding
```

The gate remains a falsification marker rather than a must-pass CI target until
at least one real machine-neutral projection consumes the facts. It still fails
when run manually: Python runs the structured checker when available, and the
shell fallback checks the load-bearing AIR JSON fields when Python is absent.
Until a backend exists, "AIR owns the capability-machine facts" means the facts
are present, validator-backed, and projectable from `--air-json`; it does not
mean a production capability-machine backend exists.

## Out Of Scope

This document does not claim current support for GPU, TPU, FPGA, BEAM-like
actor runtimes, capability hardware, neuromorphic hardware, or a production
dataflow backend.

Those targets become credible only when a backend consumes the same owner facts
and passes projection-specific golden tests. Until then, they are future
projection targets, not advertised capabilities.

## Acceptance Rule

A new non-CPU or non-von-Neumann backend may be called aligned with Pergyra only
when all of these hold:

- it consumes AIR/MIR/ABI owner facts instead of rereading source/AST;
- it fails closed when authority, effect, coordination, slot, or layout evidence
  is missing;
- it has positive and negative golden tests for retained, summarized, erased,
  and forbidden-to-erase axes;
- it documents which source axes are physicalized and which are erased;
- it proves observable parity against the current C/LLVM oracle where the
  frozen subset overlaps.
