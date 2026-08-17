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
`src/common/intent_observability_abi.def`. Its 51 source-name-sorted rows own a
stable, non-zero, unique `RuntimeCallAbiId`, source name, runtime symbol, arity,
parameter shape, and return kind. The explicit `PARAMS_NONE`, `PARAMS_INT`, and
`PARAMS_INT_INT` tokens derive both arity and argument kinds for native and
self-host consumers; neither consumer reconstructs an all-`Int` policy from a
count. IDs are append-only identities, not sorted row positions; a new source
name receives a fresh positive ID without renumbering existing rows.
Semantic checking, builtin classification, C emission, LLVM emission, and LLVM
runtime declaration consume those rows. The previous names-only table,
per-call `BuiltinKind` aliases, semantic spec table, and backend-local runtime
symbol tables are retired. The generated self-host projection returns one
complete immutable row per index; the six independent 51-branch selectors are
also retired. The installed self-host C and LLVM routes now derive their usage
receipt, runtime call spelling, and enabled runtime-header boundary from that
same row. The public installed routes and both native oracles execute canonical
zero-, one-, and two-argument rows with exact stdout `0`, `false`, and an empty
line, without native re-entry. Persisted `pgy.mir.v1` expression facts now
carry `RuntimeCallAbiId` in both native 7-field and self-host complete 10-field
node rows. Direct-MIR admission cross-seals the carried ID against the canonical
source/name/parameter-shape row, then C and LLVM resolve the runtime symbol only
through `RowForId`. Missing, mismatched, forged non-observability, and mixed
source-syntax/runtime identities fail before artifact publication. Full intent
execution is no longer wholly open: the installed self-host default-priority
legacy emitter projects enter/step/bind/materialize/fail/ok/exit events from
admitted mode, signature, participant, zone, and slot facts. Its success and
guard/expect/post failure histories, reverse compensation, final failure, and
active-count exit state match native C and LLVM. Typed v2 execution-plan
observability, non-default priority preservation, and the compiler-purpose root
intent remain separate open obligations.

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
- a lexically middle row with a fresh non-positional ID preserves all existing
  identities, while duplicate and non-positive IDs fail closed;
- zero-, one-, and two-`Int` parameter shapes project identically through
  native semantic, C, LLVM, and self-host signature consumers;
- native and self-host MIR carry stable IDs 25, 1, and 13 through both direct
  C and LLVM execution paths, while four identity mutations fail closed;
- installed self-host intent execution records the same success/failure history
  and reverse-compensation outcome as native C/LLVM, and LLVM compensation does
  not overwrite a failure phase with a forward materialization event;
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
