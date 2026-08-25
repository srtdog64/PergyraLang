#!/usr/bin/env bash
# Sourced by installed_driver_cli_mode_owner.sh after its default MIR-C stdout,
# artifact, and pressure-observed evidence are ready.

MIR_C_PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_mir_c_protocol_owner.pgy"
MIR_C_PAYLOAD_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_mir_c_payload_execution_owner.pgy"
MIR_C_EXECUTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_execution_owner.pgy"
MIR_C_READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
MIR_C_STDOUT_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_mir_c_stdout_execution_owner.pgy"
MIR_C_ARTIFACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_artifact_request_execution_owner.pgy"
MIR_C_INSTALLED_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
MIR_C_WORLD_OWNER="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
MIR_C_COMPOSITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"

require_mir_c_text() {
    grep -Fq -- "$2" "$1" ||
        fail "missing ${1#"$ROOT_DIR/"}: $2"
}

for owner in "$MIR_C_PROTOCOL_OWNER" "$MIR_C_PAYLOAD_OWNER" \
    "$MIR_C_EXECUTION_OWNER" "$MIR_C_READ_OWNER" "$MIR_C_STDOUT_OWNER" \
    "$MIR_C_ARTIFACT_OWNER" "$MIR_C_INSTALLED_OWNER" "$MIR_C_WORLD_OWNER" \
    "$MIR_C_COMPOSITION_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
for term in 'enum DriverRung2MirCMachineRequest' 'MirCDefaultMachine' \
    'MirCManifestVerified(SelfHostMachineLayerDeclaration)' \
    'tobject DriverRung2MirCPayloadReceipt' \
    'target_projection: CompilerTargetProjectionFact;' \
    'artifact: CompilerEmissionArtifact;' \
    'machine_manifest_id: String;' 'machine_manifest_fingerprint: String;' \
    'enum DriverRung2MirCPayloadAdmission' \
    'func DriverRung2MirCObservationMode(' \
    'func DriverRung2MirCMachineDeclaration(' \
    'func DriverRung2MirCManifestId(' \
    'func DriverRung2MirCManifestFingerprint(' \
    'func DriverRung2MirCPayloadAdmissionReadyFor(' \
    'func DriverRung2MirCPayloadAdmissionDiagnostic(' \
    'DriverRung2EmissionArtifactReadyForTarget('; do
    require_mir_c_text "$MIR_C_PROTOCOL_OWNER" "$term"
done
for term in 'func DriverRung2MirCProducePayloadAdmitted(' \
    'DriverRung2MirCRequestDiagnostic(' \
    'MIR C machine declaration is invalid' \
    'CompilerTargetProjectionFactFromOwner(projection_name)' \
    'DriverRung2CompileMirCForRequest(' \
    'DriverRung2MirCMachineDeclaration(machine_request)' \
    'DriverRung2EmissionArtifactReadyForTarget(' \
    'DriverRung2MirCPayloadReceipt('; do
    require_mir_c_text "$MIR_C_PAYLOAD_OWNER" "$term"
done
for term in 'action ProduceMirC(' 'action PublishMirCArtifact(' \
    'DriverRung2MirCProducePayloadAdmitted(' \
    'DriverRung2MirCPayloadReceiptReadyFor(' \
    'receipt.artifact, receipt.target_projection, output_path'; do
    require_mir_c_text "$MIR_C_EXECUTION_OWNER" "$term"
done
[[ "$(grep -Fc 'DriverRung2MirCProducePayloadAdmitted(' \
    "$MIR_C_EXECUTION_OWNER")" == "2" ]] ||
    fail "MIR-C stdout and artifact actions must share one payload producer"
mir_c_diagnostic_line="$(grep -n \
    'let diagnostic: String = DriverRung2MirCRequestDiagnostic(' \
    "$MIR_C_PAYLOAD_OWNER" | cut -d: -f1)"
mir_c_target_line="$(grep -n \
    'let target_projection: CompilerTargetProjectionFact =' \
    "$MIR_C_PAYLOAD_OWNER" | cut -d: -f1)"
mir_c_compile_line="$(grep -n \
    'let artifact: CompilerEmissionArtifact =' \
    "$MIR_C_PAYLOAD_OWNER" | cut -d: -f1)"
mir_c_admitted_line="$(grep -n \
    'return DriverRung2MirCPayloadAdmitted(' \
    "$MIR_C_PAYLOAD_OWNER" | cut -d: -f1)"
[[ "$mir_c_diagnostic_line" -lt "$mir_c_target_line" && \
    "$mir_c_target_line" -lt "$mir_c_compile_line" && \
    "$mir_c_compile_line" -lt "$mir_c_admitted_line" ]] ||
    fail "MIR-C request, target, compiler, and admission order drifted"
for term in 'CompileMirJsonToCVerified(' \
    'CompileMirJsonToCVerifiedObserved('; do
    [[ "$(grep -Fc "$term" "$MIR_C_PAYLOAD_OWNER")" == "1" ]] ||
        fail "MIR-C payload owner compiler dispatch drifted: $term"
    ! grep -Fq -- "$term" "$MIR_C_EXECUTION_OWNER" "$MIR_C_READ_OWNER" \
        "$MIR_C_STDOUT_OWNER" "$MIR_C_ARTIFACT_OWNER" ||
        fail "MIR-C compiler bypass escaped the payload owner: $term"
