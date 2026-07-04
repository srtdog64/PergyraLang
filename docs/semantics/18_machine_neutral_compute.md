# 18. Machine-Neutral Compute Contract

Last updated: 2026-06-27

Status: `long-term-contract`

This document records a design contract that is already shaping the compiler:
Pergyra source semantics must not collapse into a von Neumann CPU model. C and
LLVM are the first validation projections, not the language's final execution
ontology.

The sharp claim is not "Pergyra has many backends" in the ordinary compiler
sense. The claim is that the language keeps a machine-neutral fact layer above
all execution substrates. A CPU, C, LLVM, a future tensor/NPU backend, a future
dataflow backend, and a future capability-machine backend are all projections
from the same owner facts. They are not allowed to become the language's source
of truth.

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

## Projection Fact Envelope

Every backend projection consumes an explicit fact envelope. The envelope is
target-independent; the backend-specific lowering is not.

| Fact family | Why a projection needs it |
| --- | --- |
| `intent_graph` | Work units, dependencies, and coordination order. |
| `effect_set` | Which operations are pure, observable, external, or retained. |
| `authority_evidence` | Which participant/capability proves the operation is allowed. |
| `slot_ownership` | Buffer/handle identity, access mode, generation, and transfer boundary. |
| `layout_shape` | Type layout, field order, element shape, ABI policy, and device/host address-space facts. |
| `loss_budget` | Whether approximation, quantization, compression, or lossy projection is allowed. |
| `materialization_reason` | Why a source axis remains runtime-visible instead of being erased. |
| `fallback_reason` | Why a target cannot accept the projection, and which lower target must own the fallback. |

For CPU projections, the envelope usually lowers to stack/heap objects, calls,
branches, and ABI rows. For a future tensor/NPU projection, the same envelope
would lower to graph nodes, tensor shapes, buffer transfers, quantization/loss
budgets, and explicit host fallback reasons. This is the intended Pergyra
advantage: the abstraction layer is heavier than a CPU-first language, but it
keeps the replacement boundary above the backend.

## IR Layering Rule

`IR` is not a promise that every target must look like a CPU. In Pergyra,
`IR` means a family of owned representations, with different levels of
machine commitment:

| Layer | Status | Target dependence |
| --- | --- | --- |
| AIR / evidence graph | Semantic fact IR for `intent`, `effect`, `authority`, `coordination`, and erasure obligations. | Target-neutral. |
| DIR/RIR-style resource facts | Resource, ownership, boundary, capability, and handoff facts. | Mostly target-neutral; may carry target capability requirements. |
| MIR | Backend semantic source of truth for CPU-family projections. CFG, SSA, cleanup, and ABI facts are allowed here. | CPU/C/LLVM projection-biased, not language ontology. |
| Projection IR | Target-specific lowering for tensor/NPU/dataflow/GPU/distributed backends. | Target-specific and may be graph/schedule/placement shaped instead of CFG-shaped. |

The forbidden shape is:

```text
source -> CPU-shaped MIR -> every target
```

That would make non-CPU substrates inherit branch/block/load/store assumptions
even when the source facts are naturally dataflow, tensor, actor, or capability
facts. The intended shape is:

```text
source -> owner facts/evidence -> target-specific projection IR
```

For C and LLVM, the target-specific projection IR may be MIR/ABI/CFG-shaped.
For a future NPU/tensor backend, it should instead consume the same fact
envelope and lower to graph nodes, tensor shapes, buffer lifetimes, placement,
loss/quantization budgets, and explicit host fallback reasons. Such a backend
must not recover semantics by rereading source or by forcing every program
through CPU-shaped MIR first.

## Current Projection

The current production projections are C and LLVM. C and LLVM are the first validation projections. They are CPU-family validation projections. They exist to prove the stable subset is not accidentally defined by one backend's quirks:

```text
same source -> same AIR/MIR/ABI facts -> C output ~= LLVM output
```

This is not the end state. It is the first executable oracle pair. C and LLVM
must never become the source of truth for `intent`, `effect`, `authority`,
`coordination`, `slot`, `world`, or `zone`.

Self-hosted compiler code must obey the same rule. A self-hosted C emitter is a
projection consumer, not a second semantic oracle. A future NPU emitter would be
another projection consumer, not a new language layer.

## Future Execution Models

Pergyra should stay expressible on future compute substrates because its core
facts are not CPU-specific:

| Execution model | Pergyra fact that can project to it |
| --- | --- |
| Dataflow architecture | `intent` dependency graph, readiness facts, effect ordering. |
| Actor model | `world` / `zone` boundaries, participant identity, message-like intent dispatch. |
| Graph reduction | pure intent subgraphs, effect-free expressions, proof-gated erasure. |
| Systolic / tensor / NPU architecture | bulk slots, deterministic layout and shape facts, data-parallel coordination, loss/quantization budgets, host/device transfer ownership. |
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
7. A CPU fallback is not an implicit escape hatch. If a tensor/NPU/dataflow
   projection falls back to CPU, the fallback must be represented by an owner
   fact with a reason such as unsupported shape, forbidden loss budget, retained
   effect, missing authority evidence, or host-only slot boundary.

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

The shorter positioning sentence is:

```text
Pergyra is an intent/evidence language whose backends are projection consumers.
```

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

Promoted to a must-pass member of `test-all` on 2026-07-04 (WO-A1), after a
5-run byte-identical determinism check. Python runs the structured checker when
available, and the shell fallback checks the load-bearing AIR JSON fields when
Python is absent; both paths are fail-closed — a compiler that cannot launch or
emit AIR JSON turns the checklist RED rather than skipping. Until a backend
exists, "AIR owns the capability-machine facts" means the facts are present,
validator-backed, and projectable from `--air-json`; it does not mean a
production capability-machine backend exists.

## Out Of Scope

This document does not claim current support for GPU, TPU, FPGA, BEAM-like
actor runtimes, capability hardware, neuromorphic hardware, or a production
dataflow backend.

Those targets become credible only when a backend consumes the same owner facts
and passes projection-specific golden tests. Until then, they are future
projection targets, not advertised capabilities.

The same applies to NPU/tensor targets. Pergyra's current contract reserves the
facts such a backend would need; it does not claim an implemented accelerator
backend.

## Acceptance Rule

A new non-CPU or non-von-Neumann backend may be called aligned with Pergyra only
when all of these hold:

- it consumes AIR/MIR/ABI owner facts instead of rereading source/AST;
- it fails closed when authority, effect, coordination, slot, or layout evidence
  is missing;
- it has positive and negative golden tests for retained, summarized, erased,
  and forbidden-to-erase axes;
- it has positive and negative golden tests for target capability acceptance,
  loss/quantization acceptance, host/device slot transfer, and explicit fallback
  reasons when the target cannot consume a fact;
- it documents which source axes are physicalized and which are erased;
- it proves observable parity against the current C/LLVM oracle where the
  frozen subset overlaps.
