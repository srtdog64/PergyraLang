#!/usr/bin/env bash
# Owns the reachable direct-MIR action boundary and its no-bypass ratchet.
pgy_selfhost_assert_driver_rung2_execution_action() {
    local root="$1" term
    local main_owner="$root/src/self_hosted/compiler/driver_bootstrap_main.pgy" installed_owner="$root/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
    local execution_owner="$root/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
    local world_owner="$root/src/self_hosted/compiler/world.pgy" composition_owner="$root/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
    for term in "$main_owner" "$installed_owner" "$execution_owner" "$world_owner" "$composition_owner"; do
        [[ -f "$term" ]] || { echo "[driver-rung2-execution-action] missing owner: ${term#"$root/"}" >&2; return 1; }
    done
    for term in 'DriverRung2DirectMirRequest' \
        'EmitDirectMirThroughPgyCompilerWorld(' 'DriverRung2ExecutionOutcomeReadyFor(' \
        'DriverRung2ExecutionOutcomeDiagnostic('; do
        grep -Fq -- "$term" "$installed_owner" || {
            echo "[driver-rung2-execution-action] installed request owner bypasses compiler world composition: $term" >&2; return 1; }
    done
    [[ "$(grep -F -c -- 'EmitDirectMirThroughPgyCompilerWorld(' "$installed_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] installed request owner must consume compiler world composition exactly once" >&2; return 1; }
    for term in 'import "compiler_world_direct_mir_owner.pgy";' \
        'DriverRung2DirectMirRequest' 'EmitDirectMirThroughPgyCompilerWorld(' \
        'DriverRung2ExecutionOutcomeReadyFor(' 'DriverRung2ExecutionOutcomeDiagnostic(' \
        'import "driver_rung2_execution_owner.pgy";' 'import "world.pgy";' \
        '.EmitDirectMir(' 'let compiler_world = PgyCompilerWorld(' \
        'DriverRung2DirectMirZone(' 'DriverRung2Execution(' \
        'import "direct_mir_backend_projection_owner.pgy";' 'CompilerTargetProjectionFactFromOwner(' \
        'CompileMirJsonToDirectBackendVerified('; do
        if grep -Fq -- "$term" "$main_owner"; then
            echo "[driver-rung2-execution-action] Main retained direct owner/world call: $term" >&2; return 1; fi
    done
    for term in 'import "driver_rung2_execution_owner.pgy";' 'import "world.pgy";' \
        '.EmitDirectMir(' 'let compiler_world = PgyCompilerWorld(' \
        'DriverRung2DirectMirZone(' 'DriverRung2Execution(' \
        'import "direct_mir_backend_projection_owner.pgy";' 'CompilerTargetProjectionFactFromOwner(' \
        'CompileMirJsonToDirectBackendVerified('; do
        if grep -Fq -- "$term" "$installed_owner"; then
            echo "[driver-rung2-execution-action] installed request owner retained direct implementation detail: $term" >&2; return 1; fi
    done
    if grep -Fq -- 'WriteFile(' "$main_owner" || grep -Fq -- 'WriteFile(' "$installed_owner"; then
        echo "[driver-rung2-execution-action] CLI composition retained direct-MIR WriteFile" >&2; return 1; fi
    for term in 'enum DriverRung2DirectMirRequest' 'tobject DriverRung2ExecutionReceipt' \
        'tobject DriverRung2ExecutionRejection' \
        'enum DriverRung2ExecutionOutcome' 'func DriverRung2DirectMirRequestProjection(' \
        'DriverRung2Executed(DriverRung2ExecutionReceipt)' 'DriverRung2ArtifactRejected(SelfMirArtifactFailure)' \
        'subject DriverRung2Execution' 'action EmitDirectMir(' \
        'within DriverRung2DirectMirZone' 'authorized by self' \
        'public zone DriverRung2DirectMirZone' 'subject slot execution: DriverRung2Execution' \
        'authority execution' 'CompilerTargetProjectionFactFromOwner(projection_name)' \
        'CompilerTargetProjectionFactReadyFor(' 'CompilerEmissionArtifactReady(artifact)' \
        'import "../mir/artifact_transaction_owner.pgy";' 'SelfMirArtifactCommitPayload(output_path, artifact.payload)' \
        'match committed {' 'case SelfMirArtifactCommitted(receipt):' \
        'case SelfMirArtifactRejected(failure):'; do
        grep -Fq -- "$term" "$execution_owner" || {
            echo "[driver-rung2-execution-action] missing transition: $term" >&2
            return 1
        }
    done
    [[ "$(grep -F -c -- 'CompileMirJsonToDirectBackendVerified(' "$execution_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] backend owner must be consumed exactly once" >&2
        return 1
    }
    [[ "$(grep -Ec -- '^[[:space:]]*action ' "$execution_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] exactly one action is required" >&2
        return 1
    }
    [[ "$(grep -F -c -- 'SelfMirArtifactCommitPayload(output_path, artifact.payload)' "$execution_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] exactly one action-owned commit is required" >&2
        return 1
    }
    if grep -Fq -- 'SelfMirArtifactCommitOutcomeReady(committed)' "$execution_owner" \
        || grep -Fq -- 'execution_identity' "$execution_owner"; then
        echo "[driver-rung2-execution-action] outcome collapsed or leaked authority identity" >&2
        return 1
    fi
    if grep -Eq -- '(SelfMirArtifactBegin|CompilerArtifactWrite|SelfMirArtifactCommit|SelfMirArtifactAbort)\(' "$execution_owner"; then
        echo "[driver-rung2-execution-action] transaction mechanism escaped into action" >&2
        return 1
    fi
    if grep -Eq -- '(^|[^[:alnum:]_])(WriteFile|FileOpen|FileWrite|FileClose)\(' "$execution_owner"; then
        echo "[driver-rung2-execution-action] raw file writer bypass returned" >&2
        return 1
    fi
    [[ "$(grep -Ec -- '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+' "$execution_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] execution owner must declare exactly one zone" >&2
        return 1
    }
    [[ "$(grep -Ec -- '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' "$execution_owner")" -eq 0 ]] || {
        echo "[driver-rung2-execution-action] execution owner must not declare an alternate world" >&2
        return 1
    }
    for term in '../parser/' '../semantic/' 'ParseRootProgramArtifact' 'SemanticAstArtifact' \
        'GenerateCFromVerifiedSemanticArtifact' 'CompileSourceTo' 'CompileMirJsonToCVerified' \
        'Fallback' '"cpu-c"' '"cpu-llvm"' 'import "world.pgy";' \
        'import "stage_intents.pgy";' 'import "authority_owner.pgy";' \
        'world PgyCompilerWorld'; do
        ! grep -Fq -- "$term" "$execution_owner" || {
            echo "[driver-rung2-execution-action] forbidden owner/fallback returned: $term" >&2
            return 1
        }
    done
}
