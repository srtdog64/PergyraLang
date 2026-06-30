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
`mir_json_input_owner.pgy` owns path selection, file reads, and schema gating;
`json_fact_read.pgy` owns bounded JSON/MIR fact reads including declaration
row, object, and array bounds; `routine_inventory_owner.pgy` owns routine
discovery plus bounded routine header facts, and `main.pgy` only wires the
validated document into the lowering owners. Owner-qualified method routine
lookup is exposed as an `Option<Int>` fact so declaration lowering consumes
presence explicitly instead of using a `-1` sentinel. Routine discovery,
routine-name end, and routine block-start facts are also exposed as
`Option<Int>` so program/routine lowering must consume presence before
unwrapping positions. Supported facts are routine
signatures, source-local type facts, bounded CFG statement facts, nominal
declaration inventory facts, and explicitly listed declaration facts. Missing
or unsupported MIR facts are hard errors.

## Output Contract

The output is the compact tree text used by `pgy --ast`, with LF line endings.
Unsupported input emits an observable `MIR-LOWER ERROR` diagnostic and exits
non-zero. The tool must never silently fall back to source text, raw AST
rescans, or default type guesses.

## Oracle

`tests/self_hosted/parity/mir_json_parity.sh` compares reconstructed output
against the C compiler's `pgy --ast` output for supported fixtures and checks
that unsupported fixtures fail closed. The Makefile entry is
`self-host-mir-json-parity-test-smoke`.
