# Verified Projection Plan: Intent Observability Row

Status: `gate-backed-partial`

This document records the first native `VerifiedProjectionPlan` row. It closes
one decision only: whether the C or LLVM projection erases intent-observability
runtime support (`OBS0`) or materializes it (`OBS1`). It does not claim that the
full projection planner or all runtime materialization axes are complete. The
row is now bound to the AIR evidence certificate: C and LLVM may consume the
row only after the certificate fingerprint still matches the verified AIR
owner facts. It is also bound to the immutable target-capability envelope by a
deterministic target fingerprint, so a plan copied across a changed target
fact set can be carried through the plan identity. Persisted cross-target
freshness checking remains an explicit artifact-identity closure item.

## Owners

`src/compiler/mir_surface_usage.c` captures the canonical MIR inventory fact:

- `has_inventory_surface_usage_facts`
- `inventory_uses_intent_observability_surface`

`src/compiler/air_evidence_certificate.c` issues the deterministic
`pgy.air.certificate.v1` fingerprint after AIR verification. It hashes the
owner-owned evidence counts, intent/boundary facts, propagation facts, and
machine-layer contact sites; a missing MIR input, drift, or malformed storage
fails closed.

`src/compiler/verified_projection_plan.c` is the sole native consumer of the
target-capability envelope. It consumes that envelope and the AIR certificate
plus MIR fact, validates the target projection, and emits one
`PgyVerifiedProjectionPlanRow`. C and LLVM are downstream consumers of this
derived row only; they do not read, validate, or fingerprint the target
capability owner a second time.

The row contains a deterministic target-capability fingerprint as transport
integrity. That field is not a second target-capability SoT and backends must
only reject a missing row fingerprint.

The row's decision remains:

| MIR fact | disposition | runtime profile | reason |
|---|---|---|---|
| false | `ERASE` | `OBS0` | `mir:inventory:no_intent_observability_surface` |
| true | `MATERIALIZE` | `OBS1` | `mir:inventory:intent_observability_surface` |

Missing AIR certificates, stale certificate fingerprints, MIR, or inventory
facts fail closed. A missing target-capability fingerprint fails closed as
well; persisted cross-target freshness checking is not claimed by this
in-process row. The planner does not scan
AST, HIR direct calls, routine names, instruction strings, or source payloads.
C and LLVM select the same AIR-bound row after MIR validation; neither backend
owns a second observability-usage query and neither reads AIR directly.

The call ABI has one independent owner:
`src/common/intent_observability_abi.c`. Its 51 source-name-sorted rows own a
stable, non-zero, unique `RuntimeCallAbiId`, source name, runtime symbol, arity,
argument kind, and return kind. The current family-level argument fact is `Int`
for every handle/index argument and is consumed by semantic and LLVM type
projection rather than duplicated there. IDs are append-only identities, not
sorted row positions; a new source name receives a fresh ID without
renumbering existing rows.
Semantic checking, builtin classification, C emission, LLVM emission, and LLVM
runtime declaration consume those rows. The previous names-only table,
per-call `BuiltinKind` aliases, semantic spec table, and backend-local runtime
symbol tables are retired.

## Gate

`make verified-projection-plan-test-smoke` proves:

- missing MIR usage facts are rejected;
- an AIR certificate is required and its fingerprint mutation is rejected;
- the planner is the sole native target-envelope consumer and fingerprints it
  into the plan; C/LLVM reject a missing derived fingerprint without
  re-reading the envelope;
- `OBS0` maps to `ERASE` and `OBS1` maps to `MATERIALIZE`;
- C and LLVM consume target-specific rows from the same planner;
- all 51 ABI rows have unique non-zero IDs and deterministic source-name order;
- semantic/C/LLVM consumers use the canonical ABI owner;
- retired aliases, backend-local runtime symbols, and AST/HIR fallback scans
  cannot return;
- a negative source-inference mutation is detected.

## Remaining Closure

The native plan currently has one axis row with an in-process AIR certificate
fingerprint witness. Full projection closure still requires a persisted
cryptographic artifact digest/identity, typed layout and cleanup rows,
materialization residue attribution, Artifact Zone identity, and self-hosted
consumption of the same plan.
