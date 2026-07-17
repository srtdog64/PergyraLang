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
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

require_file "src/self_hosted/compiler/README.md"
require_file "src/self_hosted/compiler/world.pgy"
require_file "src/self_hosted/compiler/path_manifest_owner.pgy"
require_file "src/self_hosted/compiler/stage_intents.pgy"
require_file "src/self_hosted/compiler/target_capability_owner.pgy"
require_file "src/self_hosted/compiler/sandbox_capability_owner.pgy"
require_file "src/self_hosted/compiler/sandbox_capability_manifest.pgy"
require_file "src/self_hosted/compiler/compatibility_evolution_owner.pgy"
require_file "src/self_hosted/compiler/air_evidence_owner.pgy"
require_file "src/self_hosted/compiler/artifact_zone_owner.pgy"
require_file "src/self_hosted/compiler/test_harness_owner.pgy"
require_file "src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy"
require_file "src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy"
require_file "src/self_hosted/compiler/test_harness_manifest.pgy"
require_file "src/self_hosted/compiler/subprocess_runner_owner.pgy"
require_file "src/self_hosted/compiler/abi_layout_row_owner.pgy"
require_file "src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy"
require_file "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy"
require_file "src/self_hosted/compiler/abi_layout_row_manifest.pgy"
require_file "src/self_hosted/compiler/symbol_table_owner.pgy"
require_file "src/self_hosted/compiler/stage_artifact_owner.pgy"
require_file "src/self_hosted/compiler/authority_owner.pgy"
require_file "src/self_hosted/compiler/driver_pipeline_owner.pgy"
require_file "src/self_hosted/compiler/driver_bootstrap_main.pgy"
require_file "src/self_hosted/compiler/driver_rung0_owner.pgy"
require_file "src/self_hosted/compiler/driver_rung0_main.pgy"
require_file "src/self_hosted/compiler/driver_cli_owner.pgy"
require_file "src/self_hosted/compiler/driver_rung1_main.pgy"
require_file "docs/self_hosted/11_compiler_world_architecture.md"
require_file "docs/self_hosted/12_intent_zone_self_host_architecture.md"
require_file "docs/self_hosted/13_compiler_substrate_architecture.md"
require_file "docs/self_hosted/14_target_compiler_world.md"
require_file "docs/self_hosted/15_pre_self_host_expansion_ledger.md"
require_file "tests/self_host_compiler_world_contract_smoke.sh"
require_file "tests/self_hosted/compiler_world_manifest.sh"
require_file "tests/self_hosted/parity/driver_rung0_parity.sh"
require_file "tests/self_hosted/parity/driver_rung1_parity.sh"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_PATH_MANIFEST_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_STAGE_ARTIFACT_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_COMPATIBILITY_EVOLUTION_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_SANDBOX_CAPABILITY_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_SANDBOX_CAPABILITY_MANIFEST_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_TEST_HARNESS_MANIFEST_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_AUTHORITY_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_ABI_LAYOUT_ROW_MANIFEST_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_BACKEND_ABI_LAYOUT_CONTRACT_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_ABI_LAYOUT_TARGET_POLICY_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_PIPELINE_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_BOOTSTRAP_MAIN_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_RUNG0_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_RUNG0_MAIN_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_CLI_PATH"
require_text "tests/self_hosted/compiler_world_manifest.sh" "PGY_SELFHOST_COMPILER_DRIVER_RUNG1_MAIN_PATH"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "### Pergyra-Style Self-Host Test"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "C, LLVM, and self-hosted outputs are peer projections over the same facts"
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "stage_artifact_owner.pgy"
require_text "src/self_hosted/compiler/README.md" "## Pergyra-Style Check"
require_text "src/self_hosted/compiler/README.md" 'not as a C folder graph rewritten in `.pgy`'

pgy_compiler_world_require_manifest_paths "$ROOT_DIR" ||
    fail "compiler world path manifest is incomplete"
pgy_compiler_world_require_stage_conformance "$ROOT_DIR" ||
    fail "compiler world stages do not conform to the on-disk stage owners"
