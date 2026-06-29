#!/usr/bin/env bash
# Gates the self-hosted compiler source shape. Hard substitution should grow as
# Pergyra world/intent-owned flow, not as a copy of the C compiler folder graph.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-compiler-world] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

require_max_lines() {
    local rel="$1"
    local cap="$2"
    local count
    count="$(wc -l < "$ROOT_DIR/$rel" | tr -d ' ')"
    [[ "$count" -le "$cap" ]] ||
        fail "$rel has $count lines; cap is $cap"
}

forbid_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$ROOT_DIR/$rel"; then
        fail "$rel contains forbidden term: $term"
    fi
}

manifest_contains() {
    local expected="$1"
    local rel

    for rel in \
        "$PGY_SELFHOST_SOURCE_DIR" \
        "$PGY_SELFHOST_TEST_DIR" \
        "$PGY_SELFHOST_PARITY_DIR" \
        "${PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}"; do
        [[ "$rel" == "$expected" ]] && return 0
    done
    return 1
}

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"
pgy_prepend_windows_runtime_paths

require_file "src/self_hosted/compiler/README.md"
require_file "src/self_hosted/compiler/world.pgy"
require_file "src/self_hosted/compiler/path_manifest_owner.pgy"
require_file "src/self_hosted/compiler/stage_intents.pgy"
require_file "src/self_hosted/compiler/target_capability_owner.pgy"
require_file "src/self_hosted/compiler/air_evidence_owner.pgy"
require_file "src/self_hosted/compiler/artifact_zone_owner.pgy"
require_file "src/self_hosted/compiler/test_harness_owner.pgy"
require_file "src/self_hosted/compiler/subprocess_runner_owner.pgy"
require_file "src/self_hosted/compiler/abi_layout_row_owner.pgy"
require_file "src/self_hosted/compiler/symbol_table_owner.pgy"
require_file "docs/self_hosted/11_compiler_world_architecture.md"
require_file "docs/self_hosted/12_intent_zone_self_host_architecture.md"
require_file "docs/self_hosted/13_compiler_substrate_architecture.md"
require_file "docs/self_hosted/14_target_compiler_world.md"
require_file "docs/self_hosted/15_pre_self_host_expansion_ledger.md"
require_file "tests/self_host_compiler_world_contract_smoke.sh"
require_file "tests/self_hosted/compiler_world_manifest.sh"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_PATH_MANIFEST_PATH"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "### Pergyra-Style Self-Host Test"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "C, LLVM, and self-hosted outputs are peer projections over the same facts"
require_text "src/self_hosted/compiler/README.md" "## Pergyra-Style Check"
require_text "src/self_hosted/compiler/README.md" 'not as a C folder graph rewritten in `.pgy`'

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"
pgy_compiler_world_require_stage_conformance "$ROOT_DIR" ||
    fail "compiler world stages do not conform to the on-disk stage owners"
require_max_lines "src/self_hosted/compiler/world.pgy" 600
require_max_lines "src/self_hosted/compiler/path_manifest_owner.pgy" 200
require_max_lines "src/self_hosted/compiler/target_capability_owner.pgy" 200
require_max_lines "src/self_hosted/compiler/air_evidence_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/artifact_zone_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/test_harness_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/subprocess_runner_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/abi_layout_row_owner.pgy" 120
require_max_lines "src/self_hosted/compiler/symbol_table_owner.pgy" 120