done
for term in 'func ProduceMirC(' 'self.direct_mir.execution.ProduceMirC('; do
    require_mir_c_text "$MIR_C_WORLD_OWNER" "$term"
done
for term in 'func ProduceMirCThroughPgyCompilerWorld(' \
    'compiler_world.ProduceMirC('; do
    require_mir_c_text "$MIR_C_COMPOSITION_OWNER" "$term"
done
for term in 'ProduceMirCThroughPgyCompilerWorld(' \
    'DriverRung2MirCPayloadAdmissionReadyFor(' \
    'DriverRung2MirCPayloadAdmissionDiagnostic(' \
    'case DriverRung2MirCPayloadAdmitted(receipt):' \
    'Log(receipt.artifact.payload);'; do
    require_mir_c_text "$MIR_C_STDOUT_OWNER" "$term"
done
[[ "$(grep -Fc 'DriverRung2CliLogMirCPayloadOrDie(' \
    "$MIR_C_READ_OWNER")" == "2" ]] ||
    fail "both MIR-C stdout requests must share one checked last consumer"
for installed_case in 'case DriverCliMirCStdout(input_path):' \
    'case DriverCliMirCManifestStdout(input_path, manifest_path):'; do
    installed_case_body="$(awk -v header="$installed_case" '
        index($0, header) { active = 1 }
        active { print }
        active && /case DriverCli/ && !index($0, header) { exit }
    ' "$MIR_C_INSTALLED_OWNER")"
    [[ "$(grep -Fc 'DriverRung2ExecuteReadRequest(request);' \
        <<<"$installed_case_body")" == "1" ]] ||
        fail "installed MIR-C case lost exact read delegation: $installed_case"
    for forbidden in 'ProduceMirC' 'CompileMirJsonToCVerified' 'Log('; do
        ! grep -Fq -- "$forbidden" <<<"$installed_case_body" ||
            fail "installed MIR-C case regained execution authority: $forbidden"
    done
done
for forbidden in 'SelfMirArtifactCommitPayload(' 'io_write' 'Fallback'; do
    ! grep -Fq -- "$forbidden" "$MIR_C_READ_OWNER" "$MIR_C_STDOUT_OWNER" ||
        fail "MIR-C stdout regained forbidden path: $forbidden"
done
require_mir_c_text "$MIR_C_READ_OWNER" 'MirCDefaultMachine'
require_mir_c_text "$MIR_C_READ_OWNER" 'MirCManifestVerified('
require_mir_c_text "$MIR_C_ARTIFACT_OWNER" \
    'input_path, output_path, mir_c_request, MirCDefaultMachine'

cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.artifact.normalized.c" ||
    fail "shared MIR-C admission changed stdout/artifact payload parity"
cmp -s "$WORK_DIR/mir.stdout.normalized.c" \
    "$WORK_DIR/mir.observed.normalized.c" ||
    fail "shared MIR-C admission changed observation payload parity"

case "$DRIVER" in
    *.exe) MIR_C_MANIFEST="${DRIVER%.exe}.machine-layer-manifest.json" ;;
    *) MIR_C_MANIFEST="${DRIVER}.machine-layer-manifest.json" ;;
esac
[[ -f "$MIR_C_MANIFEST" ]] ||
    fail "missing installed machine declaration: $MIR_C_MANIFEST"
MIR_C_MANIFEST_PATH="$MIR_C_MANIFEST"
command -v cygpath >/dev/null 2>&1 &&
    MIR_C_MANIFEST_PATH="$(cygpath -u "$MIR_C_MANIFEST")"
MIR_C_MANIFEST_REL="${MIR_C_MANIFEST_PATH#"$ROOT_DIR"/}"
[[ "$MIR_C_MANIFEST_REL" != "$MIR_C_MANIFEST_PATH" ]] ||
    fail "machine declaration escaped repository"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    --machine-manifest-json "$MIR_C_MANIFEST_REL") \
    >"$WORK_DIR/mir-manifest.c" 2>"$WORK_DIR/mir-manifest.err" ||
    fail "admitted-manifest MIR-C stdout failed"
grep -Fq '#include' "$WORK_DIR/mir-manifest.c" ||
    fail "admitted-manifest MIR-C stdout emitted no C payload"
grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/mir-manifest.c" ||
    fail "admitted-manifest MIR-C stdout lost its declaration mapping"

printf '%s\n' '{"schema":"invalid"}' >"$WORK_DIR/mir-invalid-manifest.json"
set +e
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$WORK_REL/source.mir.json" \
    --machine-manifest-json "$WORK_REL/mir-invalid-manifest.json") \
    >"$WORK_DIR/mir-invalid-manifest.out" \
    2>"$WORK_DIR/mir-invalid-manifest.err"
mir_c_invalid_manifest_rc=$?
set -e
[[ "$mir_c_invalid_manifest_rc" -ne 0 ]] ||
    fail "invalid MIR-C machine declaration was accepted"
! grep -Fq '#include' "$WORK_DIR/mir-invalid-manifest.out" ||
    fail "invalid MIR-C machine declaration emitted a C payload"
grep -Fq 'MIR C machine declaration is invalid' \
    "$WORK_DIR/mir-invalid-manifest.out" \
    "$WORK_DIR/mir-invalid-manifest.err" ||
    fail "invalid MIR-C machine declaration lost its typed diagnostic"

echo "[driver-mir-c-stdout-action] shared payload admission, manifest identity, parity, and typed invalid-manifest rejection: PASS"
