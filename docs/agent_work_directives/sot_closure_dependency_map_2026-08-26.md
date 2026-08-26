# SoT Closure Dependency Map

Status: `AUDIT COMPLETE`

Base revision: `464a907a010b745c3ec1bdaecf783bbf9e31c037`

This is a temporary, nonnumbered multi-agent work directive. It is not a
semantic owner, progress board, implementation queue, or closure declaration.
Current source, `docs/semantics/sot_owner_spine_registry.md`, and executable
gates remain authoritative.

## Shared objective card

- Objective: account for every current `BRIDGE` or `ACTIVE` owner row and
  produce one dependency-ordered closure map without opening parallel
  implementation tracks.
- Priority order: exact registry coverage; actual old carrier and last live
  consumer; prerequisite owner edges; executable falsifier; then patch size.
- Fact owner: the current registry row plus its named authority source. Audit
  reports own observations only.
- Last legitimate consumer: the exact live consumer that still reads,
  reconstructs, or co-owns the fact; do not copy the registry's proposed
  consumer without verifying current source.
- Forbidden fallback: inferring closure from row count, owner filename, green
  tests, or documentation; proposing dual reads; changing a row to `CLOSED`;
  editing source, registry, handoff, progress, or another agent's report.
- Falsifying case: for each row name one executable fixture or negative gate
  that would fail if the old carrier were removed before its prerequisites,
  and one gate that must reject reintroduction after migration.

## Required row result

Each assigned owner must appear exactly once in the agent's report table with:

- current status and authority path;
- observed old carrier or missing stable-handle coverage;
- exact last live consumer with file and symbol;
- predecessor owner rows that must close first;
- smallest production executable rung that can reach the seam;
- one falsifying fixture and one negative ratchet;
- classification: `READY_NEXT`, `DEPENDENCY_BLOCKED`, `PRODUCT_BOUNDARY`, or
  `EVIDENCE_GAP`;
- observation versus inference clearly separated.

No agent may implement a proposed rung, change registry status, run a build,
or claim percentage progress. Read-only `rg`, `git show`, source inspection,
and static gates bounded to 60 seconds are allowed. Each agent edits only its
named report under `docs/audits/`.

## Independent scopes

### Semantic, syntax, verification, diagnostic

Report:
`docs/audits/2026-08-26_sot_semantic_syntax_verification_closure_map.md`

Rows: `diagnostic.catalog`, `air.evidence_graph`,
`lexer.language_word_registry`, `parser.syntax_provenance`,
`selfhost.match_case_pattern`, `dir.domain_graph`, `hir.typed_control_flow`,
`mir.generic_specialization`, `selfhost.enum_declaration_rows`,
`selfhost.expression_surface`, `selfhost.intent_declaration_rows`,
`selfhost.semantic_artifact_admission`, `selfhost.zone_authority_rows`,
`semantic.domain_runtime_assignment`, `semantic.loop_flow_summary`,
`semantic.nominal_field_kind`, and `semantic.symbol_type_graph`.

### Execution, projection, resource

Report:
`docs/audits/2026-08-26_sot_execution_projection_resource_closure_map.md`

Rows: `mir.execution_graph`, `semantic.machine_layer_transition`,
`projection.direct_mir_array_int_program`,
`projection.direct_mir_collection_pop_effect`,
`projection.direct_mir_collection_program_plan`,
`projection.direct_mir_scalar_cfg_foreach_receipt`,
`projection.direct_mir_scalar_cfg_program_extension`,
`projection.direct_mir_string_array_push`, `projection.verified_plan`,
`resource.region_allocation_plan`, `rir.resource_transition_graph`,
`semantic.function_param_flow_summary`, and
`semantic.resource_flow_universe`.

### ABI, target, compatibility

Report:
`docs/audits/2026-08-26_sot_abi_target_compatibility_closure_map.md`

Rows: `abi.intent_observability_rows`, `abi.layout_rows`,
`abi.mir_array_string_layout_projection`, `abi.runtime_call_rows`,
`semantic.callable_receiver_carriage`, `target.capability_profile`, and
`compatibility.evolution`.

## Integration owner and gate

The primary task is the only integration owner. It must compare the report
table owner-ID union with the current registry's `BRIDGE|ACTIVE` set and reject
any missing, extra, or duplicate row. That exact set-equality check is the one
integration gate for this audit. `tests/sot_authority_edge_smoke.sh` remains the
live registry prerequisite; it does not promote an audit proposal into an
implementation rung.

The primary task may select at most one `READY_NEXT` row after reviewing all
predecessor edges and observing a production falsifier. Agent report order,
confidence wording, or apparent patch size does not choose the successor.

## Integration result

- The current registry contains exactly 37 nonclosed rows. The three report
  tables contain exactly 37 unique owner IDs: no missing, extra, or duplicate
  row was observed.
- Classification is `READY_NEXT=2`, `DEPENDENCY_BLOCKED=24`,
  `EVIDENCE_GAP=11`, and `PRODUCT_BOUNDARY=0`. These are scheduling findings,
  not registry status or progress changes.
- Integration rejected the initial `selfhost.semantic_artifact_admission`
  successor: its 48,531,749-byte routine-1197 failure was historical. The
  current 236,684,385-byte canonical MIR emits byte-equal 10,464,651-byte
  gen2/gen3 C, and exact-revision remote full self-host is green.
- The sole selected successor is `abi.intent_observability_rows`. Public
  installed C/LLVM already consume carried RuntimeCallAbiId while the explicit
  native C and LLVM oracle emitters still call
  `pgy_intent_observability_abi_row_by_source`. The bounded migration must stamp
  the admitted ID once, consume it by ID in both native emitters, and reject
  missing, forged, or source/ID-mismatched carriage before artifact creation.
- `selfhost.match_case_pattern` remains the other bounded future candidate. It
  may not run in parallel with the selected ABI-ID consumer migration.
