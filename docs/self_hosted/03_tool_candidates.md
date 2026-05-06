# Soft Self-Host Tool Candidates

The first self-hosted programs should be small, useful, and easy to compare
against existing behavior.

## 1. Diagnostic Catalog Checker

Input:

- diagnostic registry files,
- docs diagnostic tables,
- semantic/codegen diagnostic call sites.

Output:

- missing diagnostic codes,
- duplicate codes,
- missing reason/fix vocabulary,
- docs drift report.

Why first:

- Pure analysis.
- No backend complexity.
- Directly improves beta trust.

## 2. AIR Graph JSON Validator

Input:

- `pgy.air.graph.v1` JSON dump.

Output:

- schema validation,
- missing evidence nodes,
- boundary/evidence mismatch,
- drift summary.

Why:

- AIR is the future verification layer.
- JSON input avoids compiler-internal dependency.

## 3. MIR Dump Diff Tool

Input:

- two MIR dumps,
- optional expected stable subset manifest.

Output:

- declaration inventory diff,
- CFG edge diff,
- instruction payload diff,
- cleanup/pin fact diff.

Why:

- Supports C/LLVM parity and self-host migration.

## 4. Backend Output Comparator

Input:

- C backend output,
- LLVM backend smoke output,
- expected stdout/stderr.

Output:

- normalized diff,
- ABI mismatch summary,
- unsupported backend feature report.

Why:

- Keeps dual backend parity honest during migration.

## 5. Module Manifest Resolver Helper

Input:

- package/module manifests,
- import graph.

Output:

- normalized import graph,
- cycle diagnostics,
- visibility/export summary.

Why:

- Self-hosting needs reliable module boundaries before compiler code is split into Pergyra modules.

