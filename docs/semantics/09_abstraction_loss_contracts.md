# Abstraction Loss Contracts

Last updated: 2026-09-03

Status: `beta-proof-obligation`

Stable surface: compiler and tooling abstraction boundaries. This document does
not add accepted beta syntax. It defines the contract shape that a pass,
lowering layer, or self-hosted tool must satisfy before it may say that an
abstraction preserves the meaning it claims to preserve.

## Purpose

Turing-complete control flow proves that a program can compute. It does not
prove that an abstraction faithfully represents the real fact it replaced.
Every compiler or tooling boundary loses information:

```text
source bytes -> tokens -> AST -> HIR/DIR/RIR -> MIR -> AIR -> C/LLVM -> binary
```

Loss is not automatically a bug. Hidden loss is the bug. A Pergyra abstraction
boundary must state:

- what is allowed to be lost;
- what must be preserved;
- which owner keeps the original truth;
- which later consumers are forbidden from reinterpreting the lost fact;
- which evidence, diagnostic, runtime check, or rejection proves the loss is
  bounded.

This turns abstraction from "a nicer shape" into a source-of-truth contract.

## Contract Shape

An abstraction loss contract has seven fields:

| Field | Meaning |
|---|---|
| `source` | Input artifact or semantic layer being abstracted. |
| `target` | Output artifact or semantic layer that later consumers read. |
| `owner` | The only layer allowed to decide the original fact. |
| `loses` | Facts intentionally discarded by the boundary. |
| `preserves` | Facts that must remain available in the target or evidence. |
| `forbids` | Downstream reads or fallbacks that would depend on lost facts. |
| `last consumer` | The final stage that still reads the preserved fact (backend, runtime guard, sandbox manifest, LSP/diagnostic, or compatibility corpus). Erasure before this stage is a contract violation; retention past it is hoarding. |
| `evidence` | Named evidence strength plus the artifact that supports the budget. |

Loss budget classes:

| Budget | Meaning |
|---|---|
| `zero` | No loss is accepted for this fact. The target must carry it exactly. |
| `bounded` | A representation choice is allowed, but it has a named bound. |
| `runtime-checked` | Static abstraction loses dynamic existence; runtime validates it. |
| `diagnostic-only` | The fact may be kept only for source spans, messages, or traces. |
| `forbidden` | The boundary may not lose this fact for the stable surface. |

Evidence strength is not interchangeable:

| Strength | What may be claimed |
|---|---|
| `mechanized theorem` | The stated property holds in the named formal model, under that model's assumptions. |
| `executable invariant` | A live implementation gate rejects the named violating artifact or path. |
| `property/differential evidence` | Generated or paired executions agree over the tested domain. |
| `regression witness` | A previously observed failure is fixed for the frozen fixture. |
| `textual ratchet` | Required/forbidden terms and declared ownership remain documented. |
| `documented obligation` | The requirement is named but not yet executable evidence. |

A smoke test is not a theorem, and a mechanized model is not implementation
adequacy. A boundary claim must use the weakest honest label supported by its
current artifact.

Compression budget classes:

| Budget | Meaning |
|---|---|
| `retain` | The abstraction must remain runtime-visible because execution, authority, coordination, failure, or observability needs it. |
| `summarize` | The abstraction may disappear as a runtime carrier, but its evidence/provenance fact remains in AIR/MIR tooling output. |
| `erase` | The abstraction is fully discharged by static evidence and may lower to the ordinary call/value sequence. |
| `forbid` | The abstraction may not be erased or summarized; trying to do so would change meaning. |

Compression is not an optimizer guess. It is a proof-gated erasure contract:
World, Zone, Intent, Slot, Role, Roster, and related domain axes are source-level
semantic axes, not mandatory backend-level physical artifacts. A later backend
may erase or compress one only when the owning AIR/MIR fact says the proof
budget is `summarize` or `erase`.

Canonical rule:

```text
World/Zone/Intent/Slot are source-level semantic axes, not backend-level
physical artifacts.

Missing evidence never becomes guessed static success. The owner must reject,
retain an explicit checked runtime path, or cross a declared external authority
boundary. If the evidence is sufficient, the axis may be erased or compressed
by the verified projection plan.

The chain World -> Zone -> Roster -> Role -> Intent -> Slot is therefore not a
mandatory runtime object graph. It is a verification spine. C and LLVM may
materialize only the parts whose AIR/MIR/ABI facts prove runtime necessity.
```

## Evidence Lifecycle Aesthetic

The major aesthetic is:

> Carry the authority established by evidence to its last legitimate consumer;
> do not carry the entire construction history merely because it once proved
> that authority.

