# Beta Exit Handoff

Status: **HISTORICAL START CONTRACT**. Beta exit and the first tooling packages
have already been crossed. Do not resume from this document's numbered work
packages; the active executable rung is owned by the top card in
`docs/current_work_handoff.md`. This document remains the evidence for why hard
self-host was allowed to start.

This document defines the handoff from beta closure to self-host preparation.
It is intentionally narrow: if the beta contract is not closed, do not start a
self-host migration.

## Start Condition

Self-host preparation may begin after all of these are true:

- `docs/100_beta_readiness_checklist.md` has no open blocker for the stable
  subset.
- `docs/107_beta_stable_subset.md` matches the parser, semantic checker,
  diagnostics, examples, and C/LLVM support matrix.
- CFG/MIR body safety gates prove stable paths do not rely on AST fallback
  judgments.
- AIR strict evidence gates prove boundary, authority, MIR cleanup, DAG, and
  runtime schema evidence can be exported and validated.
- DAG evidence gates prove stable type resolution does not use the retired
  recursive resolver and does not leave unresolved metadata dead-ends on stable
  paths.
- Runtime ABI gates prove Slot/Pin/panic/failure contracts and C FFI layout are
  stable for the beta subset.
- The hard-self-host gap analysis has no unacknowledged substrate blocker:
  graph-heavy collections, file/path/string basics, scoped unsafe/raw escape,
  runtime profile selection, and debug-info strategy are either implemented or
  explicitly assigned to soft/partial self-host stages.
- C backend remains the first oracle, LLVM is the second oracle, and their
  frozen support matrix is green or explicitly unsupported. Hard self-host does
  not start while C/LLVM parity is still ambiguous, because the self-hosted
  implementation must be compared against a stable two-oracle baseline rather
  than used to decide which backend is correct. Use
  `self-host-backend-tri-compare-test-smoke` for the small comparator smoke and
  `self-host-backend-tri-compare-extended-test-smoke` for the opt-in 29-case
  C/LLVM/Pergyra comparator gate.
- A dogfood path exists through emitted C and at least one compiler-adjacent
  tool candidate has a concrete input/output contract.

## Historical Handoff Judgement (2026-06-22)

Hard self-host has started as staged substitution after the substrate gates
became ready. The allowed scope is still narrow: compiler-adjacent tooling,
stable JSON or dump validators, lexer/parser parity expansion, semantic/codegen
rungs, MIR JSON fact-only lowering, and tri-compare evidence that keeps C and
LLVM as the expected-value pair.

Broad compiler-core migration remains blocked. Do not replace the full parser,
type checker, MIR lowering, C backend, LLVM backend, or runtime in one jump.
Every hard rung must stay beside the C compiler oracle and the LLVM oracle when
enabled.

## First Work Package

The first self-host work package should be the **Diagnostic Catalog Checker**.
It is the safest entry point because it is pure analysis and can be compared
directly against existing docs and diagnostic registries.

Required shape:

- input: diagnostic registry, docs diagnostic tables, selected compiler call
  sites;
- output: duplicate/missing code report, missing `Reason:`/`Fix:` report, docs
  drift report;
- oracle: the current C compiler and shell smoke tests;
- gate: stable stdout/JSON output plus a negative fixture with intentional
  drift.

## Second Work Package

The second self-host work package should be the **AIR Graph JSON Validator**.
It validates the exact layer Pergyra wants future agents and tools to consume.

Required shape:

- input: `pgy.air.graph.v1`;
- output: schema validation, missing evidence node report, boundary/evidence
  mismatch report, drift summary;
- oracle: current `air_validate`, `air_verify`, and existing AIR smoke tests;
- gate: the Pergyra validator must agree with the C validator on positive and
  negative fixtures.

## Forbidden At Handoff

Do not start with a broad replacement of:

- parser rewrite;
- type checker rewrite;
- C backend rewrite;
- LLVM backend rewrite;
- native WASM backend;
- runtime replacement;
- new syntax added only for self-host convenience.

These tasks can only be considered as rung-by-rung substitutions after the
previous rung has run beside the C implementation.

Hard self-host remains blocked for any surface where C and LLVM do not agree on
the stable backend-compare suite. A Pergyra-written compiler pass is the third
implementation, not the decider between the two native oracles.

## Architecture Rule

Self-hosted code must not copy the C-era helper-file pattern. Use Pergyra
modules and namespaces as architecture:

- feature folder first;
- responsibility owner second;
- explicit input/output contract for every tool or pass;
- no `_helpers` module unless it is truly cross-feature infrastructure.

## Exit From Soft Self-Host

Soft self-host is considered successful when at least two compiler-adjacent
tools are written in Pergyra, run in CI, and produce stable outputs compared
against the C compiler oracle. Only then should partial compiler-pass migration
begin.
