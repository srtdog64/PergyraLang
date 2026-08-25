#!/usr/bin/env bash
# Fast recursive-topology contract for the Pergyra self-host compiler world.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WORLD="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
STAGES="$ROOT_DIR/src/self_hosted/compiler/stage_intents.pgy"
EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
SOURCE_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
SOURCE_LLVM_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_source_llvm_intent_execution_owner.pgy"
SOURCE_C_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_execution_owner.pgy"
SOURCE_C_STDOUT="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_stdout_execution_owner.pgy"
COMPOSITION="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
ROOT_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/compiler_root_intent_execution_owner.pgy"
MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
INSTALLED="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
READ_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
ARTIFACT_EXECUTION="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_artifact_request_execution_owner.pgy"
COMPILER_ROOT="$ROOT_DIR/src/self_hosted/compiler"
ARCH="$ROOT_DIR/docs/self_hosted/11_compiler_world_architecture.md"

fail() {
    echo "[self-host-compiler-topology] $*" >&2
    exit 1
}

[[ -f "$WORLD" ]] || fail "missing compiler world"
[[ -f "$STAGES" ]] || fail "missing stage intent owner"
[[ -f "$EXECUTION" ]] || fail "missing direct-MIR execution owner"
[[ -f "$SOURCE_EXECUTION" ]] || fail "missing source-to-MIR execution owner"
[[ -f "$SOURCE_LLVM_EXECUTION" ]] || fail "missing source-to-LLVM intent execution owner"
[[ -f "$SOURCE_C_EXECUTION" ]] || fail "missing source-to-C execution owner"
[[ -f "$SOURCE_C_STDOUT" ]] || fail "missing source-to-C stdout execution owner"
[[ -f "$COMPOSITION" ]] || fail "missing direct-MIR world composition owner"
[[ -f "$ROOT_EXECUTION" ]] || fail "missing compiler-purpose root execution owner"
[[ -f "$MAIN" ]] || fail "missing production compiler entrypoint"
[[ -f "$INSTALLED" ]] || fail "missing installed CLI request executor"
[[ -f "$READ_EXECUTION" ]] || fail "missing read-only CLI request executor"
[[ -f "$ARTIFACT_EXECUTION" ]] || fail "missing installed artifact request executor"
[[ -f "$ARCH" ]] || fail "missing compiler world architecture"