In shorter form: **prove richly, carry minimally**. Pergyra is explicit about
meaning, not committed to preserving every intermediate history. Once an owner
has admitted a semantic decision, a later consumer receives the smallest carrier
that prevents it from deciding that meaning again. Rich provenance may remain in
a diagnostic sidecar, but it does not stay in the main IR as peer authority.

Evidence falls into four lifetime classes:

| Evidence class | Examples | Default lifetime |
|---|---|---|
| Identity | `TypeId`, `BindingId`, `IntentId`, capability/authority identity | `Reference` until its last legitimate consumer; `Materialize` only when runtime meaning requires it. |
| Validity | ownership admission, call/type/layout validity | `Summarize` to a bounded fact or receipt after discharge. |
| Diagnostic | source span, original spelling, failure provenance | `Reference` through a sidecar while diagnostics/debugging need it. |
| Construction | candidate lists, inference scratch graphs, AST traversal history, temporary proof search | `Retain` only through the last semantic consumer, then `Erase`. |

The complete lifecycle vocabulary is:

| Operation | Meaning |
|---|---|
| `Retain` | Keep the rich evidence because an admitted semantic consumer still needs it, or because discharge has not happened. |
| `Reference` | Carry stable identity/provenance without copying the construction payload. |
| `Summarize` | Seal validity into the minimum sufficient fact or receipt. |
| `Materialize` | Produce a machine/runtime representation only at a verified physical boundary. |
| `Erase` | Delete proof-construction data after discharge and its last legitimate consumer. |

These five operations refine, rather than replace, the AIR compression budgets.
`Reference` names identity carriage and `Materialize` names the verified physical
projection. `forbid` remains the budget verdict when erasure or summarization
would change meaning or when required evidence is absent.

A receipt earns its place exactly when removing it would force a downstream
consumer to make the semantic decision again. Otherwise it is ceremony, not a
new layer. `Owner -> Evidence -> Admission -> Receipt -> Plan -> Projection` is
therefore not a mandatory pipeline template: each boundary must name the actual
decision it seals and the last consumer that would otherwise reopen it.

The desired progression is:

```text
many possible interpretations -> one admitted fact -> stable identity/receipt
rich construction evidence     -> bounded summary   -> erased proof-only data
```

Semantic uncertainty and representation payload should be monotone
non-increasing along a declared compression path. This is a design admissibility
rule, not a claim that every compiler artifact or every byte count already
shrinks. [`proofs/EvidenceLifecycleCore.v`](proofs/EvidenceLifecycleCore.v)
machine-checks the bounded lifecycle model; its companion
[`proofs/EvidenceLifecycleCore.md`](proofs/EvidenceLifecycleCore.md) fixes the
model/implementation boundary. Neither artifact owns live compiler semantics or
closes an SoT row.

## Loss Composition

Loss is path-sensitive and cumulative. Each lowering boundary reports a vector:

```text
LossVector = {
  semantic_loss,
  provenance_loss,
  timing_loss,
  runtime_debt
}

path_loss = boundary_1_loss + boundary_2_loss + ... + boundary_n_loss
```

Composition is component-wise addition. Acceptance is component-wise comparison
against the program or intent path budget. Therefore two boundaries that each
fit a local budget can still violate the cumulative path budget. A gate that
checks only each pass in isolation is insufficient.

The owner of the final `VerifiedProjectionPlan` must consume the composed loss,
not a last-pass snapshot. `LossCompositionCore.v` machine-checks identity,
associativity, component bounds, and a concrete counterexample where two
locally accepted losses exceed one path budget.

## Derived Mechanism Boundary

Execution lane, guard placement, materialization, layout, and backend projection
are compiler mechanisms only while they preserve the named observation class
and remain within the declared observable-cost/loss envelope. If either fact is
unproven, the compiler must not silently choose a default. It must retain an
inspectable checked path, require an explicit policy boundary, or reject.

`LossCompositionCore.v` formalizes this narrow rule as
`DerivedMechanism`: derivation requires observational equivalence and a cost
bound. It does not claim that the current compiler has proved those premises for
every mechanism.

## Erasure Evidence And Executable Decision

AIR owns the canonical intent/boundary compression **evidence classification**
after MIR/RIR/DAG evidence has been collected. It may certify or reject a
candidate projection, but it is not the executable erasure owner. The stable
AIR evidence fields are:

- `compression_budget`: `retain`, `summarize`, `erase`, or `forbid`;
- `compression_reason`: the human-readable reason exported with the AIR node;
- boundary `retain_cause`: `none`, `inherent`, `policy`, or `unproven`;
- summary counters: `unproven_retain_count`, `inherent_concurrency_count`, and
  `slot_capability_retain_count`.

