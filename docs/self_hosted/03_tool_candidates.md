# Soft Self-Host Tool Candidates

The first self-hosted programs should be small, useful, and easy to compare
against existing behavior.

All first-stage tools must produce stable JSON or diagnostics output. Human
readable text may be emitted as a secondary view, but the oracle comparison
must use deterministic machine-readable output.

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

Minimum JSON contract:

```json
{
  "schema": "pgy.selfhost.diagnostic-catalog.v1",
  "ok": true,
  "source": {
    "code_owner": "src/semantic/diag_codes.h",
    "docs_owner": "docs/72_diagnostic_codes.md"
  },
  "counts": {
    "codes": 0,
    "documented": 0,
    "missing": 0,
    "duplicates": 0
  },
  "findings": []
}
```

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

Minimum JSON contract:

```json
{
  "schema": "pgy.selfhost.air-graph-validator.v1",
  "input_schema": "pgy.air.graph.v1",
  "ok": true,
  "counts": {
    "intents": 0,
    "boundaries": 0,
    "evidence": 0,
    "drifts": 0
  },
  "findings": []
}
```

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

## Implemented Rung-2 Soft Tools

As of 2026-05-27, the `src/self_hosted/tools/` track has ten
rung-2 tools with parity scripts:

- `diagnostic_catalog_checker`
- `stable_subset_section_checker`
- `air_graph_json_validator`
- `backend_output_comparator`
- `module_manifest_resolver`
- `stdlib_dispatch_inventory_checker`
- `doc_link_checker`
- `production_header_size_checker`
- `production_c_size_checker`
- `examples_inventory_checker`

As of 2026-05-28, compiler-internal rung-1 substitutes use familiar stage names
for discoverability, but the target self-host layout is source-owner based and
does not copy the C implementation's fragmented file topology:

- `src/self_hosted/lexer/` compares byte-for-byte against `pgy --tokens`.
- `src/self_hosted/parser/` compares a growing text-tree subset
  byte-for-byte against `pgy --ast`.

They are deliberately not full compiler stages.

The full parity set is gated by `make self-host-preparation-test-smoke`. These
tools are still **soft self-host evidence**, not compiler-core migration.