for term in \
    "world PgyCompilerWorld" \
    "zone SelfHostCompiler" \
    "zone SourceIntakeZone" \
    "zone TokenStreamZone" \
    "zone AstTreeZone" \
    "zone SemanticVerdictZone" \
    "zone MirFactGraphZone" \
    "zone TypeEnvZone" \
    "zone AbiLayoutZone" \
    "zone TargetCapabilityZone" \
    "zone AirEvidenceZone" \
    "zone SymbolFactTableZone" \
    "zone AbiRowProjectionZone" \
    "zone EmissionZone" \
    "zone ArtifactZone" \
    "zone TestHarnessZone" \
    "zone SubprocessRunnerZone" \
    "zone ParityZone" \
    "intent CompilePergyraProgram" \
    "step Frontend" \
    "step MiddleEnd" \
    "step Evidence" \
    "step Backend" \
    "step SelfProof" \
    "FrontendPipeline" \
    "MiddleEndPipeline" \
    "BackendPipeline" \
    "SelfProofPipeline" \
    "import \"stage_intents.pgy\"" \
    "intent IntakeSource" \
    "intent LexSource" \
    "intent ParseTokens" \
    "intent CheckProgramSemantics" \
    "intent LowerProgramFacts" \
    "intent EmitProgramArtifact" \
    "intent PlanTargetProjection" \
    "intent ProveHardSelfHostEvidence" \
    "intent ProveSelfHostedParity" \
    "subject SourceUnit" \
    "subject LexerStage" \
    "subject ParserStage" \
    "subject SemanticStage" \
    "subject MirLowerStage" \
    "action Scan" \
    "action BuildAst" \
    "action Check" \
    "action Lower" \
    "subject ProgramEmitter" \
    "action Emit" \
    "subject TargetProjectionPlanner" \
    "action Plan" \
    "subject AirEvidenceOwner" \
    "subject SymbolTableOwner" \
    "subject AbiRowProjector" \
    "subject ArtifactSink" \
    "subject TestHarnessRunner" \
    "subject SubprocessRunner" \
    "subject OraclePair" \
    "object SourceBatch" \
    "object StagePathManifest" \
    "compiler_world: String" \
    "source_dir: String" \
    "test_dir: String" \
    "parity_dir: String" \
    "object TokenStream" \
    "object AstTree" \
    "object SemanticVerdict" \
    "object MirFactGraph" \
    "object TypeEnvironment" \
    "object AbiLayoutFacts" \
    "object TargetCapabilityEnvelope" \
    "object AirEvidenceFacts" \
    "object SymbolFactTable" \
    "object AbiLayoutRows" \
    "object EmittedC" \
    "object ArtifactEvidence" \
    "object TestHarnessFacts" \
    "object SubprocessCapabilityEnvelope" \
    "tobject ParityVerdict" \
    "subject slot lexer: LexerStage" \
    "subject slot parser: ParserStage" \
    "subject slot checker: SemanticStage" \
    "subject slot lowerer: MirLowerStage" \
    "subject slot emitter: ProgramEmitter" \
    "object slot abi_layout: AbiLayoutFacts" \
    "object slot target_capability: TargetCapabilityEnvelope" \
    "object slot air_evidence: AirEvidenceFacts" \
    "object slot symbols: SymbolFactTable" \
    "object slot abi_rows: AbiLayoutRows" \
    "object slot artifacts: ArtifactEvidence" \
    "object slot harness: TestHarnessFacts" \
    "object slot subprocess: SubprocessCapabilityEnvelope" \
    "object slot layouts: AbiLayoutFacts" \
    "object slot envelope: TargetCapabilityEnvelope" \
    "object slot facts: AirEvidenceFacts" \
    "object slot symbols: SymbolFactTable" \
    "object slot rows: AbiLayoutRows" \
    "object slot evidence: ArtifactEvidence" \
    "zone abi_layout: AbiLayoutZone" \
    "zone target_capability: TargetCapabilityZone" \
    "zone air_evidence: AirEvidenceZone" \
    "zone symbols: SymbolFactTableZone" \
    "zone abi_rows: AbiRowProjectionZone" \
    "zone artifacts: ArtifactZone" \
    "zone harness: TestHarnessZone" \
    "zone subprocess: SubprocessRunnerZone" \
    "BackendPipeline(types, abi_layout, target_capability_zone, emit_zone, target_planner, emitter)"; do
    require_text "src/self_hosted/compiler/world.pgy" "$term"
done
require_text "src/self_hosted/compiler/world.pgy" 'import "path_manifest_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "target_capability_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "air_evidence_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "artifact_zone_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "test_harness_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "subprocess_runner_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "abi_layout_row_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "symbol_table_owner.pgy"'