The owner artifact is `pgy.air.graph.v1`. It is not a backend input. The
`VerifiedProjectionPlan` owner consumes the AIR evidence certificate together
with MIR/ABI and target-envelope facts and makes the only executable
retain/summarize/erase/materialize decision. C, LLVM, SelfHosted, and future
emitters consume that plan plus MIR/ABI facts; they must not read AIR or
rediscover the decision from source syntax, AST payloads, or backend-local
runtime symbol choices. A missing plan row is fail-closed. AIR compression by
itself is verifier/tooling evidence, never permission for backend-local
erasure.

These are not erasure decision points:

- AIR evidence classification: it constrains and certifies the plan but does
  not directly control emitted behavior;
- backend DCE/inlining: it may remove code, but it does not decide semantic
  erasure;
- runtime managers: they implement retained boundaries, but they do not decide
  whether a boundary should have been retained;
- source/AST readers: they provide provenance or parser facts, not physical
  materialization policy;
- `tests/air_erasure`: it is the independent physical-residue oracle that
  checks AIR's declaration against emitted machine-code residue.

If AIR declares `erase` and physical residue survives outside an expected drift
entry, the program has a compression-residue mismatch. If AIR declares `retain`
or `summarize`, the verified projection-plan row must cite the AIR certificate
and MIR/ABI facts that justify the remaining artifact; the backend cites that
plan row rather than reading AIR.

## Evidence-Amortization Cache Rule

Pergyra does not require every proof or guard to be zero cost. It requires that
the cost be owned, visible, and movable out of the hot path when the owner facts
prove that this is safe. A backend may therefore cache an evidence-derived view
only when the cache is an acceleration over the owner fact, not a new owner of
the fact.

Allowed cache pattern:

```text
owner fact -> preflight/evidence view -> hot-path use -> cleanup/invalidation
```

The cache may remember a typed view, rendered ABI name, local lookup snapshot,
or proof classification. It must not remember a stale source interpretation or
backend-local guess. The cache key must include the freshness facts that make the
view valid: identity, generation or epoch, access mode, layout/type fact,
authority/capability token where relevant, and the MIR/AIR region that owns
cleanup.

Forbidden cache patterns:

- caching a source/AST read to recover a fact already owned by MIR, AIR, DAG, or
  ABI metadata;
- caching a pointer or view across a release, move, generation change, async
  suspension, parallel boundary, token revocation, layout change, or cleanup
  edge;
- treating a cache hit as proof when the required owner fact is missing;
- falling back to per-access runtime checks without recording the retained
  materialization in AIR/MIR evidence.

The stable example is Slot Pin/Lease. A repeated Slot hot path may validate once
at the pin-region entry and consume a typed `ReadView<T>` or `WriteView<T>` in
the loop. That view is cacheable only for the lexical/MIR region whose
cleanup-edge facts prove invalidation. Cross-call, cross-intent, async,
parallel, or persistent caches require a future retained-materialization
contract; they are not beta-stable.

The performance claim is deliberately narrow: Pergyra is not a zero-cost
language. It is an evidence-amortized language. Proof cost may be paid once per
proven region and then consumed cheaply on the hot path, while remaining runtime
residue stays attributed as retain/summarize/erase/forbid evidence.

## Core Judgments

`A -[contract K]-> B`

Artifact `A` is abstracted into artifact `B` under loss contract `K`.

`K preserves fact`

The fact must be represented in `B`, or in evidence that `B` can cite without
reopening `A`.

`K loses fact as budget`

The fact is intentionally discarded at the boundary under the named budget.

`consumer may_read fact from owner`

A later layer may consume a fact only from the owner artifact named by the
contract.

`consumer forbidden_to_recover fact from source`

If a fact is lost or moved, a later layer must not reread the older source to
recover it. The owner must carry the fact forward, reject the program, or mark
the feature out of the stable surface.

`K compression budget fact`

The abstraction target must expose whether the source-level concept is retained
as runtime structure, summarized as evidence/provenance, erased after proof, or
forbidden to erase.

## Canonical Boundaries

### Parser To AST

Allowed loss:

- whitespace and comments that have no semantic role;
- concrete token grouping that is not needed for diagnostics or recovery.

Preserved facts:

- token/source span;
- declaration name;
- syntactic category;
- recovery diagnostics and adopted recovery value.

Forbidden:

- later semantic passes depending on raw source bytes for a fact already
  adopted into AST;