require_max_lines "src/self_hosted/compiler/world.pgy" 600
require_max_lines "src/self_hosted/compiler/path_manifest_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/target_capability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/sandbox_capability_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/compatibility_evolution_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/air_evidence_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/artifact_zone_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/test_harness_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/subprocess_runner_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_row_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/abi_layout_row_manifest.pgy" 600
require_max_lines "src/self_hosted/compiler/symbol_table_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/stage_artifact_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/authority_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_pipeline_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_bootstrap_main.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung0_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung0_main.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_cli_owner.pgy" 600
require_max_lines "src/self_hosted/compiler/driver_rung1_main.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_signature_fact_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_signature_contract_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/input/semantic_signature_codegen_view_owner.pgy" 600
require_max_lines "src/self_hosted/semantic/ast_local_binding_fact_owner.pgy" 600
require_max_lines "src/self_hosted/codegen/input/semantic_local_binding_codegen_view_owner.pgy" 600
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" 'import "../parser/program_parse_owner.pgy";'
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" 'import "../semantic/ast_artifact_verdict_owner.pgy";'
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" 'import "../semantic/ast_signature_contract_owner.pgy";'
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" 'import "../codegen/emission/program_entry_owner.pgy";'
forbid_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" 'import "../codegen/emission/program_emit.pgy";'
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompilerDriverPipelineReady"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "ParserAstTreePayloadContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "TypedAstArenaPayloadContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "AstTreeArtifactPayloadContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "SemanticAstArtifactVerdictContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "SemanticAstFunctionSignatureFactsContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "SemanticAstLocalBindingFactsContractReady()"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileSourceToAstArtifact"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "ParseRootProgramArtifact(source_path)"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileAstArtifactToC"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "SemanticAstArtifactAnalyzeCompactBridge(artifact, true)"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "GenerateCFromVerifiedSemanticArtifact("
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileSourceToCArtifact"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "CompileSourceToAstArtifact(source_path)"
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "return CompilerEmissionArtifact("
require_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" '"emitted-c",'
forbid_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileSourceToAst("
forbid_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileAstToC("
forbid_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "func CompileSourceToC("
forbid_text "src/self_hosted/compiler/driver_pipeline_owner.pgy" "let ast_text: String = CompileSourceToAst(source_path)"
require_text "src/self_hosted/compiler/driver_bootstrap_main.pgy" 'import "driver_rung2_owner.pgy";'
require_text "src/self_hosted/compiler/driver_bootstrap_main.pgy" "WriteFile("
require_text "src/self_hosted/compiler/driver_bootstrap_main.pgy" "CompileSourceToCVerified("
require_text "src/self_hosted/compiler/driver_bootstrap_main.pgy" "SelfHostMachineLayerDeclarationEmpty()"
forbid_text "src/self_hosted/compiler/driver_bootstrap_main.pgy" "CompileSourceToCArtifact"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" 'import "driver_pipeline_owner.pgy";'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func CompilerDriverRung0Ready"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "CompilerStagePathManifestReady()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "CompilerAstTreeFactReady()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "CompilerEmissionFactReady()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "CompilerTargetCapabilityEnvelopeReady()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "CompilerDriverPipelineReady()"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func DriverRung0ArtifactMode"
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" '"--emit-ast"'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" '"--emit-c"'
require_text "src/self_hosted/compiler/driver_rung0_owner.pgy" "func RunDriverRung0FromArgs"
require_text "src/self_hosted/compiler/driver_rung0_main.pgy" 'import "driver_rung0_owner.pgy";'
require_text "src/self_hosted/compiler/driver_rung0_main.pgy" "func Main()"
require_text "src/self_hosted/compiler/driver_rung0_main.pgy" "Args()"
require_text "src/self_hosted/compiler/driver_rung0_main.pgy" "RunDriverRung0FromArgs(run_args)"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" 'import "driver_rung0_owner.pgy";'
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "func DriverCliSourcePath"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "func DriverCliArtifactMode"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "func DriverCliOutputPath"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "func DriverCliWriteArtifact"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "func RunDriverRung1FromArgs"
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" 'WriteFile(out_path, Concat(artifact, "\n"))'
require_text "src/self_hosted/compiler/driver_cli_owner.pgy" "DriverCliWriteArtifact(out_path, mode, artifact)"
require_text "src/self_hosted/compiler/driver_rung1_main.pgy" 'import "driver_cli_owner.pgy";'
require_text "src/self_hosted/compiler/driver_rung1_main.pgy" "func Main()"
require_text "src/self_hosted/compiler/driver_rung1_main.pgy" "Args()"
require_text "src/self_hosted/compiler/driver_rung1_main.pgy" "RunDriverRung1FromArgs(run_args)"

# Authority skeleton locks: sensitive-boundary abilities/roles plus the zone
# and intent-step clauses that consume them. These keep the authority axis
# from silently rotting back into who-only provenance.
require_text "src/self_hosted/compiler/world.pgy" 'import "authority_owner.pgy";'
require_text "src/self_hosted/compiler/world.pgy" "authority checker requires FactProving"
require_text "src/self_hosted/compiler/world.pgy" "authority emitter requires ArtifactEmission"
require_text "src/self_hosted/compiler/world.pgy" "authority runner requires SubprocessDiscipline"
require_text "src/self_hosted/compiler/world.pgy" "authority oracle requires ParityJudging"
require_text "src/self_hosted/compiler/world.pgy" "requires: FactProving;"
require_text "src/self_hosted/compiler/world.pgy" "authorized by: checker;"
require_text "src/self_hosted/compiler/world.pgy" "requires: ArtifactEmission;"
require_text "src/self_hosted/compiler/world.pgy" "authorized by: emitter;"
require_text "src/self_hosted/compiler/world.pgy" "requires: SubprocessDiscipline;"
require_text "src/self_hosted/compiler/world.pgy" "authorized by: subprocess_runner;"
require_text "src/self_hosted/compiler/world.pgy" "requires: ParityJudging;"
require_text "src/self_hosted/compiler/world.pgy" "authorized by: oracle;"
require_text "src/self_hosted/compiler/authority_owner.pgy" "ability FactProving"
require_text "src/self_hosted/compiler/authority_owner.pgy" "ability ArtifactEmission"
require_text "src/self_hosted/compiler/authority_owner.pgy" "ability SubprocessDiscipline"
require_text "src/self_hosted/compiler/authority_owner.pgy" "ability ParityJudging"
require_text "src/self_hosted/compiler/authority_owner.pgy" "role SemanticAuthority for SemanticStage"
require_text "src/self_hosted/compiler/authority_owner.pgy" "role EmitterAuthority for ProgramEmitter"
require_text "src/self_hosted/compiler/authority_owner.pgy" "role SubprocessAuthority for SubprocessRunner"
require_text "src/self_hosted/compiler/authority_owner.pgy" "role OracleAuthority for OraclePair"
require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "with caps io_read"
require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "with caps env"

for term in \
    "func CompilerCompatibilityEvolutionSchema" \
    "func CompilerCompatibilitySurfaceCount" \
    "func CompilerCompatibilitySurfaceAt" \
    "func CompilerCompatibilitySurfaceKnown" \
    "func CompilerCompatibilityEvolutionReady" \
    "func CompilerCompatibilityChangeCount" \
    "func CompilerCompatibilityChangeRowAt" \
    "func CompilerCompatibilityChangeRowForSurface" \
    "CompilerCompatibilitySurfaceAt(CompilerCompatibilitySurfaceCount()) == \"\"" \
    "CompilerCompatibilityChangeRowAt(CompilerCompatibilityChangeCount()) == \"\"" \
    "CompilerCompatibilitySurfaceKnown(CompilerSourceCompatibilitySurface())" \
    "CompilerCompatibilityChangeRowForSurface(CompilerStdlibModuleCompatibilitySurface())" \
    "source" \
    "abi_binary" \
    "behavior" \
    "diagnostic" \
    "air_evidence" \
    "mir_json" \
    "runtime_trace" \
    "capability_profile" \
    "stdlib_module" \
    "func CompilerObsoleteMigrationFieldCount" \
    "func CompilerObsoleteMigrationFieldAt" \
    "func CompilerObsoleteMigrationFieldKnown" \
    "func CompilerCompatibilityChangeKindKnown" \
    "CompilerObsoleteMigrationFieldAt(CompilerObsoleteMigrationFieldCount()) == \"\"" \
    "CompilerCompatibilityChangeKindAt(CompilerCompatibilityChangeKindCount()) == \"\"" \
    "CompilerObsoleteMigrationFieldKnown(CompilerObsoleteCodefixStatusField())" \
    "CompilerCompatibilityChangeKindKnown(CompilerCompatibilityRequiredChangeKind())" \
    "diagnostic_id" \
    "replacement" \
    "migration_url" \
    "warning_from" \
    "error_from" \
    "remove_from" \
    "codefix_status" \
    'CompilerCompatibilityDiagnosticId("001")' \
    'CompilerCompatibilityDiagnosticId("002")' \
    'CompilerCompatibilityDiagnosticId("003")' \
    "func CompilerCompatibilityCodefixAvailableStatus" \
    "func CompilerCompatibilityManualMigrationStatus" \
    "func CompilerCompatibilityNoCodefixStatus" \
    "codefix_available" \
    "manual_migration" \
    "no_codefix"; do
    require_text "src/self_hosted/compiler/compatibility_evolution_owner.pgy" "$term"
done

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
    "zone SandboxCapabilityZone" \
    "zone CompatibilityEvolutionZone" \
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
    "step CompatibilityEvolution" \
    "intent ProveSelfHostedParity" \
    "subject SourceUnit" \
    "action Read" \
    "source.Read(paths)" \
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
    "CompilerEmissionFactReady()" \
    "CompilerAbiLayoutRowsReady()" \
    "CompilerSymbolTableReady()" \
    "CompilerTargetCapabilityEnvelopeReady()" \
    "CompilerTokenStreamFactReady()" \
    "CompilerAstTreeFactReady()" \
    "CompilerSemanticVerdictFactReady()" \
    "CompilerMirFactGraphReady()" \
    "emitter.Emit(types, abi_layout, target_capability)" \
    "subject TargetProjectionPlanner" \
    "subject SandboxCapabilityOwner" \
    "action Plan" \
    "subject CompatibilityEvolutionOwner" \
    "subject AirEvidenceOwner" \
    "subject SymbolTableOwner" \
    "subject AbiRowProjector" \
    "subject ArtifactSink" \
    "subject TestHarnessRunner" \
    "subject SubprocessRunner" \
    "subject OraclePair" \
    "CompilerArtifactZoneReady() && CompilerTestHarnessReady()" \
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
    "object SandboxCapabilityFacts" \
    "object CompatibilityEvolutionFacts" \
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
    "object slot sandbox_capability: SandboxCapabilityFacts" \
    "object slot compatibility: CompatibilityEvolutionFacts" \
    "object slot air_evidence: AirEvidenceFacts" \
    "object slot symbols: SymbolFactTable" \
    "object slot abi_rows: AbiLayoutRows" \
    "object slot artifacts: ArtifactEvidence" \
    "object slot harness: TestHarnessFacts" \
    "object slot subprocess: SubprocessCapabilityEnvelope" \
    "object slot layouts: AbiLayoutFacts" \
    "object slot envelope: TargetCapabilityEnvelope" \
    "object slot facts: SandboxCapabilityFacts" \
    "object slot facts: CompatibilityEvolutionFacts" \
    "object slot facts: AirEvidenceFacts" \
    "object slot symbols: SymbolFactTable" \
    "object slot rows: AbiLayoutRows" \
    "object slot evidence: ArtifactEvidence" \
    "zone abi_layout: AbiLayoutZone" \
    "zone target_capability: TargetCapabilityZone" \
    "zone sandbox_capability: SandboxCapabilityZone" \
    "zone compatibility: CompatibilityEvolutionZone" \
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
require_text "src/self_hosted/compiler/world.pgy" 'import "sandbox_capability_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "compatibility_evolution_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "air_evidence_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "artifact_zone_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "test_harness_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "subprocess_runner_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "abi_layout_row_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "symbol_table_owner.pgy"'
require_text "src/self_hosted/compiler/world.pgy" 'import "stage_artifact_owner.pgy"'

for term in \
    "func SelfHostSourceDir" \
    "func SelfHostTestDir" \
    "func SelfHostParityDir" \
    "func CompilerWorldPath" \
    "func CompilerPathManifestPath" \
    "func CompilerStageIntentsPath" \
    "func CompilerTargetCapabilityOwnerPath" \
    "func CompilerSandboxCapabilityOwnerPath" \
    "func CompilerSandboxCapabilityManifestPath" \
    "func CompilerCompatibilityEvolutionOwnerPath" \
    "func CompilerAirEvidenceOwnerPath" \
    "func CompilerArtifactZoneOwnerPath" \
    "func CompilerTestHarnessOwnerPath" \
    "func CompilerTestHarnessManifestPath" \
    "func CompilerSubprocessRunnerOwnerPath" \
    "func CompilerAbiLayoutRowOwnerPath" \
    "func CompilerAbiLayoutTargetPolicyOwnerPath" \
    "func CompilerAbiLayoutRowManifestPath" \
    "func CompilerSymbolTableOwnerPath" \
    "func CompilerStageArtifactOwnerPath" \
    "func CompilerDriverPipelineOwnerPath" \
    "func CompilerDriverBootstrapMainPath" \
    "func CompilerDriverRung0OwnerPath" \
    "func CompilerDriverRung0MainPath" \
    "func CompilerDriverCliOwnerPath" \
    "func CompilerDriverRung1MainPath" \
    "func CompilerDriverRung2OwnerPath" \
    "func CompilerDriverRung2MainPath" \
    "func CompilerDriverRung0ParityPath" \
    "func CompilerDriverRung1ParityPath" \
    "func CompilerDriverRung2ParityPath" \
    "func CompilerOwnerManifestPath" \
    "func CompilerWorldPathProjectionSuiteName" \
    "func CompilerStagePathManifestReady" \
    "func CompilerStagePathAt" \
    "func CompilerStageWorldBindingAt" \
    "func CompilerParityPathAt" \
    "func CompilerWorldManifestPathAt" \
    "func CompilerWorldProjectionPrefixCount" \
    "func CompilerBackendAbiLayoutContractOwnerPath" \
    "func CompilerWorldProjectionPathAt"; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$term"
done
for term in \
    "return CompilerPathManifestPath();" \
    "return CompilerStageIntentsPath();" \
    "return CompilerTargetCapabilityOwnerPath();" \
    "return CompilerSandboxCapabilityOwnerPath();" \
    "return CompilerSandboxCapabilityManifestPath();" \
    "return CompilerCompatibilityEvolutionOwnerPath();" \
    "return CompilerAirEvidenceOwnerPath();" \
    "return CompilerArtifactZoneOwnerPath();" \
    "return CompilerTestHarnessOwnerPath();" \
    "return CompilerTestHarnessManifestPath();" \
    "return CompilerSubprocessRunnerOwnerPath();" \
    "return CompilerAbiLayoutRowOwnerPath();" \
    "return CompilerBackendAbiLayoutContractOwnerPath();" \
    "return CompilerAbiLayoutTargetPolicyOwnerPath();" \
    "return CompilerAbiLayoutRowManifestPath();" \
    "return CompilerSymbolTableOwnerPath();" \
    "return CompilerStageArtifactOwnerPath();" \
    "return CompilerDriverPipelineOwnerPath();" \
    "return CompilerDriverBootstrapMainPath();" \
    "return CompilerDriverRung0OwnerPath();" \
    "return CompilerDriverRung0MainPath();" \
    "return CompilerDriverCliOwnerPath();" \
    "return CompilerDriverRung1MainPath();" \
    "return CompilerDriverRung2OwnerPath();" \
    "return CompilerDriverRung2MainPath();" \
    "return CompilerDriverRung0ParityPath();" \
    "return CompilerDriverRung1ParityPath();" \
    "return CompilerDriverRung2ParityPath();" \
    "return CompilerOwnerManifestPath();" \
    "CompilerWorldProjectionPathCount() != CompilerWorldManifestPathCount() + CompilerWorldProjectionPrefixCount()" \
    "CompilerStagePathAt(CompilerStagePathCount() - 1)" \
    "CompilerStagePathAt(CompilerStagePathCount()) != \"\"" \
    "CompilerParityPathAt(CompilerParityPathCount() - 1)" \
    "CompilerParityPathAt(CompilerParityPathCount()) != \"\"" \
    "CompilerWorldManifestPathAt(CompilerWorldManifestPathCount() - 1)" \
    "CompilerWorldManifestPathAt(CompilerWorldManifestPathCount()) != \"\"" \
    "CompilerWorldProjectionPathAt(CompilerWorldProjectionPrefixCount()) != CompilerWorldManifestPathAt(0)" \
    "CompilerWorldProjectionPathAt(CompilerWorldProjectionPathCount()) != \"\"" \
    "CompilerBackendAbiLayoutContractOwnerPath() == \"\"" \
    "compiler-world-paths" \
    "CompilerStagePathManifestReady" \
    "if index < 24" \
    "CompilerStagePathAt(index - 19)" \
    "if index < 33" \
    "CompilerParityPathAt(index - 24)" \
    "return 42;" \
    "lexer|TokenStreamZone|LexerStage|LexSource|LexerTokenPayloadContractReady" \
    "parser|AstTreeZone|ParserStage|ParseTokens|ParserAstTreePayloadContractReady" \
    "semantic|SemanticVerdictZone|SemanticStage|CheckProgramSemantics|SemanticVerdictPayloadContractReady" \
    "mir_lower|MirFactGraphZone|MirLowerStage|LowerProgramFacts|MirFactGraphPayloadContractReady" \
    "codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact|TypedAstArenaPayloadContractReady"; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$term"
done
forbid_text "src/self_hosted/compiler/path_manifest_owner.pgy" "CompilerParityPathCount() != 9"
forbid_text "src/self_hosted/compiler/path_manifest_owner.pgy" "CompilerWorldManifestPathCount() != 42"
forbid_text "src/self_hosted/compiler/path_manifest_owner.pgy" "CompilerWorldProjectionPathCount() != 45"

for term in \
    "func CompilerStageArtifactRowReady" \
    "func CompilerTokenStreamFactReady" \
    "func CompilerAstTreeFactReady" \
    "func CompilerSemanticVerdictFactReady" \
    "func CompilerMirFactGraphReady" \
    "func CompilerEmissionFactReady" \
    'import "../lexer/token_owner.pgy";' \
    'import "../parser/tree_text_owner.pgy";' \
    'import "../semantic/diagnostic_contract_owner.pgy";' \
    'import "../mir_lower/mir_fact_graph_contract_owner.pgy";' \
    'import "../hir/ast_text_arena_projection_owner.pgy";' \
    "LexerTokenPayloadContractReady()" \
    "ParserAstTreePayloadContractReady()" \
    "SemanticVerdictPayloadContractReady()" \
    "MirFactGraphPayloadContractReady()" \
    "TypedAstArenaPayloadContractReady()" \
    "AstTreeArtifactPayloadContractReady()" \
    "CompilerStagePathAt(index)" \
    "CompilerStageWorldBindingAt(index)" \
    "lexer|TokenStreamZone|LexerStage|LexSource|LexerTokenPayloadContractReady" \
    "parser|AstTreeZone|ParserStage|ParseTokens|ParserAstTreePayloadContractReady" \
    "semantic|SemanticVerdictZone|SemanticStage|CheckProgramSemantics|SemanticVerdictPayloadContractReady" \
    "mir_lower|MirFactGraphZone|MirLowerStage|LowerProgramFacts|MirFactGraphPayloadContractReady" \
    "codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact|TypedAstArenaPayloadContractReady"; do
    require_text "src/self_hosted/compiler/stage_artifact_owner.pgy" "$term"
done

for term in \
    "func LexerTokenPayloadSchema" \
    "pgy.selfhost.lexer-token-stream.v1" \
    "func LexerTokenPayloadFixtureCount" \
    'import "fixture_manifest_owner.pgy";' \
    "return LexerFixtureManifestCount();" \
    "LexerTokenPayloadFixtureCount() != LexerFixtureManifestCount()" \
    "func LexerTokenPayloadKeywordReady" \
    "func LexerTokenPayloadFormatReady" \
    "func LexerTokenPayloadContractReady" \
    "KeywordType(\"projection\") != \"IDENTIFIER\"" \
    "KeywordType(\"impl\") != \"UNKNOWN\"" \
    "TokenLine(1, \"FUNC\", \"func\", 7, 1)"; do
    require_text "src/self_hosted/lexer/token_owner.pgy" "$term"
done

for term in \
    "func ParserAstTreePayloadSchema" \
    "pgy.selfhost.parser-ast-tree.v1" \
    "func ParserAstTreePayloadFixtureCount" \
    'import "fixture_manifest_owner.pgy";' \
    "return ParserFixturePayloadFixtureCount();" \
    "func ParserAstTreePayloadRootReady" \
    "func ParserAstTreePayloadContractReady" \
    "ParserAstTreePayloadFixtureCount() != ParserFixturePayloadFixtureCount()" \
    "AppendImplicitMain(\"\", body)" \
    "StringIndexOf(tree_text, \"  Function: Main\")"; do
    require_text "src/self_hosted/parser/tree_text_owner.pgy" "$term"
done

for term in \
    "func SemanticVerdictPayloadSchema" \
    "pgy.selfhost.semantic.v1" \
    "func SemanticVerdictPayloadFixtureCount" \
    "func SemanticVerdictPayloadFixtureFrontierCount"; do
    require_text "src/self_hosted/semantic/diagnostic_owner.pgy" "$term"
done

for term in \
    "func SemanticVerdictPayloadStatusReady" \
    "func SemanticVerdictPayloadContractReady" \
    "SemanticVerdictPayloadFixtureCount() != SemanticVerdictPayloadFixtureFrontierCount()" \
    "SemanticDiagnosticCodeCount() != 24" \
    "StringIndexOf(ok, \"Status: ok\")" \
    "StringIndexOf(err, \"Code: undefined_symbol\")"; do
    require_text "src/self_hosted/semantic/diagnostic_contract_owner.pgy" "$term"
done

for term in \
    "func MirFactGraphPayloadSchema" \
    "pgy.mir.v1" \
    "func MirFactGraphPayloadFixtureCount" \
    "func MirFactGraphPayloadRootReady" \
    "func MirFactGraphPayloadContractReady" \
    "return MirParityFixtureCount();" \
    "MirParityFixtureCount() != MirFactGraphPayloadFixtureCount()" \
    "MirDocumentSchemaEquals(json, MirFactGraphPayloadSchema())" \
    "MirDeclArrayBounds(json, decls)" \
    "MirRoutineObjectBoundsAt(json, 0, routine)" \
    "MirObjectArrayObjectBoundsAt(json, routine[0], routine[1], \"body\", 0, inst)" \
    "MirObjectStringFactOpt(json, inst[0], inst[1], \"source_type\")"; do
    require_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "$term"
done
forbid_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirFactGraphPayloadFixtureCount() != 95"
forbid_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "MirFactGraphPayloadFixtureCount() != 96"

for term in \
    "func TypedAstArenaPayloadSchema" \
    "pgy.selfhost.typed-ast-arena.v1" \
    "func TypedAstArenaPayloadFixtureCount" \
    "func TypedAstArenaPayloadFixtureFrontierCount" \
    "func TypedAstArenaPayloadContractReady" \
    "return TypedAstArenaPayloadFixtureFrontierCount();" \
    "TypedAstArenaPayloadFixtureCount() != TypedAstArenaPayloadFixtureFrontierCount()" \
    "let arena: AstArena = TypedAstArenaFixture()"; do
    require_text "src/self_hosted/hir/typed_ast_arena_owner.pgy" "$term"
done
forbid_text "src/self_hosted/hir/typed_ast_arena_owner.pgy" "TypedAstArenaPayloadFixtureCount() != 1"

for term in \
    "JsonFieldArrayBounds(json," \
    "JsonArrayObjectBoundsAt(json," \
    "JsonObjectFieldValueBounds(json,"; do
    forbid_text "src/self_hosted/mir_lower/mir_fact_graph_contract_owner.pgy" "$term"
done

for term in \
    "func CompilerAirEvidenceSchema" \
    "func CompilerAirEvidenceFactKnown" \
    "CompilerAirEvidenceEnvelopeReady" \
    "CompilerAirEvidenceFactCount() > 0" \
    "CompilerAirEvidenceFactAt(CompilerAirEvidenceFactCount()) == \"\"" \
    "CompilerAirEvidenceFactKnown(CompilerAirEvidenceIntentGraphFact())" \
    "CompilerAirEvidenceFactKnown(CompilerAirEvidenceLossBudgetFact())" \
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
    "CompilerArtifactKindCount() > 0" \
    "CompilerArtifactKindAt(CompilerArtifactKindCount()) == \"\"" \
    "CompilerArtifactKindKnown(CompilerDiagnosticsArtifactKind())" \
    "diagnostics" \
    "air_json" \
    "mir_json" \
    "ast_text" \
    "CompilerAstTextArtifactKind" \
    "abi_layout" \
    "runtime_call_abi" \
    "compatibility_evolution" \
    "CompilerRuntimeCallAbiArtifactKind" \
    "CompilerCompatibilityEvolutionArtifactKind" \
    "CompilerTargetCapabilityArtifactKind" \
    "CompilerSandboxCapabilityArtifactKind" \
    "CompilerRunGroupPlanArtifactKind" \
    "runtime_materialization" \
    "CompilerRunOutputArtifactKind" \
    "CompilerArtifactKindKnown(CompilerCompatibilityEvolutionArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerEmittedSelfHostedArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerRunOutputArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerAstTextArtifactKind())" \
    "CompilerLspDiagnosticsArtifactKind" \
    "CompilerLspTransportFrameArtifactKind" \
    "CompilerLspTransportStreamArtifactKind" \
    "CompilerLspRequestDispatchArtifactKind" \
    "CompilerLspResponseEmissionArtifactKind" \
    "CompilerLspSessionReplayArtifactKind" \
    "CompilerLspDocumentStoreArtifactKind" \
    "CompilerLspSessionStateArtifactKind" \
    "CompilerLspHoverContentArtifactKind" \
    "CompilerArtifactKindKnown(CompilerLspRequestDispatchArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerLspResponseEmissionArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerLspSessionReplayArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerLspDocumentStoreArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerLspSessionStateArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerLspHoverContentArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerTargetCapabilityArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerSandboxCapabilityArtifactKind())" \
    "CompilerArtifactKindKnown(CompilerRunGroupPlanArtifactKind())" \
    "emitted_self_hosted" \
    "run_output" \
    "run_group_plan"; do
    require_text "src/self_hosted/compiler/artifact_zone_owner.pgy" "$term"
done

for term in \
    "func CompilerTestHarnessSchema" \
    "CompilerTestHarnessReady" \
    "func CompilerHarnessRowIndexKnown" \
    "func CompilerHarnessRowKnown" \
    "func CompilerHarnessProjectionKnown" \
    "CompilerHarnessRowCount() > 0" \
    "CompilerHarnessRowKnown(CompilerHarnessSourcePathRow())" \
    "CompilerHarnessRowKnown(CompilerHarnessProjectionRow())" \
    "CompilerHarnessRowIndexKnown(CompilerHarnessRowCount() - 1)" \
    "!CompilerHarnessRowIndexKnown(CompilerHarnessRowCount())" \
    "source_path" \
    "expected_diagnostic" \
    "expected_air_json" \
    "expected_mir_json" \
    "expected_abi_layout" \
    "CompilerHarnessProjectionCount() > 0" \
    "CompilerHarnessProjectionKnown(CompilerHarnessCOracleProjection())" \
    "CompilerHarnessProjectionKnown(CompilerHarnessSelfHostedProjection())" \
    "!CompilerHarnessProjectionIndexKnown(CompilerHarnessProjectionCount())" \
    "CompilerHarnessComparableArtifactPathCount() > 0" \
    "CompilerHarnessComparableArtifactPathKnown(CompilerHarnessExpectedComparableArtifactPath())" \
    "CompilerHarnessComparableArtifactPathKnown(CompilerHarnessActualComparableArtifactPath())" \
    "CompilerHarnessComparableArtifactPathAt(CompilerHarnessComparableArtifactPathCount()) == \"\"" \
    "c_oracle" \
    "llvm_oracle" \
    "self_hosted"; do
    require_text "src/self_hosted/compiler/test_harness_owner.pgy" "$term"
done

for term in \
    "CompilerHarnessRowCount() == 8" \
    "CompilerHarnessProjectionCount() == 3" \
    "CompilerHarnessComparableArtifactPathCount() == 2"; do
    forbid_text "src/self_hosted/compiler/test_harness_owner.pgy" "$term"
done
for term in \
    "CompilerHarnessRowAt(0) == CompilerHarnessSourcePathRow()" \
    "CompilerHarnessRowAt(CompilerHarnessRowCount() - 1) == CompilerHarnessProjectionRow()" \
    "CompilerHarnessProjectionAt(0) == CompilerHarnessCOracleProjection()" \
    "CompilerHarnessProjectionAt(CompilerHarnessProjectionCount() - 1) == CompilerHarnessSelfHostedProjection()" \
    "CompilerHarnessComparableArtifactPathAt(0) == CompilerHarnessExpectedComparableArtifactPath()" \
    "CompilerHarnessComparableArtifactPathAt(CompilerHarnessComparableArtifactPathCount() - 1) == CompilerHarnessActualComparableArtifactPath()"; do
    forbid_text "src/self_hosted/compiler/test_harness_owner.pgy" "$term"
done

for term in \
    "func CompilerSubprocessSchema" \
    "CompilerSubprocessRunnerReady" \
    "CompilerSubprocessFactCount() > 0" \
    "CompilerSubprocessFactAt(CompilerSubprocessFactCount()) == \"\"" \
    "CompilerSubprocessUseCaseCount() > 0" \
    "CompilerSubprocessUseCaseAt(CompilerSubprocessUseCaseCount()) == \"\"" \
    "CompilerSubprocessExecutablePathFact" \
    "CompilerSubprocessArgvFact" \
    "CompilerSubprocessCwdFact" \
    "CompilerSubprocessEnvAllowlistFact" \
    "CompilerSubprocessTimeoutMsFact" \
    "CompilerSubprocessFactKnown" \
    "executable_path" \
    "argv" \
    "env_allowlist" \
    "timeout_ms" \
    "CompilerSubprocessOracleCompareUseCase" \
    "CompilerSubprocessFixtureBuildUseCase" \
    "CompilerSubprocessArtifactProbeUseCase" \
    "CompilerSubprocessUseCaseKnown" \
    "CompilerSubprocessStreamFact" \
    "CompilerSubprocessExitFact" \
    "oracle_compare" \
    "artifact_probe" \
    "CompilerSubprocessOracleCompareTimeoutMsValue" \
    "CompilerSubprocessOracleCompareTimeoutMs" \
    "CompilerSubprocessOracleCompareEnvAllowlistCount" \
    "CompilerSubprocessOracleCompareEnvAllowlistCount() > 0" \
    "CompilerSubprocessOracleCompareEnvAllowlistAt(CompilerSubprocessOracleCompareEnvAllowlistCount()) == \"\"" \
    "CompilerSubprocessEnvPathName" \
    "CompilerSubprocessEnvPgyBinName" \
    "CompilerSubprocessEnvBackendRunTimeoutName" \
    "CompilerSubprocessEnvSelfHostBuildDirName" \
    "CompilerSubprocessOracleCompareEnvAllowlistAt" \
    "CompilerSubprocessOracleCompareEnvAllowlistKnown" \
    "CompilerSubprocessOracleCompareEnvAllowlist" \
    "CompilerSubprocessOracleComparePlanReady"; do
    require_text "src/self_hosted/compiler/subprocess_runner_owner.pgy" "$term"
done

for term in \
    "func CompilerAbiLayoutRowSchema" \
    "CompilerAbiLayoutRowsReady" \
    "CompilerAbiLayoutRowFactCount() > 0" \
    "CompilerAbiLayoutConcreteRowCount" \
    "CompilerAbiLayoutConcreteRowCount() > 0" \
    "CompilerAbiLayoutRowTypeNameAt(CompilerAbiLayoutConcreteRowCount()) == \"\"" \
    "CompilerAbiLayoutRowIndex" \
    "CompilerAbiLayoutRowCValueTypeFor" \
    "CompilerAbiLayoutRowFieldOrderFor" \
    "CompilerAbiLayoutRowMaterializationFor" \
    "CompilerAbiLayoutRowDefaultReturnValueFor" \
    "CompilerAbiLayoutRowCValueTypeAt" \
    "CompilerAbiLayoutFieldAllowed" \
    "CompilerAbiLayoutRowMaterializationAt" \
    "CompilerAbiLayoutRowDefaultReturnValueAt" \
    "field_order" \
    "tag_kind" \
    "niche" \
    "ownership_shape" \
    "materialization_policy" \
    "default_return_value"; do
    require_text "src/self_hosted/compiler/abi_layout_row_owner.pgy" "$term"
done

for term in \
    'import "target_capability_owner.pgy";' \
    "func CompilerAbiLayoutSelfHostedCAbi" \
    "func CompilerAbiLayoutTargetPolicyCount" \
    "func CompilerAbiLayoutTargetPolicyAbiAt" \
    "func CompilerAbiLayoutTargetPolicyProjectionSetAt" \
    "func CompilerAbiLayoutTargetPolicyRequiredFactsAt" \
    "func CompilerAbiLayoutTargetPolicyFallbacksAt" \
    "func CompilerAbiLayoutTargetPolicyKnown" \
    "func CompilerAbiLayoutTargetPolicyRowKnown" \
    "CompilerAbiLayoutTargetPolicyCount() > 0" \
    "CompilerAbiLayoutTargetPolicyAbiAt(CompilerAbiLayoutTargetPolicyCount()) == \"\"" \
    "CompilerAbiLayoutTargetPolicyProjectionSetAt(CompilerAbiLayoutTargetPolicyCount()) == \"\"" \
    "CompilerAbiLayoutTargetPolicyRequiredFactsAt(CompilerAbiLayoutTargetPolicyCount()) == \"\"" \
    "CompilerAbiLayoutTargetPolicyFallbacksAt(CompilerAbiLayoutTargetPolicyCount()) == \"\"" \
    "CompilerAbiLayoutTargetPolicyRowKnown(" \
    "func CompilerAbiLayoutTargetPolicyReady"; do
    require_text "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy" "$term"
done
for term in \
    "CompilerAbiLayoutTargetPolicyCount() == 1" \
    "CompilerAbiLayoutTargetPolicyAbiAt(0) == CompilerAbiLayoutSelfHostedCAbi()" \
    "CompilerAbiLayoutTargetPolicyProjectionSetAt(0) ==" \
    "CompilerAbiLayoutTargetPolicyRequiredFactsAt(0) ==" \
    "CompilerAbiLayoutTargetPolicyFallbacksAt(0) =="; do
    forbid_text "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy" "$term"
done

for term in \
    "func CompilerSymbolTableSchema" \
    "CompilerSymbolTableReady" \
    "func CompilerSymbolTableRowKnown" \
    "func CompilerSymbolProjectionKnown" \
    "CompilerSymbolTableRowAt(CompilerSymbolTableRowCount()) == \"\"" \
    "CompilerSymbolProjectionAt(CompilerSymbolProjectionCount()) == \"\"" \
    "CompilerSymbolProjectionKnown(CompilerSymbolSelfHostedSymbolRow())" \
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
    "func CompilerTargetCpuCProjection" \
    "func CompilerTargetCpuLlvmProjection" \
    "func CompilerTargetSelfHostedProjection" \
    "return \"cpu-c\"" \
    "return \"cpu-llvm\"" \
    "return \"self-hosted\"" \
    "CompilerTargetProjectionAt(CompilerTargetProjectionCount()) == \"\"" \
    "CompilerTargetProjectionKnown(CompilerTargetCpuCProjection())" \
    "CompilerTargetProjectionKnown(CompilerTargetCpuLlvmProjection())" \
    "CompilerTargetProjectionKnown(CompilerTargetSelfHostedProjection())" \
    "func CompilerTargetFactAt" \
    "CompilerTargetFactAt(CompilerTargetFactCount()) == \"\"" \
    "CompilerTargetFactKnown(CompilerTargetMaterializationReasonFact())" \
    "func CompilerTargetFallbackReasonAt" \
    "CompilerTargetFallbackReasonAt(CompilerTargetFallbackReasonCount()) == \"\"" \
    "CompilerTargetFallbackReasonKnown(CompilerTargetHostOnlySlotBoundaryFallbackReason())" \
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
    "paths: StagePathManifest" \
    "IntakeSource(intake, source, paths)" \
    "LexSource(tokens, lexer)" \
    "ParseTokens(ast, parser)" \
    "CheckProgramSemantics(semantic_zone, checker)" \
    "LowerProgramFacts(lower_zone, lowerer)" \
    "PlanTargetProjection(target_capability, target_planner)" \
    "EmitProgramArtifact(emit_zone, types, abi_layout, target_capability, emitter)" \
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
require_text "src/self_hosted/compiler/README.md" "CompatibilityEvolutionZone"
require_text "src/self_hosted/compiler/README.md" "compatibility_evolution_owner.pgy"
require_text "src/self_hosted/compiler/README.md" "LexerStage"
require_text "src/self_hosted/compiler/README.md" 'There is no generic `StageOwner` alias'
require_text "src/self_hosted/compiler/README.md" "path_manifest_owner.pgy"
forbid_text "src/self_hosted/compiler/README.md" "mirrors the C-side"
forbid_text "src/self_hosted/compiler/README.md" "intentionally empty"
forbid_text "src/self_hosted/compiler/README.md" "??"

require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/world.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/stage_intents.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/target_capability_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/sandbox_capability_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/sandbox_capability_manifest.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/compatibility_evolution_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/air_evidence_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/artifact_zone_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/test_harness_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/test_harness_comparator_paths_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/test_harness_backend_compare_paths_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/subprocess_runner_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/abi_layout_row_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/backend_abi_layout_contract_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/abi_layout_target_policy_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/abi_layout_row_manifest.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/symbol_table_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/stage_artifact_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/driver_rung0_main.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/driver_cli_owner.pgy"
require_text "src/self_hosted/OWNERS.md" "src/self_hosted/compiler/driver_rung1_main.pgy"
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
require_text "docs/self_hosted/11_compiler_world_architecture.md" "CompatibilityEvolutionZone"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "ProgramEmitter"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "object slot c_output: EmittedC"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "projection nerve bundle"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "backend resource cluster"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a semantic zone split"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "LexerStage"
require_text "docs/self_hosted/11_compiler_world_architecture.md" 'generic `StageOwner.Consume()`'
require_text "docs/self_hosted/11_compiler_world_architecture.md" "path_manifest_owner.pgy"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "Stage Binding Visibility"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "CompilerStageWorldBindingAt"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "Pergyra-Likeness Reading"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "not a beauty score"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "topology is load-bearing"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "world actions call named compiler fact owners"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "<stage>|<resource zone>|<stage actor>|<stage intent>|<payload contract>"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "stage|zone|actor|intent|payload_contract"
require_text "tests/self_host_pergyra_likeness_smoke.sh" "COMPILER_WORLD_FACT_CONSUMERS_MIN"
require_text "tests/self_host_pergyra_likeness_smoke.sh" "STAGE_PAYLOAD_CONSUMERS_EXACT"
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
for term in \
    'lexer|TokenStreamZone|LexerStage|LexSource' \
    'parser|AstTreeZone|ParserStage|ParseTokens' \
    'semantic|SemanticVerdictZone|SemanticStage|CheckProgramSemantics' \
    'mir_lower|MirFactGraphZone|MirLowerStage|LowerProgramFacts' \
    'codegen|EmissionZone|ProgramEmitter|EmitProgramArtifact'; do
    require_text "src/self_hosted/compiler/path_manifest_owner.pgy" "$term"
done
require_text "docs/INDEX.md" "self_hosted/11_compiler_world_architecture.md"
require_text "docs/INDEX.md" "self_hosted/12_intent_zone_self_host_architecture.md"
require_text "Makefile" "self-host-compiler-world-contract-test-smoke"
require_text "Makefile" "self-host-preparation-contract-test-smoke"
require_text "Makefile" "self-host-preparation-parity-test-smoke"
require_text "Makefile" "self-host-preparation-impact-test-smoke"
require_text "Makefile" "self-host-preparation-impact-changed-paths-test-smoke"
require_text "Makefile" "tests/self_host_compiler_world_contract_smoke.sh"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-contract-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-parity-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-impact-test-smoke"
require_text "docs/self_hosted/11_compiler_world_architecture.md" "self-host-preparation-impact-changed-paths-test-smoke"
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
require_text "docs/self_hosted/13_compiler_substrate_architecture.md" "compatibility_evolution_owner.pgy"
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
require_text "docs/self_hosted/14_target_compiler_world.md" "compatibility_evolution_owner.pgy"
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
require_text "docs/self_hosted/15_pre_self_host_expansion_ledger.md" "Compatibility evolution envelope"
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
PGY="$pgy_bin"

tmp_dir="$ROOT_DIR/.tmp/self_hosted/compiler_world"
mkdir -p "$tmp_dir"
shell_paths="$tmp_dir/compiler_world_paths.shell.txt"
pgy_paths="$tmp_dir/compiler_world_paths.pgy.txt"
manifest_bin="$tmp_dir/test_harness_manifest.exe"
manifest_compile_log="$tmp_dir/test_harness_manifest.compile.log"
manifest_out="$tmp_dir/test_harness_manifest.out"
manifest_err="$tmp_dir/test_harness_manifest.err"

{
    printf '%s\n' "$PGY_SELFHOST_SOURCE_DIR"
    printf '%s\n' "$PGY_SELFHOST_TEST_DIR"
    printf '%s\n' "$PGY_SELFHOST_PARITY_DIR"
    printf '%s\n' "${PGY_SELFHOST_COMPILER_WORLD_MANIFEST_PATHS[@]}"
} | sort -u >"$shell_paths"

(cd "$ROOT_DIR" && "$pgy_bin" \
    "$(pgy_path_for_compiler "$pgy_bin" "$ROOT_DIR/${PGY_SELFHOST_COMPILER_TEST_HARNESS_MANIFEST_PATH}")" \
    --backend=c \
    -o "$(pgy_path_for_compiler "$pgy_bin" "$manifest_bin")") \
    >"$manifest_compile_log" 2>&1 ||
    { cat "$manifest_compile_log" >&2; fail "compiler world TestHarness manifest failed to build"; }

(cd "$ROOT_DIR" && "$manifest_bin" "compiler-world-paths") \
    >"$manifest_out" 2>"$manifest_err" ||
    { cat "$manifest_out" "$manifest_err" >&2; fail "compiler world TestHarness path projection failed"; }

tr -d '\r' <"$manifest_out" | sort -u >"$pgy_paths"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "self-host-compiler-world:paths" \
    "$tmp_dir" \
    "$shell_paths" \
    "$pgy_paths" \
    "run_output"

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
grep -Fq "Zone: SandboxCapabilityZone" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityZone zone"
grep -Fq "Zone: CompatibilityEvolutionZone" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionZone zone"
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
grep -Fq "Subject: SandboxCapabilityOwner" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityOwner subject"
grep -Fq "Subject: CompatibilityEvolutionOwner" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionOwner subject"
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
grep -Fq "Object: SandboxCapabilityFacts" "$ast_out" ||
    fail "compiler world AST missing SandboxCapabilityFacts object"
grep -Fq "Object: CompatibilityEvolutionFacts" "$ast_out" ||
    fail "compiler world AST missing CompatibilityEvolutionFacts object"
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