for term in \
    "func SelfHostSourceDir" \
    "func SelfHostTestDir" \
    "func SelfHostParityDir" \
    "func CompilerWorldPath" \
    "func CompilerPathManifestPath" \
    "func CompilerStageIntentsPath" \
    "func CompilerTargetCapabilityOwnerPath" \
    "func CompilerAirEvidenceOwnerPath" \
    "func CompilerArtifactZoneOwnerPath" \
    "func CompilerTestHarnessOwnerPath" \
    "func CompilerSubprocessRunnerOwnerPath" \
    "func CompilerAbiLayoutRowOwnerPath" \
    "func CompilerSymbolTableOwnerPath" \
    "func CompilerOwnerManifestPath" \
    "func CompilerStagePathAt" \
    "func CompilerParityPathAt" \
    "func CompilerWorldManifestPathAt"; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$term"
done
for term in \
    "return CompilerPathManifestPath();" \
    "return CompilerStageIntentsPath();" \
    "return CompilerTargetCapabilityOwnerPath();" \
    "return CompilerAirEvidenceOwnerPath();" \
    "return CompilerArtifactZoneOwnerPath();" \
    "return CompilerTestHarnessOwnerPath();" \
    "return CompilerSubprocessRunnerOwnerPath();" \
    "return CompilerAbiLayoutRowOwnerPath();" \
    "return CompilerSymbolTableOwnerPath();" \
    "return CompilerOwnerManifestPath();" \
    "if index < 16" \
    "CompilerStagePathAt(index - 11)" \
    "if index < 22" \
    "CompilerParityPathAt(index - 16)"; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$term"
done

for term in \
    "func CompilerAirEvidenceSchema" \
    "CompilerAirEvidenceEnvelopeReady" \
    "intent_graph" \
    "authority_evidence" \
    "coordination" \
    "materialization_reason" \
    "loss_budget"; do
    require_text "src/self_hosted/compiler/air_evidence_owner.pgy" "$term"
done

for term in \
    "func CompilerArtifactSchema" \
    "CompilerArtifactZoneReady" \
    "diagnostics" \
    "ir_json" \
    "abi_layout" \
    "emitted_self_hosted" \
    "run_output"; do
    require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "$term"
done

for term in \
    "func CompilerTestHarnessSchema" \
    "CompilerTestHarnessReady" \
    "source_path" \
    "expected_diagnostic" \
    "expected_abi_layout" \
    "c_oracle" \
    "llvm_oracle" \
    "self_hosted"; do
    require_text "src/self_hosted/compiler/test_harness_owner.pgy" "$term"
done

for term in \
    "func CompilerSubprocessSchema" \
    "CompilerSubprocessRunnerReady" \
    "executable_path" \
    "argv" \
    "env_allowlist" \
    "timeout_ms" \
    "oracle_compare" \
    "artifact_probe" \
    "CompilerSubprocessOracleCompareTimeoutMs" \
    "CompilerSubprocessOracleCompareEnvAllowlist" \
    "CompilerSubprocessOracleComparePlanReady"; do
    require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "$term"
done

for term in \
    "func CompilerAbiLayoutRowSchema" \
    "CompilerAbiLayoutRowsReady" \
    "CompilerAbiLayoutConcreteRowCount" \
    "CompilerAbiLayoutCValueType" \
    "CompilerAbiLayoutFieldAllowed" \
    "CompilerAbiLayoutRowMaterializationAt" \
    "field_order" \
    "tag_kind" \
    "niche" \
    "ownership_shape" \
    "materialization_policy"; do
    require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "$term"
done

for term in \
    "func CompilerSymbolTableSchema" \
    "CompilerSymbolTableReady" \
    "CompilerSymbolProjectionAt" \
    "CompilerSymbolCQualifiedName" \
    "source_owner" \
    "namespace_path" \
    "c_symbol" \
    "llvm_symbol" \
    "self_hosted_symbol" \
    "collision_policy"; do
    require_text "src/self_hosted/compiler/symbol_table_owner.pgy" "$term"
done