- recovery success without a diagnostic when the adopted value changes meaning.

### AST To HIR/DIR/RIR/MIR

Allowed loss:

- concrete syntax shape;
- parse-only recovery scaffolding;
- sugar that has a defined lower-level representation.

Preserved facts:

- source span and diagnostic provenance;
- symbol identity;
- type, generic, ability, and module facts through the DAG owner;
- resource, authority, effect, relation, projection, and handoff facts through
  DIR/RIR;
- body-flow, cleanup, pin, terminator, and backend-executable facts through
  HIR/MIR.

Forbidden:

- backend semantic rediscovery by walking AST payloads again;
- compatibility fallback that succeeds because a preserved fact was not carried
  by MIR/DIR/RIR/DAG;
- treating a provenance-only AST pointer as semantic truth.

### MIR To AIR

Allowed loss:

- exact instruction sequence when AIR only needs boundary/evidence presence;
- backend-specific representation names that are not semantic facts.

Preserved facts:

- boundary identity;
- evidence provider;
- source/provenance name;
- compression budget for intent/boundary evidence;
- compression `retain_cause` (inherent/policy/unproven — the A/B/C reason a retain
  was unavoidable), so the analyzer's "could not prove" verdict is preserved, not
  flattened into an undifferentiated retain;
- the lifecycle `CHECK`-guard count (`unproven_retain_count`, bucket C), the
  parallel/channel concurrency count (`inherent_concurrency_count`, bucket A),
  and the SecureSlot/DeviceSlot capability count
  (`slot_capability_retain_count`, bucket B) — program-level retain facts that
  no boundary node carries;
- cleanup, pin, terminator, select-receive, DAG, runtime, and observability
  evidence when those facts discharge an abstraction proof.

Forbidden:

- AIR creating lower-layer facts to make verification pass;
- AIR accepting summary counters as proof when first-class evidence inventory is
  required;
- AIR or a backend treating a source-level domain axis as a physical layout
  owner without a MIR/ABI layout fact;
- AIR becoming the owner of CFG, DAG, RIR, MIR, runtime, or backend truth.

### MIR To C And LLVM

Allowed loss:

- high-level source grouping;
- private compiler naming;
- generic spelling after canonical ABI lowering;
- backend-local temporary layout.

Preserved facts:

- observable behavior for the frozen subset;
- ABI ownership and lifetime shape;
- compression budget consumed from AIR/MIR, not rediscovered from syntax;
- panic/failure behavior;
- cleanup, drop, pin, and resource release effects;
- deterministic output where the gate requires stable text or stable behavior.

Forbidden:

- C and LLVM choosing different semantic owners;
- a backend inserting a zone/world/intent/slot carrier, padding, barrier, or
  runtime authority check from source syntax alone;
- backend text succeeding after a required MIR/DIR/RIR/DAG fact is missing;
- a backend accepting a source-level program that the shared semantic owner
  rejected.

## Compression Examples

The first stable compression vocabulary is intentionally conservative:

| Source abstraction | Compression budget |
|---|---|
| Pure intent step with no runtime-visible boundary, authority, effect, async, or failure policy | `erase` to the ordinary call sequence |
| Static zone contract with no runtime authority check | `summarize` as AIR provenance/evidence |
| World transfer, parallel, channel, IO, execution, or authority-bearing zone boundary | `retain` |
| Slot, Pin, or packed-layout carrier without ownership/layout proof | `forbid` until the owner fact exists |

This is the DOP/Zone rule: Zone is a semantic boundary. Physical memory layout,
padding, SoA/AoS shape, barriers, and ABI structs are decided only by the
ABI/Layout owner. A backend must not make a Zone physically heavier unless a
runtime, MIR, or ABI fact requires that cost.

Each `retain`/`summarize` additionally carries a `retain_cause` — `inherent`
(bucket A, irreducible runtime), `policy` (bucket B, kept by traceability/opt-out
policy), or `unproven` (bucket C, retained only because the analysis could not
discharge it). Only bucket C is improvable debt; it must trend downward. The
budget says *whether* a carrier remains, the cause says *why it had to*. This
separation is what lets the erasure dashboard (`docs/semantics/14`,
`tests/air_erasure`) attribute every physically-surviving primitive to a bucket
and gate that bucket C only ever shrinks — the honest claim is bounded, measured,
attributed loss, not zero loss.

### Self-Hosted Tool To C Oracle

Allowed loss:

- implementation language and internal data structure shape;
- private ordering when the tool contract declares semantic equality instead of
  byte equality.

Preserved facts:

