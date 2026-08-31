#!/usr/bin/env bash
# Sourced by driver_source_c_execution_action_gate.sh after its installed
# driver and artifact evidence are ready.

READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
STDOUT_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_stdout_execution_owner.pgy" C_REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_request_owner.pgy"

for owner in "$READ_OWNER" "$STDOUT_OWNER" "$C_REQUEST_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
for term in 'tobject DriverSourceCPayloadReceipt' \
    'artifact: CompilerEmissionArtifact;' \
    'machine_manifest_id: String;' \
    'machine_manifest_fingerprint: String;' \
    'enum DriverSourceCPayloadAdmission' \
    'func DriverSourceCPayloadAdmissionReadyFor(' \
    'func DriverSourceCPayloadAdmissionDiagnostic('; do
    require_text "$PROTOCOL_OWNER" "$term"
done
for term in 'enum DriverSourceCRequest' 'SourceCDefault(Bool)' \
    'SourceCManifestVerified(SelfHostMachineLayerDeclaration, Bool)' \
    'func DriverSourceCRequestEmitsJsonDiagnostic(' \
    'func DriverSourceCRequestDeclaration(' \
    'func DriverSourceCRequestManifestId(' \
    'func DriverSourceCRequestManifestFingerprint('; do
    require_text "$C_REQUEST_OWNER" "$term"; done
for term in 'func DriverSourceCProducePayloadAdmitted(' \
    'action ProduceSourceC(' 'DriverSourceCPayloadReceipt(' \
    'DriverSourceCRequestDeclaration(request)' \
    'DriverSourceCRequestManifestId(request)' \
    'DriverSourceCRequestManifestFingerprint(request)' \
    'source C machine declaration is invalid' \
    '!CompilerEmissionArtifactReady(artifact)' \
    'source C compiler artifact target is invalid';
do
    require_text "$SOURCE_OWNER" "$term"
done
[[ "$(grep -Fc 'CompileSourceToCVerified(' "$SOURCE_OWNER")" == "1" ]] ||
    fail "source-C payload admission must consume the compiler exactly once"
[[ "$(grep -Fc 'CompilerEmissionArtifactReady(' "$SOURCE_OWNER")" == "1" ]] ||
    fail "source-C admission must use canonical artifact readiness once"
[[ "$(grep -Fc 'CompilerEmissionArtifactReady(' "$PROTOCOL_OWNER")" == "1" ]] ||
    fail "detached source-C receipt must use canonical artifact readiness once"
diagnostic_line="$(grep -n 'let diagnostic: String = DriverSourceCRequestDiagnostic(' "$SOURCE_OWNER" | cut -d: -f1)"
compile_line="$(grep -n 'let artifact: CompilerEmissionArtifact = CompileSourceToCVerified(' "$SOURCE_OWNER" | cut -d: -f1)"
admitted_line="$(grep -n 'return DriverSourceCPayloadAdmitted(' "$SOURCE_OWNER" | cut -d: -f1)"
[[ "$diagnostic_line" -lt "$compile_line" && "$compile_line" -lt "$admitted_line" ]] ||
    fail "source-C request, compiler artifact, and admission order drifted"
for term in 'func ProduceSourceC(' \
    'self.source_c.execution.ProduceSourceC('; do
    require_text "$WORLD_OWNER" "$term"
done
for term in 'func ProduceSourceCThroughPgyCompilerWorld(' \
    'compiler_world.ProduceSourceC('; do
    require_text "$COMPOSITION_OWNER" "$term"
done
for term in 'ProduceSourceCThroughPgyCompilerWorld(' \
    'DriverSourceCPayloadAdmissionReadyFor(' \
    'DriverSourceCPayloadAdmissionDiagnostic(' \
    'case DriverSourceCPayloadAdmitted(receipt):' \
    'Log(receipt.artifact.payload);'; do
    require_text "$STDOUT_OWNER" "$term"
done
[[ "$(grep -Fc 'DriverRung2CliLogSourceCPayloadOrDie(' "$READ_OWNER")" == "2" ]] ||
    fail "both source-C stdout requests must share one checked last consumer"
for forbidden in 'CompileSourceToCVerified(' 'SelfMirArtifactCommitPayload(' \
    'io_write' 'Fallback'; do
    ! grep -Fq -- "$forbidden" "$READ_OWNER" "$STDOUT_OWNER" ||
        fail "source-C stdout regained forbidden path: $forbidden"
done
require_text "$READ_OWNER" 'SelfHostMachineLayerDeclarationFromPath(manifest_path)'
require_text "$READ_OWNER" 'SourceCDefault'
require_text "$READ_OWNER" 'SourceCManifestVerified('

(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE") >"$WORK_DIR/stdout-default.c"
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --emit-c-verified) \
    >"$WORK_DIR/stdout-explicit.c"
tr -d '\r' <"$WORK_DIR/stdout-default.c" >"$WORK_DIR/stdout-default.normalized.c"
tr -d '\r' <"$WORK_DIR/direct.c" >"$WORK_DIR/direct.normalized.c"
cmp -s "$WORK_DIR/stdout-default.c" "$WORK_DIR/stdout-explicit.c" ||
    fail "default and explicit source-C stdout bytes differ"
cmp -s "$WORK_DIR/stdout-default.normalized.c" "$WORK_DIR/direct.normalized.c" ||
    fail "source-C stdout and artifact actions produced different payloads"

MANIFEST="$(pgy_self_driver_machine_manifest_path "$SELF_DRIVER")"
[[ -f "$MANIFEST" ]] || fail "missing installed machine declaration: $MANIFEST"
MANIFEST_REL="${MANIFEST#"$ROOT_DIR"/}"
[[ "$MANIFEST_REL" != "$MANIFEST" ]] || fail "machine declaration escaped repository"
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --machine-manifest-json "$MANIFEST_REL") \
    >"$WORK_DIR/stdout-non-machine-manifest.c"
! grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/stdout-non-machine-manifest.c" ||
    fail "non-machine source emitted a machine startup call"
(cd "$ROOT_DIR" && "$SELF_DRIVER" tests/cases/backend_compare/device_slot_machine_layer/main.pgy \
    --machine-manifest-json "$MANIFEST_REL") \
    >"$WORK_DIR/stdout-manifest.c"
grep -Fq '#include' "$WORK_DIR/stdout-manifest.c" ||
    fail "manifest source-C stdout emitted no C payload"
grep -Fq 'pgy_machine_layer_require_mapping();' "$WORK_DIR/stdout-manifest.c" ||
    fail "DeviceSlot source-C stdout lost its admitted machine declaration"

printf '%s\n' '{"schema":"invalid"}' >"$WORK_DIR/invalid-manifest.json"
set +e
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$SOURCE" --machine-manifest-json \
    "$WORK_REL/invalid-manifest.json") >"$WORK_DIR/invalid-manifest.out" \
    2>"$WORK_DIR/invalid-manifest.err"
invalid_manifest_rc=$?
set -e
[[ "$invalid_manifest_rc" -ne 0 ]] || fail "invalid machine declaration was accepted"
! grep -Fq '#include' "$WORK_DIR/invalid-manifest.out" ||
    fail "invalid machine declaration emitted a C payload"
grep -Fq 'source C machine declaration is invalid' \
    "$WORK_DIR/invalid-manifest.out" "$WORK_DIR/invalid-manifest.err" ||
    fail "invalid machine declaration lost its typed diagnostic"

echo "[driver-source-c-stdout-action] default/explicit raw byte parity, normalized artifact payload parity, admitted manifest, and typed invalid-manifest rejection: PASS"