for term in \
    "func CompilerTargetCapabilitySchema" \
    "pgy.selfhost.target-capability-envelope.v1" \
    "func CompilerTargetProjectionAt" \
    "func CompilerTargetFactAt" \
    "func CompilerTargetFallbackReasonAt" \
    "func CompilerTargetCapabilityEnvelopeReady" \
    "intent_graph" \
    "effect_set" \
    "authority_evidence" \
    "coordination" \
    "slot_ownership" \
    "layout_shape" \
    "loss_budget" \
    "materialization_reason" \
    "unsupported_shape" \
    "forbidden_loss_budget" \
    "retained_effect" \
    "missing_authority_evidence" \
    "host_only_slot_boundary"; do
    require_text "src/self_hosted/compiler/target_capability_owner.pgy" "$term"
done

for rel in \
    "$PGY_SELFHOST_SOURCE_DIR" \
    "$PGY_SELFHOST_TEST_DIR" \
    "$PGY_SELFHOST_PARITY_DIR" \
    "${PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}"; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$rel"
done

while IFS= read -r manifest_path; do
    if ! manifest_contains "$manifest_path"; then
        fail "path_manifest_owner.pgy contains path outside compiler-world shell manifest: $manifest_path"
    fi
done < <(
    grep -oE 'return "(src|tests)/[^"]+"' \
        "$ROOT_DIR/src/self_hosted/compiler/path_manifest_owner.pgy" |
        sed -E 's/^return "([^"]+)"/\1/' |
        sort -u
)

for term in \
    "intent FrontendPipeline" \
    "intent MiddleEndPipeline" \
    "intent BackendPipeline" \
    "target_capability: TargetCapabilityZone" \
    "intent SelfProofPipeline" \
    "step Intake" \
    "step Lex" \
    "step Parse" \
    "step Check" \
    "step Lower" \
    "step Emit" \
    "step PlanTarget" \
    "step Prove" \
    "IntakeSource(intake, source)" \
    "LexSource(tokens, lexer)" \
    "ParseTokens(ast, parser)" \
    "CheckProgramSemantics(semantic_zone, checker)" \
    "LowerProgramFacts(lower_zone, lowerer)" \
    "PlanTargetProjection(target_capability, target_planner)" \
    "EmitProgramArtifact(emit_zone, types, abi_layout, emitter)" \
    "abi_layout: AbiLayoutZone" \
    "target_planner: TargetProjectionPlanner" \
    "ProveSelfHostedParity(parity_zone, oracle)"; do
    require_text "src/self_hosted/compiler/stage_intents.pgy" "$term"
done

for term in \
    "subject StageOwner" \
    ".Consume()"; do
    forbid_text "src/self_hosted/compiler/world.pgy" "$term"
    forbid_text "src/self_hosted/compiler/stage_intents.pgy" "$term"
done

for term in \
    "zone ProgramEmitZone" \
    "zone FunctionEmitZone" \
    "zone StmtEmitZone" \
    "zone ExprRewriteZone" \
    "zone StructValueEmitZone"; do
    forbid_text "src/self_hosted/compiler/world.pgy" "$term"
done

require_text "src/self_hosted/compiler/README.md" "Compiler World"
require_text "src/self_hosted/compiler/README.md" "PgyCompilerWorld"
require_text "src/self_hosted/compiler/README.md" "world.pgy"
require_text "src/self_hosted/compiler/README.md" '`world.pgy` stays under the same 600-line cap'
require_text "src/self_hosted/compiler/README.md" "resource-owned intent cluster"
require_text "src/self_hosted/compiler/README.md" "does this boundary own a distinct resource"
require_text "src/self_hosted/compiler/README.md" "LexerStage"
require_text "src/self_hosted/compiler/README.md" 'There is no generic `StageOwner` alias'
require_text "src/self_hosted/compiler/README.md" "path_manifest_owner.pgy"
forbid_text "src/self_hosted/compiler/README.md" "mirrors the C-side"
forbid_text "src/self_hosted/compiler/README.md" "intentionally empty"
forbid_text "src/self_hosted/compiler/README.md" "??"

