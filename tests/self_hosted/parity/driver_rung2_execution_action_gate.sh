#!/usr/bin/env bash
# Owns the reachable direct-MIR action boundary and its no-bypass ratchet.

pgy_selfhost_assert_driver_rung2_execution_action() {
    local root="$1"
    local main_owner="$root/src/self_hosted/compiler/driver_bootstrap_main.pgy"
    local execution_owner="$root/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
    local term

    for term in "$main_owner" "$execution_owner"; do
        [[ -f "$term" ]] || {
            echo "[driver-rung2-execution-action] missing owner: ${term#"$root/"}" >&2
            return 1
        }
    done
    for term in 'import "driver_rung2_execution_owner.pgy";' \
        'DriverRung2DirectMirRequest' '.EmitDirectMir(' \
        'DriverRung2ExecutionResultReady(result)'; do
        grep -Fq -- "$term" "$main_owner" || {
            echo "[driver-rung2-execution-action] Main bypasses action: $term" >&2
            return 1
        }
    done
    for term in 'import "direct_mir_backend_projection_owner.pgy";' \
        'CompilerTargetProjectionFactFromOwner(' \
        'CompileMirJsonToDirectBackendVerified('; do
        if grep -Fq -- "$term" "$main_owner"; then
            echo "[driver-rung2-execution-action] Main retained direct call: $term" >&2
            return 1
        fi
    done
    if awk '/if ArrayLength\(args\) == 3 &&/{direct=1} direct && /if ArrayLength\(args\) > 0/{exit} direct{print}' "$main_owner" | grep -Fq -- 'WriteFile('; then
        echo "[driver-rung2-execution-action] Main retained direct-MIR WriteFile" >&2
        return 1
    fi

    for term in 'enum DriverRung2DirectMirRequest' \
        'enum DriverRung2ExecutionStage' 'Requested,' 'TargetAdmitted,' \
        'ArtifactWritten,' 'Rejected,' 'struct DriverRung2ExecutionResult' \
        'subject DriverRung2Execution' 'action EmitDirectMir(' \
        'CompilerTargetProjectionFactFromOwner(projection_name)' \
        'CompilerTargetProjectionFactReadyFor(' \
        'CompilerEmissionArtifactReady(artifact)' \
        'WriteFile(output_path, artifact.payload);'; do
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
    [[ "$(grep -F -c -- 'WriteFile(' "$execution_owner")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] exactly one action-owned write is required" >&2
        return 1
    }
    for term in '../parser/' '../semantic/' 'ParseRootProgramArtifact' \
        'SemanticAstArtifact' 'GenerateCFromVerifiedSemanticArtifact' \
        'CompileSourceTo' 'CompileMirJsonToCVerified' 'Fallback' \
        '"cpu-c"' '"cpu-llvm"'; do
        if grep -Fq -- "$term" "$execution_owner"; then
            echo "[driver-rung2-execution-action] forbidden owner/fallback returned: $term" >&2
            return 1
        fi
    done
}
