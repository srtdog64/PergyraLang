# Evidence Kind Manifest

Status: `beta-proof-obligation`

Registry for `AIREvidenceKind` (src/compiler/air.h): every evidence kind the
AIR verifier carries must declare its producer, its discharge point, its last
consumer, and its compression budget (docs/semantics/09 contract shape, with
the `last consumer` field). The evidence-lifetime gate
(`tests/evidence_lifetime_smoke.sh`) holds this table and the enum in exact
two-way correspondence: adding an evidence kind without a row, or keeping a
row for a removed kind, is RED. Rows must point at producer files and gates
that exist.

Reading the columns:

- `producer` — the file that materializes the evidence node from upstream IR
  facts (HIR/RIR/MIR/DAG/runtime policy).
- `discharge` — the validator that consumes the evidence as a verification
  obligation. After discharge the evidence is no longer load-bearing for
  compilation.
- `last consumer` — the final stage that still reads the fact:
  `pgy.air.graph.v1` (air_dump_json.c) is the tooling/compatibility digest
  for every kind; kinds that also feed driver diagnostics or runtime
  projections name them here. Erasure before this point violates the
  contract; retention past it is hoarding.
- `budget` — docs/semantics/09 compression budget. All AIR evidence rows are
  `summarize`: the graph itself never reaches a backend or the runtime
  (IRMinimality: AIR is off-path); what survives is the JSON digest plus any
  named artifact projection.

<!-- BEGIN evidence-kind-manifest -->
| kind | producer | discharge | last consumer | budget |
|---|---|---|---|---|
| AIR_EVIDENCE_HIR_ROUTINE | src/compiler/air_evidence_hir.c | src/compiler/air_validate_boundary_evidence.c | air_dump_json.c + driver_diag.c | summarize |
| AIR_EVIDENCE_HIR_CFG | src/compiler/air_evidence_hir.c | src/compiler/air_validate_boundary_evidence.c | air_dump_json.c + driver_diag.c | summarize |
| AIR_EVIDENCE_RIR_BOUNDARY | src/compiler/air_evidence_rir_boundary.c | src/compiler/air_validate_boundary_evidence.c | air_dump_json.c + driver_diag.c | summarize |
| AIR_EVIDENCE_RIR_AUTHORITY | src/compiler/air_evidence_rir_boundary.c | src/compiler/air_validate_boundary_evidence.c | air_dump_json.c + driver_diag.c | summarize |
| AIR_EVIDENCE_MIR_CLEANUP | src/compiler/air_evidence_mir_facts.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_MIR_PIN_CLEANUP | src/compiler/air_evidence_mir_pin.c | src/compiler/air_validate_boundary_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_MIR_TERMINATOR | src/compiler/air_evidence_mir_facts.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_MIR_SELECT_RECEIVE | src/compiler/air_evidence_mir_facts.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_DAG_METADATA | src/compiler/air_evidence_dag.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_DAG_GENERIC | src/compiler/air_evidence_dag.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_DAG_ABILITY | src/compiler/air_evidence_dag.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_RIR_EFFECT_PROPAGATION | src/compiler/air_evidence_rir_propagation.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_RIR_RELATION_PROPAGATION | src/compiler/air_evidence_rir_propagation.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c | summarize |
| AIR_EVIDENCE_OBSERVABILITY_SCHEMA | src/compiler/air_evidence_runtime.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c + runtime trace-export projection | summarize |
| AIR_EVIDENCE_RUNTIME_FRONTIER_POLICY | src/compiler/air_evidence_runtime.c | src/compiler/air_validate_global_evidence.c | air_dump_json.c + runtime lane-scheduler facade | summarize |
<!-- END evidence-kind-manifest -->

Gate coverage for the shared surfaces:

- graph digest shape: `tests/air_json_schema_smoke.sh`
- evidence drift (declared vs validated): `tests/air_drift_smoke.sh`
- machine-neutral projection ownership: `tests/machine_neutral/`
- this registry's enum correspondence + RED self-test:
  `tests/evidence_lifetime_smoke.sh`

Target-topology note (docs/180 §2): when the Verified Projection Plan owner
lands, the AIR verifier's compact **evidence certificate** becomes an
additional last consumer surface for every kind here — the Projection
Planner cites the certificate; backends still never read this graph. Rows
should gain the certificate column when that owner exists, not before.

Related: docs/semantics/09 (contract shape and budgets),
docs/semantics/14 §0a (single decision point), loss_contract_manifest.md /
pass_contract_manifest.md (the sibling registries this one extends),
docs/180 §7 "AIR Evidence Lifetime Gate" row (CI promotion is the declared
missing closure), TODO board WO-A3.