require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/world.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/stage_intents.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/target_capability_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/air_evidence_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/artifact_zone_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/test_harness_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/subprocess_runner_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/abi_layout_row_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/symbol_table_owner.pgy"
require_text "src/self_hosted/README.md" "compiler/world.pgy"
require_text "docs/self_hosted/README.md" "11_compiler_world_architecture.md"
require_text "docs/self_hosted/README.md" "12_intent_zone_self_host_architecture.md"
require_text "docs/self_hosted/README.md" "14_target_compiler_world.md"
require_text "docs/self_hosted/README.md" "15_pre_self_host_expansion_ledger.md"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "## Compiler World Rule"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "PgyCompilerWorld"
require_text "docs/self_hosted/10_hard_self_host_contract.md" "No Compiler World exception exists for the 600-line cap"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "resource-owned intent cluster"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "resource ownership boundary"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a module, folder, phase, or helper"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "TypeEnvZone"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "AbiLayoutZone"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "ProgramEmitter"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "object slot c_output: EmittedC"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "projection nerve bundle"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "backend resource cluster"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a semantic zone split"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "LexerStage"
require_text "docs/self_hosted/11_compiler_world_architecture.md" 'generic `StageOwner.Consume()`'
require_text "docs/self_hosted/11_compiler_world_architecture.md" "path_manifest_owner.pgy"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "compiler flow owner"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "stage_intents.pgy"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "Codegen Shape"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "The mental model is a projection nerve bundle"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "helper categories"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "Path And Source Intake"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" "LexerStage"
require_text "docs/self_hosted/12_intent_zone_self_host_architecture.md" 'generic `StageOwner`'
require_text "src/self_hosted/compiler/README.md" "resource ownership boundary"
require_text "src/self_hosted/compiler/README.md" "ProgramEmitter"
require_text "src/self_hosted/codegen/intent.md" "EmissionZone"
require_text "src/self_hosted/codegen/intent.md" "TypeEnvZone"
require_text "src/self_hosted/codegen/intent.md" "AbiLayoutZone"
require_text "src/self_hosted/codegen/intent.md" "ProgramEmitter"
require_text "src/self_hosted/codegen/intent.md" "participants in the emission action graph"
require_text "src/self_hosted/codegen/intent.md" "Projection-nerve rule"
require_text "src/self_hosted/codegen/intent.md" "does not own a second semantic truth"
require_text "src/self_hosted/codegen/intent.md" "does this boundary own a distinct resource"
require_text "src/self_hosted/codegen/intent.md" "recursive participants over the same output/type resources"
require_text "docs/INDEX.md" "self_hosted/11_compiler_world_architecture.md"
require_text "docs/INDEX.md" "self_hosted/12_intent_zone_self_host_architecture.md"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-contract-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-parity-test-smoke"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "PgyCompilerWorld"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Compiler Tree And Projection Nerves"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "tree-like, not bucket-like"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "projection nerve"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "C, LLVM, and self-hosted emission are projections"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Anti-rule: do not split codegen"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Compiler Flow"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Required Substrates"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Codegen Architecture"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Compiler Architecture"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Caching Shape"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Runtime And Materialization"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "Promotion Rule"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "15_pre_self_host_expansion_ledger.md"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "READY"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "ACTIVE"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "HOLD"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "import graph"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "deterministic collections"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "TypeEnvZone"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "AbiLayoutZone"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "EmissionZone"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "ProgramEmitter"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "LexerStage"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" 'A shared `StageOwner` alias would hide'
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "path_manifest_owner.pgy"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "contract-checked against the Pergyra owner"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "no-hidden-runtime"
require_text "docs/self_hosted/README.md" "13_compiler_substrate_architecture.md"
require_text "src/self_hosted/compiler/README.md" "13_compiler_substrate_architecture.md"
require_text "src/self_hosted/codegen/README.md" "13_compiler_substrate_architecture.md"
require_text "docs/INDEX.md" "self_hosted/13_compiler_substrate_architecture.md"
require_text "docs/INDEX.md" "self_hosted/14_target_compiler_world.md"
require_text "docs/self_hosted/14_target_compiler_world.md" "Target Compiler World"
require_text "docs/self_hosted/14_target_compiler_world.md" "PgyCompilerWorld"
require_text "docs/self_hosted/14_target_compiler_world.md" "CompilePergyraProgram"
require_text "docs/self_hosted/14_target_compiler_world.md" "AIR Evidence"
require_text "docs/self_hosted/14_target_compiler_world.md" "MIR Fact"
require_text "docs/self_hosted/14_target_compiler_world.md" "ABI Layout"
require_text "docs/self_hosted/14_target_compiler_world.md" "Codegen Projection Intent"
require_text "docs/self_hosted/14_target_compiler_world.md" "C Emission Zone"
require_text "docs/self_hosted/14_target_compiler_world.md" "LLVM Emission Zone"
require_text "docs/self_hosted/14_target_compiler_world.md" "SelfHosted Emission Zone"
require_text "docs/self_hosted/14_target_compiler_world.md" "Artifact Zone"
require_text "docs/self_hosted/14_target_compiler_world.md" "projection nerve bundle"
require_text "docs/self_hosted/14_target_compiler_world.md" "No hidden materialization"
require_text "docs/self_hosted/14_target_compiler_world.md" "runtime materialization classification"
require_text "docs/self_hosted/14_target_compiler_world.md" "target-capability gate"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Pre-Self-Host Expansion Ledger"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Expansion Import Rule"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Ready Surfaces"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Active Blockers"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Held Surfaces"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Mixed AST-like tree owner"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Stable JSON parse/emit owner"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Subprocess runner"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Symbol/mangle owner"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "ABI/layout row projection"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "AIR evidence zone"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Artifact Zone evidence"
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "no-hidden-fallback"
forbid_text "docs/self_hosted/14_target_compiler_world.md" "??"
forbid_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "??"

