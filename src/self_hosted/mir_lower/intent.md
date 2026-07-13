# MIR JSON Lowering Substitute

## Intent

Provide a Pergyra-written hard self-hosting slice that consumes C-oracle
`pgy --mir-json` output and reconstructs the compact AST tree shape needed by
the Pergyra codegen substitute. This closes source-of-truth seams by forcing
the self-hosted path to consume MIR facts instead of rescanning source or AST
payloads for semantic answers.

## Compiler World Binding

- **world_zone**: `MirFactGraphZone`
- **stage_actor**: `MirLowerStage`
- **stage_intent**: `LowerProgramFacts`
- **intent_cluster**: `MiddleEndPipeline`
- **payload_contract**: `MirFactGraphPayloadContractReady`
- **manifest_binding**: `mir_lower|MirFactGraphZone|MirLowerStage|LowerProgramFacts|MirFactGraphPayloadContractReady`

## Input Contract

The input is a path to a `pgy.mir.v1` JSON document emitted by `pgy --mir-json`.
`run_owner.pgy` owns CLI mode selection and output orchestration;
`mir_json_input_owner.pgy` owns path selection, file reads, and schema gating;
`fixture_manifest_owner.pgy` owns the curated parity source fixture rows exposed
through `--fixture-manifest` for the shell runner;
`json_fact_read.pgy` owns bounded JSON/MIR fact reads including declaration
row, object, and array bounds; `routine_inventory_owner.pgy` owns routine
discovery plus bounded routine header facts, and `main.pgy` only calls the run
owner with `Args()`. Owner-qualified method routine
lookup is exposed as an `Option<Int>` fact so declaration lowering consumes
presence explicitly instead of using a `-1` sentinel. Routine discovery,
routine-name end, and routine block-start facts are also exposed as
`Option<Int>` so program/routine lowering must consume presence before
unwrapping positions. Supported facts are routine
signatures, source-local type facts, bounded CFG statement facts, nominal
declaration inventory facts, and explicitly listed declaration facts. Missing
or unsupported MIR facts are hard errors.

`expression_graph_fact_owner.pgy` owns `expr0_graph` decoding for migrated
branch, definition, value-return, Log, and bare-call instructions. It validates node
kinds, postorder child edges, root bounds, and reconstructed-artifact lane
binding before semantic/codegen consumption. The direct DRV-2 `--mir-json`
path requires this fact and never
rebuilds it from `expr0`; `--canonicalize-mir-json` is the named C-oracle bridge
that upgrades older native MIR JSON before comparison through the explicitly
named `SemanticAstArtifactAnalyzeCompactBridge` boundary.

`parallel_capture_fact_owner.pgy` validates the stable boundary ID, seal,
task count, unique row names, and the closed `snapshot_copy` /
`join_index_disjoint` / `join_readonly` kind set. `--verify-input` exposes this
same production input contract to parity and mutation gates; it is not a
second parser or a compatibility fallback.

## Output Contract

The output is the compact tree text used by `pgy --ast`, with LF line endings.
Unsupported input emits an observable `MIR-LOWER ERROR` diagnostic and exits
non-zero. The tool must never silently fall back to source text, raw AST
rescans, or default type guesses.

## Oracle

`tests/self_hosted/parity/mir_json_parity.sh` compares reconstructed output
against the C compiler's `pgy --ast` output for supported fixtures and checks
that unsupported fixtures fail closed. Supported fixture paths come from the
compiled `fixture_manifest_owner.pgy` manifest, not from shell-owned arrays. The
Makefile entry is
`self-host-mir-json-parity-test-smoke`.
