# Behavior Contract Closure Gaps

Last updated: 2026-06-19

Status: `beta-proof-obligation`

Stable surface: the gap between "DDD-style behavioral heuristics encoded in the
language" and "a closed behavior-contract core with explicit judgments,
typed evidence, and verifier-backed proof obligations."

This document is an anti-overclaim register. Pergyra already lowers intent,
effect, authority, zone, slot, CFG, MIR, and ABI facts into compiler evidence.
That is stronger than DDD as methodology. The remaining work is to make the
proof shape denser and more machine-checkable before claiming that the behavior
contract core is mathematically closed.

## Current Floor

These are already part of the beta proof surface:

- `docs/125_source_of_truth_spine.md` names the owner of each semantic fact and
  rejects downstream rediscovery.
- `docs/semantics/07_air_abstraction_safety.md` defines AIR intent/boundary
  evidence obligations and default strict evidence diagnostics.
- `docs/semantics/09_abstraction_loss_contracts.md` defines the loss contract
  vocabulary for compiler and tooling boundaries.
- `docs/semantics/08_slot_capability_calculus.md` defines the Slot capability
  calculus boundary and keeps mechanized-proof claims scoped.
- AIR/RIR/MIR/ABI smokes act as ratchets for source-of-truth drift, backend
  parity, authority evidence, cleanup evidence, pin evidence, and explicit ABI
  shape.

This is enough to say: Pergyra is not merely naming DDD concepts. It already
turns several behavioral boundaries into compiler facts.

It is not enough to say: the whole behavior contract system is closed as a
formal calculus.

## Compiler Maturity Shortcut

Pergyra should not try to copy Swift SIL or Rust MIR as surface architecture.
The useful shortcut is to import the proof discipline that made those compilers
survive production use:

1. Every IR has a verifier that owns its invariant.
2. Fallback paths are counted as debt, not compatibility features.
3. C, LLVM, and self-hosted tools agree on the declared oracle class.
4. Formal surface rules are specified before optimizer or layout cleverness is
   treated as stable.
5. Canonicalization comes before optimization: the same meaning must produce
   the same MIR, ABI facts, diagnostic codes, and deterministic ordering.

The competitive axis is Pergyra-specific: intent produces effects; effects
require authority; authority is discharged by evidence; coordination remains
deterministic; Slot/Zone ownership survives to ABI facts. If that evidence
chain is preserved through AOT lowering, Pergyra is not merely another
ahead-of-time language. It is an auditable intent compiler.

Current priority order:

1. Strengthen MIR verifier coverage for CFG/body safety, cleanup, branch/join,
   loop, channel, cancellation, and zone/effect transitions.
2. Strengthen AIR verifier coverage for intent/effect/authority/coordination
   evidence.
3. Strengthen DAG/type verifier coverage for generic/default/ability
   resolution as a single source of truth.
4. Remove C/LLVM backend semantic fallback paths and keep provenance fallback
   explicitly named.
5. Freeze ABI/layout facts with golden fixtures, including explicit-tag Option
   layout and future niche proofs.
6. Promote self-hosted tools only through three-way parity: C oracle, LLVM
   oracle, and Pergyra-origin tool output.
7. Add optimizer work only after canonical forms and miscompile-oriented
   regression fixtures exist.

## Gap 1: Canonical Judgment Rules

Current state:

- Judgments exist across the proof pack and AIR verifier prose.
- The implementation has concrete verifier behavior for missing HIR CFG, RIR
  boundary, RIR authority, MIR cleanup, MIR pin cleanup, MIR terminator, DAG,
  runtime, and observability evidence.

Remaining closure:

- Define a compact rule table for the stable behavior-contract core:
  - `IntentStep |- Effect(e) requires Authority(a)`.
  - `Boundary(b) |- Authority(a) discharged_by Evidence(kind, subject, provider)`.
  - `Zone(z); Slot(s) |- mutation allowed only inside declared boundary`.
  - `MIR(cfg) |- cleanup/pin/terminator evidence complete`.
  - `Backend(C, LLVM) |- consumes same MIR/DIR/RIR/DAG facts`.
- Give each rule a stable identifier that diagnostics and verifier tests can
  cite.

Acceptance:

- The proof pack contains the rule table.
- AIR/MIR/ABI diagnostics name the violated rule class or stable cause.
- No stable documentation calls a behavior contract closed without naming the
  rule that closes it.

## Gap 2: Typed Evidence Facts

Current state:

- `AIREvidenceNode` carries `kind`, `boundary_index`, `provider_name`,
  `subject_name`, `fact_count`, and `fallback_count`.