pgy_bin="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$pgy_bin" != *.exe ]] && pgy_binary_expects_windows_paths "${pgy_bin}.exe"; then
    pgy_bin="${pgy_bin}.exe"
fi
pgy_bin="$(pgy_path_for_bash_tool "$pgy_bin")"
pgy_require_runnable_binary_here "self-host-compiler-world" "$pgy_bin" ||
    fail "PGY_BIN is not runnable: $pgy_bin"

tmp_dir="$ROOT_DIR/.tmp/self_hosted/compiler_world"
mkdir -p "$tmp_dir"
ast_out="$tmp_dir/world.ast.txt"
(cd "$ROOT_DIR" && "$pgy_bin" --ast \
    "$(pgy_path_for_compiler "$pgy_bin" "$ROOT_DIR/${PGY_SELFHOST_COMPILER_WORLD_PATH}")") >"$ast_out"

grep -Fq "World: PgyCompilerWorld" "$ast_out" ||
    fail "compiler world AST missing PgyCompilerWorld"
grep -Fq "Intent: CompilePergyraProgram" "$ast_out" ||
    fail "compiler world AST missing CompilePergyraProgram intent"
grep -Fq "Intent: LexSource" "$ast_out" ||
    fail "compiler world AST missing LexSource intent"
grep -Fq "Intent: ProveSelfHostedParity" "$ast_out" ||
    fail "compiler world AST missing ProveSelfHostedParity intent"
grep -Fq "Intent: ProveHardSelfHostEvidence" "$ast_out" ||
    fail "compiler world AST missing ProveHardSelfHostEvidence intent"
grep -Fq "Intent: FrontendPipeline" "$ast_out" ||
    fail "compiler world AST missing FrontendPipeline intent"
grep -Fq "Intent: BackendPipeline" "$ast_out" ||
    fail "compiler world AST missing BackendPipeline intent"
grep -Fq "Zone: SelfHostCompiler" "$ast_out" ||
    fail "compiler world AST missing SelfHostCompiler zone"
grep -Fq "Zone: TokenStreamZone" "$ast_out" ||
    fail "compiler world AST missing TokenStreamZone zone"
