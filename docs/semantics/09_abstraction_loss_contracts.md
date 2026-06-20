# Abstraction Loss Contracts

Last updated: 2026-06-16

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
| `evidence` | Smoke, regression, diagnostic, trace, or invariant proving the budget. |

Loss budget classes:

| Budget | Meaning |
|---|---|
| `zero` | No loss is accepted for this fact. The target must carry it exactly. |
| `bounded` | A representation choice is allowed, but it has a named bound. |
| `runtime-checked` | Static abstraction loses dynamic existence; runtime validates it. |
| `diagnostic-only` | The fact may be kept only for source spans, messages, or traces. |
| `forbidden` | The boundary may not lose this fact for the stable surface. |

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
- cleanup, pin, terminator, select-receive, DAG, runtime, and observability
  evidence when those facts discharge an abstraction proof.

Forbidden:

- AIR creating lower-layer facts to make verification pass;
- AIR accepting summary counters as proof when first-class evidence inventory is
  required;
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
- panic/failure behavior;
- cleanup, drop, pin, and resource release effects;
- deterministic output where the gate requires stable text or stable behavior.

Forbidden:

- C and LLVM choosing different semantic owners;
- backend text succeeding after a required MIR/DIR/RIR/DAG fact is missing;
- a backend accepting a source-level program that the shared semantic owner
  rejected.

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

## Theorem: Loss Visibility

Every stable abstraction boundary must expose its accepted loss budget.

- Reason: unlisted loss becomes an invisible second source of truth.
- Evidence: `abstraction-loss-contract-test-smoke` checks that this document,
  the proof-pack index, AIR architecture, compiler contracts, and source-of-
  truth spine all name the same loss-contract vocabulary.
  `loss_contract_adequacy_smoke.sh` also checks
  `loss_contract_manifest.md`, a machine-readable boundary index that binds
  each canonical boundary to a live compiler stage and, where applicable, a
  live enforcement gate. Current manifest coverage is 3/5 gate-enforced
  (`mir_to_air`, `mir_to_backends`, `selfhost_to_oracle`) and 2/5
  documentation-only (`parser_to_ast`, `ast_to_mir`).
- Remaining obligation: attach explicit contract blocks to each major compiler
  pass as the self-hosted substitution surface grows, and move
  `parser_to_ast` / `ast_to_mir` from documentation-only to enforced manifest
  rows.

## Theorem: Preservation Carry

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

## Theorem: Bounded Approximation Soundness

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
- the old compatibility path is rejected, removed, or quarantined behind a
  shrinking gate;
- tests or diagnostics prove the loss budget;
- documentation does not call the boundary lossless unless the budget is
  explicitly `zero`.
