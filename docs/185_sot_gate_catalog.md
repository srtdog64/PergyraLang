# SoT Gate Catalog

Status: `ACTIVE`

This document is the single operational index for source-of-truth gates. It
does not own compiler facts. Fact authority remains in
`docs/semantics/sot_owner_spine_registry.md`; this catalog explains how that
registry is enforced.

## 1. Objective Card

- Objective: prevent a second producer, undeclared fact carrier, fallback
  reconstruction, or stale cache from becoming semantic authority.
- Priority: semantic identity, one owner, declared projections, fallback
  removal, executable negative evidence, then patch size.
- Fact owner: the owner registry.
- Last legitimate consumers: the paths declared by each registry row.
- Forbidden fallback: AST/program-root re-scan, source-text/JSON semantic
  reconstruction, backend-local type/layout guesses, and cache-only answers.
- Verification: static authority edges first, then the active rung's executable
  missing-fact and C/LLVM/self-host parity gate.

## 2. Canonical Inputs

| Input | Role | Authority status |
|---|---|---|
| `docs/semantics/sot_owner_spine_registry.md` | owner rows and derived fact-carrier classification | canonical |
| `docs/semantics/proofs/SoTAuthority.v` | formal projection of registry owner/fact pairs | checked projection |
| `scripts/sot_registry_gate.py` | generic registry and relation validator | consumer |
| Per-rung parity scripts | executable missing/corrupt fact evidence | consumer |

The gate must not carry a copied owner list or copied status count. The Coq
mapping, registry summary, producer definitions, and self-host fact-owner file
coverage are compared to the registry at execution time.

## 3. Gate Inventory

| Gate | Cost budget | State | What failure means |
|---|---:|---|---|
| `make sot-authority-edge-test-smoke` | 60 s | LANDED | duplicate producer, unclassified fact owner, stale derived row, registry/Coq drift, forbidden layer input, or a CLOSED consumer reopened a named fallback |
| `make sot-authority-adequacy-test-smoke` | 60 s | LANDED, bounded | current typed-expression owner/source bindings or their negative source mutations drifted; this is not whole-compiler extraction evidence |
| `make self-host-codegen-assignment-projection-parity-test-smoke` | 5 min | LANDED, focused | semantic assignment target/expected type is missing, guessed, or differs across C/LLVM projection |
| `sot-missing-fact executable matrix` | 5 min per active rung | PARTIAL | a last consumer accepted a missing/corrupt canonical fact or emitted output through fallback |
| `layer-input capability checks in authority-edge gate` | 60 s | LANDED | self-host codegen re-produced semantic facts/imported parser owners, native backend read AIR/raw AST at its public boundary, or the one declared AST-text bridge drifted |
| `cache-shadow authority gate` | 5 min focused | NEXT | cache-on/off differs, stale owner revision hits, or a cache answers without canonical owner evidence |
| `CLOSED promotion gate` | 60 s + focused negative | PARTIAL | a row was promoted without old-path deletion, declared projection, fail-closed missing-fact behavior, and negative evidence |

## 4. Authority-Edge Rules

The static gate models these relations:

```text
owns(owner, fact)
writes(producer, fact)
reads(consumer, fact)
projects(derived_fact, source_fact)
caches(cache_fact, source_fact)
bridges(temporary_fact, source_fact)
reconstructs(consumer, source_carrier)
```

Required invariants:

1. A fact has exactly one authority root. One authority may expose several
   owner-local write operations; this is not multiple authority.
2. Every `*_fact_owner.pgy` is either an authority path or a classified
   `projection`, `cache`, `bridge`, or `local_view`.
3. A projection may copy representation but may not make a new semantic
   decision.
4. A `CLOSED` consumer may not contain a named reconstruction fallback.
5. Missing owner facts fail closed at the actual last consumer. A grep mutation
   alone cannot promote a row to production-grade closure.
6. Cache facts carry owner revision or equivalent freshness identity and are
   reproducible with the cache disabled.

## 5. Gate Placement

- Static authority-edge and bounded adequacy gates run in
  `self-host-preparation-contract-test-smoke`.
- Executable missing-fact and backend parity gates run only for the active
  substitution rung in the parity lane.
- Full C/LLVM/self-host matrices run at scheduled or merge boundaries.
- A new fact family must update the registry before its producer can land.
  Adding another path-specific grep script is not an acceptable substitute.

## 6. Closure Evidence

`CLOSED` is bounded to one registry row. Promotion requires all of the
following:

```text
owner identity fixed
all fact carriers classified
old producer/read path deleted
missing fact rejected by the real consumer
stable diagnostic or verifier failure observed
named fallback absent
negative gate prevents reintroduction
```

The current registry contains 36 authority rows and 11 explicitly derived
self-host fact carriers. Status remains `CLOSED=14 BRIDGE=7 ACTIVE=15` until an
executable rung changes that evidence.

`selfhost.assignment_type_verdict` reached `CLOSED` on 2026-07-15. The
semantic body-type bundle is produced once per driver path, transferred through
an explicit `own` boundary, and projected by codegen without semantic
reconstruction. The focused probe is positive-output equal under C/LLVM and
rejects both a missing assignment expected type and a missing indexed-target
type. It also rejects the former source-expression and backend-environment type
guess patterns before compiling the probe.

## 7. Next Execution Order

1. Extend the layer-input capability inventory as native AST/HIR bridges reach
   the active executable rung; do not add global zero claims early.
2. Add cache-disabled and stale-revision mutation cases to compiler-scale
   semantic memo and future incremental compilation caches.
3. Migrate existing CLOSED rows from source-copy mutation evidence to actual
   executable missing-fact fixtures as each row becomes the active rung.