- verdict;
- diagnostic code and reason/fix routing when the tool claims diagnostic
  parity;
- stable JSON or text schema when the tool is a compiler-adjacent validator.

Forbidden:

- self-hosted output becoming the deciding value before the C oracle agrees;
- tool-only success that bypasses C/LLVM parity for the stable subset.

## Executable Invariant: Loss Visibility

Every stable abstraction boundary must expose its accepted loss budget.

- Reason: unlisted loss becomes an invisible second source of truth.
- Evidence: `abstraction-loss-contract-test-smoke` checks that this document,
  the proof-pack index, AIR architecture, compiler contracts, and source-of-
  truth spine all name the same loss-contract vocabulary.
  `loss_contract_adequacy_smoke.sh` also checks
  `loss_contract_manifest.md`, a machine-readable boundary index that binds
  each canonical boundary to a live compiler stage and, where applicable, a
  live enforcement gate. Current manifest coverage is 4/5 gate-enforced
  (`ast_to_mir`, `mir_to_air`, `mir_to_backends`, `selfhost_to_oracle`) and
  1/5 documentation-only (`parser_to_ast`).
- Compression evidence: `pgy.air.graph.v1` exposes `compression_budget` and
  `compression_reason` for AIR intent and boundary nodes. The AIR invariant
  validator rejects unknown compression budgets, and the JSON schema smoke keeps
  the fields present for self-hosted and external tools.
- Remaining obligation: attach explicit contract blocks to each major compiler
  pass as the self-hosted substitution surface grows, and move `parser_to_ast`
  from documentation-only to an enforced manifest row.

## Executable Invariant: Preservation Carry

If a later stable layer needs a fact, the abstraction target or its evidence
must carry that fact without rereading the older source artifact.

- Reason: rereading the older source reopens the source-of-truth seam and makes
  the abstraction boundary non-binding.
- Evidence: current MIR/AIR/DAG source-of-truth smokes ratchet AST/source
  rediscovery paths to zero for the measured frontiers. The loss-contract
  manifest gives those boundaries a machine-readable stage/gate index, and
  `pass_contract_manifest.md` pins the major compiler passes to their required
  facts, preserved facts, invalidated facts, stable diagnostics, and forbidden
  reads.
- Remaining obligation: extend the manifest from stage/gate adequacy to
  per-boundary forbidden-read checks for every listed loss clause.

## Proof Obligation: Bounded Approximation Visibility

If a boundary uses a `bounded` or `runtime-checked` loss budget, the bound must
be visible to diagnostics, trace, or runtime validation.

- Reason: a bounded approximation that users cannot observe is indistinguishable
  from silent unsoundness.
- Evidence: AIR evidence diagnostics already expose provider/provenance for
  missing boundary evidence, and runtime frontier/Slot contracts expose runtime
  validation failures instead of treating them as static proof.
- Remaining obligation: standardize loss-budget names in diagnostic JSON and
  bind them back to the manifest rows.

## Syntax Position

Future Pergyra syntax may expose this idea with declarations such as:

```pgy
pass AstToMir
  loses { concrete_syntax_shape }
  preserves { symbol_identity, type_fact, ownership_boundary }
  forbids { backend_ast_semantic_read }
```

That syntax is a design sketch only. The beta contract today is the compiler
architecture rule: every abstraction boundary must name its owner, accepted
loss, preserved facts, forbidden downstream reads, and evidence gate before it
can be called stable.

## Acceptance Rule

An abstraction boundary is loss-contract aligned only when all of these are
true:

- the owner layer is named;
- the allowed loss and preserved facts are listed;
- downstream forbidden reads are listed;
- the last consumer of every preserved fact is named (`last consumer`
  field), and the fact is carried exactly that far: erased after full
  discharge, summarized when tooling/compatibility still needs a digest,
  retained when runtime safety/security needs the live fact, rejected when
  the evidence is missing or contradictory;
- the old compatibility path is rejected, removed, or quarantined behind a
  shrinking gate;
- tests or diagnostics prove the loss budget;
- documentation does not call the boundary lossless unless the budget is
  explicitly `zero`.

One sentence holds the whole discipline together: Pergyra is an
**evidence-carrying compiler, not an evidence-hoarding runtime**. Evidence
is carried to its last consumer and then compressed by its declared budget;
it is never erased without proof, and never kept past its last consumer
without a materialization reason. Coverage of this rule across every
`AIREvidenceKind` is gate-enforced: evidence_kind_manifest.md holds the
per-kind lifetime rows and `tests/evidence_lifetime_smoke.sh` keeps the
manifest and the enum in exact two-way correspondence (WO-A3).