awk '
    /^(public[[:space:]]+)?zone[[:space:]]+[A-Za-z0-9_]+[[:space:]]*\{/ {
        zones++
        if ($0 ~ /^(public[[:space:]]+)?zone[[:space:]]+DriverRung2DirectMirZone[[:space:]]*\{/) {
            direct_zone++
        }
        if ($0 ~ /^(public[[:space:]]+)?zone[[:space:]]+DriverSourceMirZone[[:space:]]*\{/) {
            source_zone++
        }
        if ($0 ~ /^(public[[:space:]]+)?zone[[:space:]]+DriverSourceLlvmIntentZone[[:space:]]*\{/) {
            source_llvm_zone++
        }
        if ($0 ~ /^(public[[:space:]]+)?zone[[:space:]]+DriverSourceCZone[[:space:]]*\{/) {
            source_c_zone++
        }
    }
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
        worlds++
        in_world = 1
        next
    }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
        if (members == 1 &&
            $0 ~ /^[[:space:]]+zone[[:space:]]+direct_mir:[[:space:]]+DriverRung2DirectMirZone[[:space:]]*$/) {
            direct_member_is_first++
        }
        if ($0 ~ /^[[:space:]]+zone[[:space:]]+direct_mir:[[:space:]]+DriverRung2DirectMirZone[[:space:]]*$/) {
            direct_member++
        }
        if (members == 2 &&
            $0 ~ /^[[:space:]]+zone[[:space:]]+source_mir:[[:space:]]+DriverSourceMirZone[[:space:]]*$/) {
            source_member_is_second++
        }
        if ($0 ~ /^[[:space:]]+zone[[:space:]]+source_mir:[[:space:]]+DriverSourceMirZone[[:space:]]*$/) {
            source_member++
        }
        if (members == 3 &&
            $0 ~ /^[[:space:]]+zone[[:space:]]+source_llvm:[[:space:]]+DriverSourceLlvmIntentZone[[:space:]]*$/) {
            source_llvm_member_is_third++
        }
        if ($0 ~ /^[[:space:]]+zone[[:space:]]+source_llvm:[[:space:]]+DriverSourceLlvmIntentZone[[:space:]]*$/) {
            source_llvm_member++
        }
        if (members == 4 &&
            $0 ~ /^[[:space:]]+zone[[:space:]]+source_c:[[:space:]]+DriverSourceCZone[[:space:]]*$/) {
            source_c_member_is_fourth++
        }
        if ($0 ~ /^[[:space:]]+zone[[:space:]]+source_c:[[:space:]]+DriverSourceCZone[[:space:]]*$/) {
            source_c_member++
        }
    }
    in_world && /^}/ { in_world = 0 }
    /intent[[:space:]]+CompilePergyraProgram[[:space:]]*\(/ { compile_intent++ }
    /step[[:space:]]+Compile[[:space:]]*\{/ { compile_step++ }
    /step[[:space:]]+(Frontend|MiddleEnd|Evidence|Backend|SelfProof)[[:space:]]*\{/ {
        readiness_step++
    }
    /SelfHostCompiler/ { duplicate_aggregate++ }
    END {
        if (zones != 22 || worlds != 1 || members != 4 ||
            direct_zone != 1 || source_zone != 1 || source_llvm_zone != 1 ||
            source_c_zone != 1 || direct_member != 1 || source_member != 1 ||
            source_llvm_member != 1 || source_c_member != 1 ||
            direct_member_is_first != 1 || source_member_is_second != 1 ||
            source_llvm_member_is_third != 1 || source_c_member_is_fourth != 1 ||
            compile_intent != 1 || compile_step != 1 || readiness_step != 0 ||
            duplicate_aggregate != 0) {
            exit 1
        }
    }
' "$WORLD" "$EXECUTION" "$SOURCE_EXECUTION" "$SOURCE_LLVM_EXECUTION" \
    "$SOURCE_C_EXECUTION" || fail "world/resource topology drifted"

first_world_member="$(awk '
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ {
        in_world = 1
        next
    }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        print $2 "|" $3
        exit
    }
' "$WORLD")"
[[ "$first_world_member" == \
    'direct_mir:|DriverRung2DirectMirZone' ]] ||
    fail "direct_mir must remain PgyCompilerWorld's first positional field"

second_world_member="$(awk '
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ { in_world = 1; next }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
        if (members == 2) { print $2 "|" $3; exit }
    }
' "$WORLD")"
[[ "$second_world_member" == 'source_mir:|DriverSourceMirZone' ]] ||
    fail "source_mir must be PgyCompilerWorld's second positional field"

third_world_member="$(awk '
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ { in_world = 1; next }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
        if (members == 3) { print $2 "|" $3; exit }
    }
' "$WORLD")"
[[ "$third_world_member" == 'source_llvm:|DriverSourceLlvmIntentZone' ]] ||
    fail "source_llvm must be PgyCompilerWorld's third positional field"

fourth_world_member="$(awk '
    /^world[[:space:]]+PgyCompilerWorld[[:space:]]*\{/ { in_world = 1; next }
    in_world && /^[[:space:]]+zone[[:space:]]+[A-Za-z0-9_]+:/ {
        members++
        if (members == 4) { print $2 "|" $3; exit }
    }
' "$WORLD")"
[[ "$fourth_world_member" == 'source_c:|DriverSourceCZone' ]] ||
    fail "source_c must be PgyCompilerWorld's fourth positional field"

compiler_world_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$compiler_world_count" -eq 1 ]] ||
    fail "compiler graph must declare exactly one world"

for owner_term in \
    "$EXECUTION|public zone DriverRung2DirectMirZone" \
    "$EXECUTION|subject slot execution: DriverRung2Execution" \
    "$EXECUTION|authority execution" \
    "$SOURCE_EXECUTION|public zone DriverSourceMirZone" \
    "$SOURCE_EXECUTION|subject slot execution: DriverSourceMirExecution" \
    "$SOURCE_EXECUTION|authority execution" \
    "$SOURCE_LLVM_EXECUTION|public zone DriverSourceLlvmIntentZone" \
    "$SOURCE_LLVM_EXECUTION|action Compile(" \
    "$SOURCE_LLVM_EXECUTION|within DriverSourceLlvmIntentZone" \
    "$SOURCE_LLVM_EXECUTION|authorized by self" \
    "$SOURCE_LLVM_EXECUTION|subject slot execution: DriverSourceLlvmIntentExecution" \
    "$SOURCE_LLVM_EXECUTION|authority execution" \
    "$SOURCE_C_EXECUTION|public zone DriverSourceCZone" \
    "$SOURCE_C_EXECUTION|action ProduceSourceC(" \
    "$SOURCE_C_EXECUTION|subject slot execution: DriverSourceCExecution" \
    "$SOURCE_C_EXECUTION|authority execution" \
    "$WORLD|zone direct_mir: DriverRung2DirectMirZone" \
    "$WORLD|zone source_mir: DriverSourceMirZone" \
    "$WORLD|zone source_llvm: DriverSourceLlvmIntentZone" \
    "$WORLD|zone source_c: DriverSourceCZone" \
    "$WORLD|return self.direct_mir.execution.EmitDirectMir(" \
    "$WORLD|return self.direct_mir.execution.PublishMirCArtifact(" \
    "$WORLD|return self.source_mir.execution.ProduceSourceMir(" \
    "$WORLD|return self.source_mir.execution.PublishSourceMirArtifact(" \
    "$WORLD|let completed: Bool = CompilePergyraProgram(" \
    "$WORLD|return self.source_c.execution.ProduceSourceC(" \
    "$WORLD|return self.source_c.execution.PublishSourceCArtifact(" \
    "$COMPOSITION|import \"world.pgy\";" \
    "$COMPOSITION|func PgyCompilerWorldMaterializeExecutableZones()" \
    "$COMPOSITION|func EmitDirectMirThroughPgyCompilerWorld(" \
    "$COMPOSITION|func PublishMirCArtifactThroughPgyCompilerWorld(" \
    "$COMPOSITION|func ProduceSourceMirThroughPgyCompilerWorld(" \
    "$COMPOSITION|func PublishSourceMirArtifactThroughPgyCompilerWorld(" \
    "$COMPOSITION|func ProduceSourceCThroughPgyCompilerWorld(" \
    "$COMPOSITION|func PublishSourceCArtifactThroughPgyCompilerWorld(" \
    "$COMPOSITION|DriverRung2DirectMirZone(" \
    "$COMPOSITION|DriverSourceMirZone(" \
    "$COMPOSITION|DriverSourceLlvmIntentZone(" \
    "$COMPOSITION|DriverSourceCZone(" \
    "$COMPOSITION|return compiler_world.EmitDirectMir(" \
    "$COMPOSITION|return compiler_world.PublishMirCArtifact(" \
    "$COMPOSITION|return compiler_world.ProduceSourceMir(" \
    "$COMPOSITION|return compiler_world.PublishSourceMirArtifact(" \
    "$COMPOSITION|return compiler_world.ProduceSourceC(" \
    "$COMPOSITION|return compiler_world.PublishSourceCArtifact(" \
    "$SOURCE_C_STDOUT|ProduceSourceCThroughPgyCompilerWorld(" \
    "$SOURCE_C_STDOUT|DriverSourceCPayloadAdmissionReadyFor(" \
    "$MAIN|import \"driver_rung2_installed_cli_owner.pgy\";" \
    "$MAIN|DriverRung2ExecuteInstalledRequest(request);" \
    "$INSTALLED|case DriverCliSourceCStdout(source_path):" \
    "$INSTALLED|case DriverCliSourceCManifestStdout(source_path, manifest_path):" \
    "$READ_EXECUTION|case DriverCliSourceCStdout(source_path):" \
    "$READ_EXECUTION|case DriverCliSourceCManifestStdout(source_path, manifest_path):" \
    "$INSTALLED|case DriverCliSourceLlvmArtifact(source_path, output_path):" \
    "$ARTIFACT_EXECUTION|EmitDirectMirThroughPgyCompilerWorld(" \
    "$ARTIFACT_EXECUTION|PublishMirCArtifactThroughPgyCompilerWorld(" \
    "$ARTIFACT_EXECUTION|PublishSourceMirArtifactThroughPgyCompilerWorld(" \
    "$ARTIFACT_EXECUTION|PublishSourceCArtifactThroughPgyCompilerWorld(" \
    "$ARTIFACT_EXECUTION|CompileSourceToLlvmThroughPgyCompilerWorld(" \
    "$ROOT_EXECUTION|func CompileSourceToLlvmThroughPgyCompilerWorld("; do
    owner="${owner_term%%|*}"
    term="${owner_term#*|}"
    [[ "$(grep -F -c -- "$term" "$owner")" -eq 1 ]] ||
        fail "direct-MIR topology fact must occur exactly once: $term"
done

[[ "$(grep -F -c -- 'within DriverRung2DirectMirZone' "$EXECUTION")" -eq 2 ]] ||
    fail "direct-MIR subject must expose exactly two zone-bound artifact actions"
[[ "$(grep -F -c -- 'authorized by self' "$EXECUTION")" -eq 2 ]] ||
    fail "both direct-MIR artifact actions must retain subject authority"
[[ "$(grep -F -c -- 'within DriverSourceMirZone' "$SOURCE_EXECUTION")" -eq 2 ]] ||
    fail "source-MIR subject must expose exactly two zone-bound publication actions"
[[ "$(grep -F -c -- 'authorized by self' "$SOURCE_EXECUTION")" -eq 2 ]] ||
    fail "both source-MIR publication actions must retain subject authority"
[[ "$(grep -F -c -- 'within DriverSourceCZone' "$SOURCE_C_EXECUTION")" -eq 2 ]] ||
    fail "source-C subject must expose payload and artifact actions in one zone"
[[ "$(grep -F -c -- 'authorized by self' "$SOURCE_C_EXECUTION")" -eq 2 ]] ||
    fail "both source-C actions must retain subject authority"
[[ "$(grep -F -c -- 'DriverRung2CliLogSourceCPayloadOrDie(' "$READ_EXECUTION")" -eq 2 ]] ||
    fail "both installed source-C stdout requests must reach one checked consumer"
for installed_case in \
    'case DriverCliSourceCStdout(source_path):' \
    'case DriverCliSourceCManifestStdout(source_path, manifest_path):'; do
    awk -v header="$installed_case" '
        index($0, header) { active = 1; next }
        active && /DriverRung2ExecuteReadRequest\(request\);/ { found = 1; exit }
        active && /case DriverCli/ { exit }
        END { exit found ? 0 : 1 }
    ' "$INSTALLED" || fail "installed source-C case lost read delegation: $installed_case"
done

if grep -Fq -- 'import "driver_rung2_execution_owner.pgy";' "$MAIN" ||
    grep -Fq -- 'import "driver_source_mir_execution_owner.pgy";' "$MAIN" ||
    grep -Fq -- 'import "driver_source_llvm_intent_execution_owner.pgy";' "$MAIN" ||
    grep -Fq -- 'import "driver_source_c_execution_owner.pgy";' "$MAIN" ||
    grep -Fq -- 'import "world.pgy";' "$MAIN" ||
    grep -Fq -- '.EmitDirectMir(' "$MAIN" ||
    grep -Fq -- '.PublishMirCArtifact(' "$MAIN" ||
    grep -Fq -- '.ProduceSourceMir(' "$MAIN" ||
    grep -Fq -- '.ProduceSourceC(' "$MAIN" ||
    grep -Fq -- 'CompilePergyraProgram(' "$MAIN" ||
    grep -Fq -- '.PublishSourceMirArtifact(' "$MAIN" ||
    grep -Fq -- '.PublishSourceCArtifact(' "$MAIN"; then
    fail "production Main bypassed the single compiler-world composition path"
fi

direct_zone_decl_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+DriverRung2DirectMirZone[[:space:]]*\{' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$direct_zone_decl_count" -eq 1 ]] ||
    fail "DriverRung2DirectMirZone must have exactly one declaration"
[[ "$(grep -F -c -- 'DriverRung2DirectMirZone' "$WORLD")" -eq 1 ]] ||
    fail "PgyCompilerWorld must bind DriverRung2DirectMirZone exactly once"

source_zone_decl_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+DriverSourceMirZone[[:space:]]*\{' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$source_zone_decl_count" -eq 1 ]] ||
    fail "DriverSourceMirZone must have exactly one declaration"
[[ "$(grep -F -c -- 'DriverSourceMirZone' "$WORLD")" -eq 1 ]] ||
    fail "PgyCompilerWorld must bind DriverSourceMirZone exactly once"

source_llvm_zone_decl_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+DriverSourceLlvmIntentZone[[:space:]]*\{' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$source_llvm_zone_decl_count" -eq 1 ]] ||
    fail "DriverSourceLlvmIntentZone must have exactly one declaration"
[[ "$(grep -F -c -- 'zone source_llvm: DriverSourceLlvmIntentZone' "$WORLD")" -eq 1 ]] ||
    fail "PgyCompilerWorld must bind DriverSourceLlvmIntentZone exactly once"

source_c_zone_decl_count="$(
    {
        grep -REh --include='*.pgy' \
            '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+DriverSourceCZone[[:space:]]*\{' \
            "$COMPILER_ROOT" || true
    } | wc -l | tr -d '[:space:]'
)"
[[ "$source_c_zone_decl_count" -eq 1 ]] ||
    fail "DriverSourceCZone must have exactly one declaration"
[[ "$(grep -F -c -- 'DriverSourceCZone' "$WORLD")" -eq 1 ]] ||
    fail "PgyCompilerWorld must bind DriverSourceCZone exactly once"

awk '
    /^intent[[:space:]]+FrontendPipeline[[:space:]]*\(/ { frontend++ }
    /^intent[[:space:]]+MiddleEndPipeline[[:space:]]*\(/ { middle_end++ }
    /^intent[[:space:]]+BackendPipeline[[:space:]]*\(/ { backend++ }
    /^intent[[:space:]]+SelfProofPipeline[[:space:]]*\(/ { self_proof++ }
    END {
        if (frontend != 1 || middle_end != 1 || backend != 1 ||
            self_proof != 1) {
            exit 1
        }
    }
' "$STAGES" || fail "derived stage-intent topology drifted"

awk '
    /^Status: `hard-self-host-shape-contract \/ bounded source-LLVM intent SUBSTITUTING`$/ { status++ }
    /## Recursive Topology Rule/ { rule++ }
    /## Target Resource Facade/ { facade++ }
    /resource zone -> intent transition -> typed owner fact -> final consumer/ {
        chain++
    }
    /carriage decision remains positional by default/ { positional++ }
    /not the literal golden ratio/ { balance++ }
    /^\| `FrontendResources\.Intake` \| `SourceIntakeZone` \|$/ { intake++ }
    /^\| `FrontendResources\.Token` \| `TokenStreamZone` \|$/ { token++ }
    /^\| `FrontendResources\.AST` \| `AstTreeZone` \|$/ { ast++ }
    /^\| `MiddleEndResources\.Semantic` \| `SemanticVerdictZone`, `TypeEnvZone` \|$/ { semantic++ }
    /^\| `MiddleEndResources\.MIR` \| `MirFactGraphZone` \|$/ { mir++ }
    /^\| `EvidenceResources\.AIR` \| `AirEvidenceZone` \|$/ { air++ }
    /^\| `EvidenceResources\.Compatibility` \| `CompatibilityEvolutionZone` \|$/ { compatibility++ }
    /^\| `EvidenceResources\.ABI` \| `AbiLayoutZone`, `AbiRowProjectionZone` \|$/ { abi++ }
    /^\| `BackendResources\.Target` \| `TargetCapabilityZone`, `SandboxCapabilityZone` \|$/ { target++ }
    /^\| `BackendResources\.Emission` \| `SymbolFactTableZone`, `EmissionZone` \|$/ { emission++ }
    /^\| `BackendResources\.Artifact` \| `ArtifactZone`, `TestHarnessZone`, `SubprocessRunnerZone`, `ParityZone` \|$/ { artifact++ }
    /^\| `BackendResources\.DirectMIR` \| `DriverRung2DirectMirZone` \|$/ { direct_mir++ }
    /^\| `BackendResources\.SourceMIR` \| `DriverSourceMirZone` \|$/ { source_mir++ }
    /^\| `BackendResources\.SourceLLVM` \| `DriverSourceLlvmIntentZone` \|$/ { source_llvm++ }
    /^\| `BackendResources\.SourceC` \| `DriverSourceCZone` \|$/ { source_c++ }
    /driver_bootstrap_main\.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishDirectMir -> EmitDirectMirThroughPgyCompilerWorld/ { reachable_path++ }
    /driver_bootstrap_main\.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishSourceMir -> PublishSourceMirArtifactThroughPgyCompilerWorld/ { reachable_source_path++ }
    /driver_bootstrap_main\.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishSourceC -> PublishSourceCArtifactThroughPgyCompilerWorld/ { reachable_source_c_path++ }
    /driver_bootstrap_main\.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2ExecuteReadRequest -> DriverRung2CliLogSourceCPayloadOrDie -> ProduceSourceCThroughPgyCompilerWorld/ { reachable_source_c_stdout_path++ }
    /driver_bootstrap_main\.Main -> DriverRung2ExecuteInstalledRequest -> DriverRung2InstalledPublishSourceLlvm -> CompileSourceToLlvmThroughPgyCompilerWorld/ { reachable_source_llvm_path++ }
    /exactly 22 concrete resource zones and four executable world members/ { exact_topology++ }
    /target facade projection, not a claim that four new aggregate zones/ {
        projection++
    }
    /It must not be emitted as a nested value bundle/ { no_bundle++ }
    END {
        if (status != 1 || rule != 1 || facade != 1 || chain != 1 || positional != 1 ||
            balance != 1 || intake != 1 || token != 1 || ast != 1 ||
            semantic != 1 || mir != 1 || air != 1 || compatibility != 1 ||
            abi != 1 || target != 1 || emission != 1 || artifact != 1 ||
            direct_mir != 1 || source_mir != 1 || source_llvm != 1 ||
            source_c != 1 ||
            reachable_path != 1 || reachable_source_path != 1 ||
            reachable_source_c_path != 1 ||
            reachable_source_c_stdout_path != 1 ||
            reachable_source_llvm_path != 1 ||
            exact_topology != 1 ||
            projection != 1 || no_bundle != 1) {
            exit 1
        }
    }
' "$ARCH" || fail "recursive topology contract documentation drifted"

echo "[self-host-compiler-topology] recursive owner topology ok"
