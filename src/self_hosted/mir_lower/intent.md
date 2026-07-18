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

`routine_fact_index_owner.pgy` also consumes the routine-owned
`source_syntax_id` and `function_param_flow_summaries` rows. It checks the
declared row count, stable identity presence, non-negative parameter/mask
values, and duplicate parameter indexes before `routine_lower.pgy` can emit a
routine. A malformed summary is an observable `MIR-LOWER ERROR`; it is never
recovered by reopening HIR or source text.

The same owner consumes the MIR routine-owned function-local
`ResourceFlowUniverse` projection through `resource_flow_symbol_count` and
`resource_flow_symbols`. HIR is only the native adapter that copies the rows;
MIR JSON emits the MIR-owned storage. Each row
carries the stable symbol index, declaration syntax identity, symbol kind,
parameter boundary, and canonical name. Declared counts, required fields, and
duplicate stable indexes are checked before lowering; a missing or malformed
row is an observable `MIR-LOWER ERROR`, never a source or pointer-identity
fallback.

The same owner consumes the native `LoopFlowSummary` projection through
`loop_flow_summary_count`/`loop_flow_summaries` and
`loop_flow_state_count`/`loop_flow_states`. Summary rows carry loop syntax
identity, while/for kind, effect delta, and entry/exit state spans; state rows
carry stable ResourceFlowUniverse indices and immutable transfer state.
The projection admits only canonical CFG loop headers; a terminal while body
with no backedge may be accepted from its explicit transfer branch, but no
summary row is synthesized or re-derived from source text.
Declared counts, kind values, duplicate loop identities, every span bound, and
the cross-family binding from each state stable index to the routine's
ResourceFlowUniverse rows are checked before lowering. Missing or malformed
rows are hard errors; the self-host producer's explicit empty projection is
not a recovery path for native semantic rows.

Before CFG reconstruction, `routine_lower.pgy` consumes the same rows as an
admission fact: every loop header it would render must have one native summary
row with the matching `while`/`for` kind, and every summary must correspond to
one such header. A count or kind mismatch is a hard `MIR-LOWER ERROR`; the
lowerer never reopens source text or invents a summary from CFG edges.

`expression_graph_fact_owner.pgy` owns `expr0_graph` decoding for migrated
branch, definition, value-return, Log, ArrayPush, ArraySet, and bare-call instructions. It validates node
kinds, postorder child edges, root bounds, and reconstructed-artifact lane
binding before semantic/codegen consumption. The direct DRV-2 `--mir-json`
path requires this fact and never
rebuilds it from `expr0`. `--canonicalize-mir-json` is also graph-only and
fails closed when `expr0_graph` is missing or malformed. The native MIR JSON
writer now projects the bounded scalar/binary/direct-call graph slice directly
from its instruction AST into the same graph schema. It does not parse
`expr0`; unsupported AST shapes remain `null` and therefore fail closed when
the hard consumer requires them. Older graph-less artifacts can be upgraded
only by the explicitly named `--canonicalize-oracle-mir-json` compatibility
boundary, which reuses the canonical Pergyra expression parser through
`SemanticAstArtifactAnalyzeCompactBridge`. The hard consumer cannot invoke
that bridge.

`parallel_capture_fact_owner.pgy` validates the stable boundary ID, seal,
task count, unique row names, and the closed `snapshot_copy` /
`join_index_disjoint` / `join_readonly` kind set. `--verify-input` exposes this
same production input contract to parity and mutation gates; it is not a
second parser or a compatibility fallback.

`machine_layer_fact_owner.pgy` validates every explicit `machine_contact_kind`
and its `machine_layer` object against the checked self-host runtime-call
projection row plus the native `pgy.machine-layer.declaration.v1` artifact
passed to `mir_lower` as its second path. A contact kind with a missing/null
owner object, a manifest or runtime-operation mismatch, an inadequate
authority/lease bit, or a missing/mutated declaration is a hard
`MIR-LOWER ERROR`; the self-host path never infers the contact from expression
spelling.

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
