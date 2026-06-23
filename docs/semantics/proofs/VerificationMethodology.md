# Verification Methodology Core

Status: `proof-sketch`

This note is the companion to `VerificationMethodology.v`. It models one
discipline only: each verification method permits only the claim that its
evidence can support.

## Scope

The model covers the evidence ladder from
`docs/139_golden_adt_verification_methodology.md`:

- prose contracts make a rule reviewable;
- smoke gates catch known regressions;
- golden fixtures stabilize output shape;
- differential oracles compare implementations;
- verifier gates prove that an IR owner consumed required facts;
- mechanized models prove the small calculus they define;
- ADT owners identify the single source of truth for a fact;
- trace and capability evidence are required before runtime materialization can
  be treated as intentional.

## Negative Claims

The model intentionally proves the following negative boundaries:

- a golden fixture alone does not prove model soundness;
- a golden fixture alone does not qualify a hard self-hosted slice;
- a smoke gate alone does not qualify a hard self-hosted slice;
- a mechanized model alone does not prove implementation parity;
- a differential oracle alone does not prove model soundness.

These are guardrails against overclaiming. They do not replace backend compare,
IR verifiers, semantic tests, or proof-carrying IR checks.

## Positive Claims

The positive claims are narrow:

- hard self-hosted slices require ADT owner facts, golden fixtures,
  differential oracle comparison, verifier gates, and smoke gates;
- layout/niche soundness requires owner facts, verifier gates, golden layout
  fixtures, and typestate/refinement evidence;
- runtime materialization requires trace evidence, capability evidence, and a
  verifier gate;
- fact consumption is permitted when a verifier gate and an ADT owner agree.

This is not whole-compiler verification. It is a small proof that the
methodology vocabulary cannot collapse golden tests, differential testing,
verifiers, and mechanized proof into one ambiguous word.