grep -Fq "Zone: TypeEnvZone" "$ast_out" ||
    fail "compiler world AST missing TypeEnvZone zone"
grep -Fq "Zone: AbiLayoutZone" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutZone zone"
grep -Fq "Zone: TargetCapabilityZone" "$ast_out" ||
    fail "compiler world AST missing TargetCapabilityZone zone"
grep -Fq "Zone: AirEvidenceZone" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceZone zone"
grep -Fq "Zone: SymbolFactTableZone" "$ast_out" ||
    fail "compiler world AST missing SymbolFactTableZone zone"
grep -Fq "Zone: AbiRowProjectionZone" "$ast_out" ||
    fail "compiler world AST missing AbiRowProjectionZone zone"
grep -Fq "Zone: ArtifactZone" "$ast_out" ||
    fail "compiler world AST missing ArtifactZone zone"
grep -Fq "Zone: TestHarnessZone" "$ast_out" ||
    fail "compiler world AST missing TestHarnessZone zone"
grep -Fq "Zone: SubprocessRunnerZone" "$ast_out" ||
    fail "compiler world AST missing SubprocessRunnerZone zone"
grep -Fq "Subject: LexerStage" "$ast_out" ||
    fail "compiler world AST missing LexerStage subject"
grep -Fq "Subject: ParserStage" "$ast_out" ||
    fail "compiler world AST missing ParserStage subject"
grep -Fq "Subject: SemanticStage" "$ast_out" ||
    fail "compiler world AST missing SemanticStage subject"
grep -Fq "Subject: MirLowerStage" "$ast_out" ||
    fail "compiler world AST missing MirLowerStage subject"
grep -Fq "Subject: ProgramEmitter" "$ast_out" ||
    fail "compiler world AST missing ProgramEmitter subject"
grep -Fq "Subject: TargetProjectionPlanner" "$ast_out" ||
    fail "compiler world AST missing TargetProjectionPlanner subject"
grep -Fq "Subject: AirEvidenceOwner" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceOwner subject"
grep -Fq "Subject: SymbolTableOwner" "$ast_out" ||
    fail "compiler world AST missing SymbolTableOwner subject"
grep -Fq "Subject: AbiRowProjector" "$ast_out" ||
    fail "compiler world AST missing AbiRowProjector subject"
grep -Fq "Subject: ArtifactSink" "$ast_out" ||
    fail "compiler world AST missing ArtifactSink subject"
grep -Fq "Subject: TestHarnessRunner" "$ast_out" ||
    fail "compiler world AST missing TestHarnessRunner subject"
grep -Fq "Subject: SubprocessRunner" "$ast_out" ||
    fail "compiler world AST missing SubprocessRunner subject"
grep -Fq "Object: StagePathManifest" "$ast_out" ||
    fail "compiler world AST missing StagePathManifest object"
grep -Fq "Object: TypeEnvironment" "$ast_out" ||
    fail "compiler world AST missing TypeEnvironment object"
grep -Fq "Object: AbiLayoutFacts" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutFacts object"
grep -Fq "Object: TargetCapabilityEnvelope" "$ast_out" ||
    fail "compiler world AST missing TargetCapabilityEnvelope object"
grep -Fq "Object: AirEvidenceFacts" "$ast_out" ||
    fail "compiler world AST missing AirEvidenceFacts object"
grep -Fq "Object: SymbolFactTable" "$ast_out" ||
    fail "compiler world AST missing SymbolFactTable object"
grep -Fq "Object: AbiLayoutRows" "$ast_out" ||
    fail "compiler world AST missing AbiLayoutRows object"
grep -Fq "Object: ArtifactEvidence" "$ast_out" ||
    fail "compiler world AST missing ArtifactEvidence object"
grep -Fq "Object: TestHarnessFacts" "$ast_out" ||
    fail "compiler world AST missing TestHarnessFacts object"
grep -Fq "Object: SubprocessCapabilityEnvelope" "$ast_out" ||
    fail "compiler world AST missing SubprocessCapabilityEnvelope object"

echo "[self-host-compiler-world] compiler world source shape ok"
