#!/usr/bin/env bash
# Structural ratchet for the reachable MIR execution actions and their one
# protocol/world/publication path.
pgy_selfhost_assert_driver_rung2_execution_action() {
    local root="$1" term
    local main="$root/src/self_hosted/compiler/driver_bootstrap_main.pgy"
    local installed="$root/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
    local protocol="$root/src/self_hosted/compiler/driver_rung2_execution_protocol_owner.pgy"
    local execution="$root/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
    local world="$root/src/self_hosted/compiler/world.pgy"
    local composition="$root/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
    for term in "$main" "$installed" "$protocol" "$execution" "$world" "$composition"; do
        [[ -f "$term" ]] || { echo "[driver-rung2-execution-action] missing owner: ${term#"$root/"}" >&2; return 1; }
    done
    for term in 'DriverRung2DirectMirRequest' 'DriverRung2MirCRequest' \
        'EmitDirectMirThroughPgyCompilerWorld(' 'PublishMirCArtifactThroughPgyCompilerWorld(' \
        'DriverRung2ExecutionOutcomeReadyFor(' 'DriverRung2ExecutionOutcomeDiagnostic('; do
        grep -Fq -- "$term" "$installed" || {
            echo "[driver-rung2-execution-action] installed owner bypasses compiler world: $term" >&2; return 1; }
    done
    for term in 'EmitDirectMirThroughPgyCompilerWorld(' \
        'PublishMirCArtifactThroughPgyCompilerWorld('; do
        [[ "$(grep -F -c -- "$term" "$installed")" -eq 1 ]] || {
            echo "[driver-rung2-execution-action] installed owner must consume once: $term" >&2; return 1; }
    done
    for term in 'CompileMirJsonToDirectBackendVerified' 'CompileMirJsonToCVerified(' \
        'CompileMirJsonToCVerifiedObserved(' 'SelfMirArtifactCommitPayload(' \
        'DriverRung2InstalledCommitMirC' 'WriteFile(' 'import "world.pgy";'; do
        ! grep -Fq -- "$term" "$installed" || {
            echo "[driver-rung2-execution-action] installed implementation bypass returned: $term" >&2; return 1; }
        ! grep -Fq -- "$term" "$main" || {
            echo "[driver-rung2-execution-action] Main implementation bypass returned: $term" >&2; return 1; }
    done
    for term in 'enum DriverRung2DirectMirRequest' 'enum DriverRung2MirCRequest' \
        'tobject DriverRung2ExecutionReceipt' 'tobject DriverRung2ExecutionRejection' \
        'enum DriverRung2ExecutionOutcome' 'DriverRung2ArtifactRejected(SelfMirArtifactFailure)' \
        'func DriverRung2ExecutionOutcomeReadyFor(' 'func DriverRung2ExecutionOutcomeDiagnostic('; do
        grep -Fq -- "$term" "$protocol" || {
            echo "[driver-rung2-execution-action] typed protocol is incomplete: $term" >&2; return 1; }
        ! grep -Fq -- "$term" "$execution" || {
            echo "[driver-rung2-execution-action] protocol authority leaked into execution: $term" >&2; return 1; }
    done
    for term in 'import "driver_rung2_execution_protocol_owner.pgy";' \
        'subject DriverRung2Execution' 'action EmitDirectMir(' 'action PublishMirCArtifact(' \
        'within DriverRung2DirectMirZone' 'authorized by self' \
        'func DriverRung2CommitArtifactForTarget(' 'CompilerTargetProjectionFactFromOwner(' \
        'CompileMirJsonToDirectBackendVerifiedObserved(' 'CompileMirJsonToCVerified(' \
        'CompileMirJsonToCVerifiedObserved(' 'CompilerEmissionArtifactReady(artifact)' \
        'SelfMirArtifactCommitPayload(output_path, artifact.payload)' 'match committed {' \
        'case SelfMirArtifactCommitted(receipt):' 'case SelfMirArtifactRejected(failure):' \
        'public zone DriverRung2DirectMirZone' 'subject slot execution: DriverRung2Execution'; do
        grep -Fq -- "$term" "$execution" || {
            echo "[driver-rung2-execution-action] missing transition: $term" >&2; return 1; }
    done
    [[ "$(grep -Ec -- '^[[:space:]]*action ' "$execution")" -eq 2 ]] || {
        echo "[driver-rung2-execution-action] exactly two MIR actions are required" >&2; return 1; }
    [[ "$(grep -F -c -- 'SelfMirArtifactCommitPayload(output_path, artifact.payload)' "$execution")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] one shared typed commit transition is required" >&2; return 1; }
    for term in 'CompileMirJsonToDirectBackendVerifiedObserved(' 'CompileMirJsonToCVerified(' \
        'CompileMirJsonToCVerifiedObserved('; do
        [[ "$(grep -F -c -- "$term" "$execution")" -eq 1 ]] || {
            echo "[driver-rung2-execution-action] compiler owner call count drifted: $term" >&2; return 1; }
    done
    for term in '.EmitDirectMir(' '.PublishMirCArtifact(' 'DriverRung2DirectMirZone(' \
        'DriverRung2Execution(' 'let compiler_world: PgyCompilerWorld = PgyCompilerWorld('; do
        grep -Fq -- "$term" "$world" "$composition" || {
            echo "[driver-rung2-execution-action] world composition is incomplete: $term" >&2; return 1; }
    done
    if grep -Fq -- 'SelfMirArtifactCommitOutcomeReady(committed)' "$execution" \
        || grep -Eq -- '(^|[^[:alnum:]_])(WriteFile|FileOpen|FileWrite|FileClose)\(' "$execution" \
        || grep -Eq -- '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' "$execution"; then
        echo "[driver-rung2-execution-action] outcome collapse, raw writer, or alternate world returned" >&2; return 1
    fi
    [[ "$(grep -Ec -- '^[[:space:]]*(public[[:space:]]+)?zone[[:space:]]+' "$execution")" -eq 1 ]] || {
        echo "[driver-rung2-execution-action] execution owner must declare one zone" >&2; return 1; }
}