- AIR validation rejects malformed authority evidence, authority evidence on
  non-authority boundaries, and authority evidence without matching boundary
  evidence.
- Global AIR evidence validation consumes `kGlobalEvidencePolicies` as the
  shape table for MIR cleanup/terminator/select, RIR propagation, DAG
  metadata/generic/ability, observability schema, and runtime frontier-policy
  facts. The table owns fixed provider/subject names where the evidence class
  has a single schema owner and exact fact counts where the runtime schema has
  one stable count.

Remaining closure:

- Extend the same typed-table approach to every evidence class, not only global
  evidence. For each evidence kind, define:
  - provider layer;
  - subject kind;
  - boundary classes it may discharge;
  - required predecessor evidence;
  - whether fallback count is permitted.
- Make the evidence-shape table the verifier source of truth.

Acceptance:

- AIR validation consumes the table for evidence shape.
- Boolean summary flags cannot independently satisfy proof obligations.
- A missing or malformed evidence record fails closed with stable diagnostics.

## Gap 3: Strict Evidence Default Boundary

Current state:

- Default AIR is strict.
- `PGY_AIR_STRICT_EVIDENCE=0` exists as a development/debug opt-out and is used
  by non-impact tests to compare relaxed and strict backend output.

Remaining closure:

- Keep the relaxed mode out of stable correctness claims.
- Stable docs must distinguish "relaxed mode proves backend non-impact" from
  "strict mode proves behavior-contract safety".

Acceptance:

- No beta-stable claim uses relaxed AIR as the evidence that a program is
  behavior-contract safe.
- Relaxed AIR may appear only in differential/non-impact gates and debugging
  documentation.

## Gap 4: Machine-Readable Pass And Loss Manifest

Current state:

- `docs/semantics/09_abstraction_loss_contracts.md` defines the seven-field
  loss contract.
- `docs/125_source_of_truth_spine.md` defines the source-of-truth rule.

Remaining closure:

- Add a machine-readable manifest for compiler passes and tooling boundaries:
  `source`, `target`, `owner`, `loses`, `preserves`, `forbids`, and `evidence`.
- Make smokes consume that manifest instead of duplicating all rules as grep
  terms.

Acceptance:

- Every stable compiler boundary has a manifest row.
- A backend/source rediscovery path can be checked against `forbids`.
- Documentation and smoke gates use the same owner/loss vocabulary.

## Gap 5: Three-Way Self-Evidence

Current state:

- C/LLVM parity exists for the frozen backend surface.
- Self-hosted tools are growing as compiler-adjacent validators and oracles.

Remaining closure:

- For every compiler-adjacent self-hosted tool, define whether the oracle is:
  byte equality, normalized text equality, stable JSON equality, diagnostic
  equality, or semantic verdict equality.
- Keep self-hosted success advisory until the C/LLVM oracle agrees.

Acceptance:

- C, LLVM, and self-hosted outputs agree on the declared oracle class.
- Negative fixtures prove clean rejection, not only happy-path parity.
- A self-hosted tool cannot become the deciding source of truth before the C
  oracle and backend parity gate agree.

## Gap 6: Mechanized Scope Boundary

Current state:

- Slot capability calculus has a Coq proof sketch scoped to selected Slot and
  Pin invariants.
- The proof pack explicitly does not claim whole-language mechanized proof.

Remaining closure:

- Keep mechanization scoped and useful:
  - Slot/Pin token invariants first.
  - Then cleanup/pin evidence discharge.
  - Then authority-evidence discharge, only after the typed evidence table is
    stable.

Acceptance:

- CI type-checks any mechanized artifact that is cited as evidence.
- No document presents proof sketches as completed mechanized proof.
- Whole-language soundness remains a theorem-statement plus evidence-pack claim
  until a mechanized model exists for the relevant judgments.

## Non-Goals

This closure path does not mean:

- embedding DDD terms such as aggregate/entity/repository as core syntax;
- proving arbitrary business invariants;
- making AIR a codegen IR;
- turning Pergyra into a general theorem prover;
- replacing runtime existence checks for dynamic Slot/token/registry facts.

## Closure Rule

The behavior-contract core may be called closed only when:

- each stable behavior claim maps to a named judgment rule;
- each proof obligation maps to typed evidence, not just a boolean flag;
- strict evidence is the stable proof path;
- pass/loss ownership is machine-readable;
- C, LLVM, and self-hosted validators agree on their declared oracle class;
- mechanized proof claims are scoped to artifacts actually checked by CI.

Until then, the honest status is:

> Pergyra has compiler-enforced behavior-contract evidence for important
> boundaries, but the full behavior-contract calculus remains a beta proof
> obligation.
