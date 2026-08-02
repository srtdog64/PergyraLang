#!/usr/bin/env bash
set -euo pipefail

# Ratchets source_mir_main_direct_commit and source_mir_file_helper_fallback.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy" CLI_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_owner.pgy"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy" READ_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy"
INSTALLED_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy" SOURCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy" WORLD_OWNER="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
COMPOSITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy" MIR_MANIFEST="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy"
NATIVE_LAUNCHER="$ROOT_DIR/src/compiler/self_host_driver.c" BUILD_OWNER="$ROOT_DIR/tests/self_hosted/parity/self_host_compiler_build.sh"
fail() { echo "[driver-source-mir-execution-action] $1" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
for owner in "$MAIN_OWNER" "$CLI_OWNER" "$REQUEST_OWNER" "$READ_OWNER" "$INSTALLED_OWNER" \
    "$SOURCE_OWNER" "$PROTOCOL_OWNER" "$WORLD_OWNER" "$COMPOSITION_OWNER" \
    "$MIR_MANIFEST" "$NATIVE_LAUNCHER" "$BUILD_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
for term in \
    'enum DriverSourceMirRequest' 'SourceMirVerified' 'SourceMirPressureObserved' \
    'tobject DriverSourceMirPayloadReceipt' \
    'tobject DriverSourceMirExecutionReceipt' 'tobject DriverSourceMirExecutionRejection' \
    'enum DriverSourceMirExecutionOutcome' 'DriverSourceMirProduced(' 'DriverSourceMirExecuted(' \
    'DriverSourceMirRejected(' 'DriverSourceMirArtifactRejected(SelfMirArtifactFailure)' \
    'enum DriverSourceMirPayloadAdmission' 'DriverSourceMirPayloadAdmitted(' \
    'DriverSourceMirPayloadDenied(' \
    'func DriverSourceMirPayloadReceiptReadyFor(' \
    'func DriverSourceMirExecutionOutcomePayloadReadyFor(' \
    'func DriverSourceMirExecutionOutcomePayloadDiagnostic(' \
    'func DriverSourceMirRequestObservesPressure(' 'func DriverSourceMirIsFullDriverSource(' \
    'func DriverSourceMirExecutionOwnerIdentity(' 'func DriverSourceMirExecutionTopologyIdentity('; do
    require_text "$PROTOCOL_OWNER" "$term"
done
for term in 'func DriverSourceMirRequestDiagnostic(' \
    'func DriverSourceMirProjectionFromAdmittedRequest(' \
    'func DriverSourceMirProducePayloadAdmitted(' \
    'subject DriverSourceMirExecution' 'action ProduceSourceMir(' \
    'action PublishSourceMirArtifact(' 'within DriverSourceMirZone' \
    'authorized by self' 'public zone DriverSourceMirZone' \
    'subject slot execution: DriverSourceMirExecution' 'authority execution' \
    'full driver MIR production requires pressure observation' \
    'pressure-observed source MIR is only valid for the full driver' \
    'source MIR execution subject identity is invalid' \
    'source MIR execution topology identity is invalid' \
    'source MIR artifact destination path is empty' \
    'DriverRung2MirProjectionJson(' \
    'SelfMirProgramJsonWriteArtifactVerified(' \
    'case SelfMirArtifactCommitted(receipt):' 'case SelfMirArtifactRejected(failure):'; do
    require_text "$SOURCE_OWNER" "$term"
done
[[ "$(grep -Ec -- '^[[:space:]]*subject DriverSourceMirExecution[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one execution subject"
[[ "$(grep -Ec -- '^[[:space:]]*action ProduceSourceMir\(' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one payload action"
[[ "$(grep -Ec -- '^[[:space:]]*action PublishSourceMirArtifact\(' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one artifact action"
[[ "$(grep -Ec -- '^[[:space:]]*public zone DriverSourceMirZone[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one execution zone"
[[ "$(grep -F -c -- 'SelfMirProgramJsonWriteArtifactVerified(' "$SOURCE_OWNER")" -eq 1 ]] || fail "artifact action must consume the streaming MIR writer exactly once"
[[ "$(grep -F -c -- 'DriverSourceMirRequestDiagnostic(' "$SOURCE_OWNER")" -eq 3 ]] || fail "one request admission owner must be defined once and consumed by both actions"
[[ "$(grep -F -c -- 'DriverSourceMirProjectionFromAdmittedRequest(' "$SOURCE_OWNER")" -eq 3 ]] || fail "one source-to-MIR projection owner must be defined once and consumed by both actions"
[[ "$(grep -F -c -- 'DriverSourceMirProducePayloadAdmitted(' "$SOURCE_OWNER")" -eq 2 ]] || fail "payload admission owner must be defined once and consumed only by stdout"
payload_action="$(awk '/^[[:space:]]*action ProduceSourceMir\(/{inside=1} inside{print} inside && /^    }[[:space:]]*$/{exit}' "$SOURCE_OWNER")"
artifact_action="$(awk '/^[[:space:]]*action PublishSourceMirArtifact\(/{inside=1} inside{print} inside && /^    }[[:space:]]*$/{exit}' "$SOURCE_OWNER")"
grep -Fq -- 'with caps io_read {' <<<"$payload_action" || fail "payload action must require io_read only"
grep -Fq -- 'io_write' <<<"$payload_action" && fail "payload action regained io_write authority"
grep -Fq -- 'with caps io_read, io_write {' <<<"$artifact_action" || fail "artifact action must require io_read and io_write"
empty_path_line="$(grep -nF -- 'if output_path == "" {' "$SOURCE_OWNER" | cut -d: -f1)"
projection_call_line="$(grep -nF -- 'DriverSourceMirProjectionFromAdmittedRequest(' "$SOURCE_OWNER" | sed -n '3p' | cut -d: -f1)"
[[ -n "$empty_path_line" && -n "$projection_call_line" && "$empty_path_line" -lt "$projection_call_line" ]] || fail "artifact destination must fail closed before source-MIR production"
if grep -Eq -- '(^|[^[:alnum:]_])(WriteFile|FileOpen|FileWrite|FileClose)\(' "$SOURCE_OWNER" ||
    grep -Eq -- '(SelfMirArtifactBegin|CompilerArtifactWrite|SelfMirArtifactCommit|SelfMirArtifactAbort)\(' "$SOURCE_OWNER"; then
    fail "source-MIR action bypassed the atomic streaming owner"
fi
grep -Fq -- 'SelfMirArtifactCommitPayload(' "$SOURCE_OWNER" && fail "source-MIR artifact mode rematerialized a whole payload"
grep -Eq -- '(payload_receipt|DriverSourceMirProducePayloadAdmitted\(|DriverRung2MirProjectionJson\()' <<<"$artifact_action" && fail "artifact action regained the stdout payload path"
grep -Fq -- 'SelfMirProgramJsonWriteArtifactVerified(' <<<"$artifact_action" || fail "artifact action no longer streams verified program facts"
grep -Eq -- '(SelfMirProgramJsonWriteArtifactVerified|io_write)' <<<"$payload_action" && fail "stdout payload action regained artifact authority"
grep -Fq -- 'Fallback' "$SOURCE_OWNER" && fail "source-MIR action regained a fallback branch"
world_zones="$(
    awk '/^world PgyCompilerWorld[[:space:]]*\{/{inside=1; next} inside && /^}/{exit} inside && /^[[:space:]]+zone /{print}' "$WORLD_OWNER"
)"
world_zone_count="$(printf '%s\n' "$world_zones" | awk 'NF { count++ } END { print count + 0 }')"
first_world_zone="$(printf '%s\n' "$world_zones" | sed -n '1p')"
second_world_zone="$(printf '%s\n' "$world_zones" | sed -n '2p')"
[[ "$world_zone_count" -eq 2 ]] || fail "PgyCompilerWorld must expose exactly two executable zone fields"
[[ "$first_world_zone" == *'zone direct_mir: DriverRung2DirectMirZone'* ]] ||
    fail "direct-MIR zone is not the first world field"
[[ "$second_world_zone" == *'zone source_mir: DriverSourceMirZone'* ]] ||
    fail "source-MIR zone is not the second world field"
require_text "$WORLD_OWNER" 'func ProduceSourceMir('
require_text "$WORLD_OWNER" 'self.source_mir.execution.ProduceSourceMir('
require_text "$WORLD_OWNER" 'func PublishSourceMirArtifact('
require_text "$WORLD_OWNER" 'self.source_mir.execution.PublishSourceMirArtifact('
world_declarations="$(
    find "$ROOT_DIR/src/self_hosted/compiler" -type f -name '*.pgy' \
        -exec grep -HnE '^[[:space:]]*(public[[:space:]]+)?world[[:space:]]+' {} + || true
)"
world_declaration_count="$(printf '%s\n' "$world_declarations" | awk 'NF { count++ } END { print count + 0 }')"
[[ "$world_declaration_count" -eq 1 ]] || fail "compiler tree declared a second world"
world_declaration="$(printf '%s\n' "$world_declarations" | sed -n '1p' | tr '\\' '/')"
[[ "$world_declaration" == *'/src/self_hosted/compiler/world.pgy:'* ]] ||
    fail "PgyCompilerWorld moved from its canonical owner"
constructor_sites="$(
    find "$ROOT_DIR/src/self_hosted/compiler" -type f -name '*.pgy' \
        -exec grep -lE '(^|[^[:alnum:]_])PgyCompilerWorld\(' {} + || true
)"
constructor_site_count="$(printf '%s\n' "$constructor_sites" | awk 'NF { count++ } END { print count + 0 }')"
[[ "$constructor_site_count" -eq 1 ]] || fail "compiler tree must have one world materialization root"
constructor_site="$(printf '%s\n' "$constructor_sites" | sed -n '1p' | tr '\\' '/')"
[[ "$constructor_site" == *'/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy' ]] ||
    fail "world materialization escaped its composition owner"
[[ "$(grep -Ec -- '(^|[^[:alnum:]_])PgyCompilerWorld\(' "$COMPOSITION_OWNER")" -eq 1 ]] ||
    fail "composition root must materialize the world exactly once"
for term in 'DriverRung2DirectMirZone(' 'DriverSourceMirZone(' \
    'func ProduceSourceMirThroughPgyCompilerWorld(' \
    'func PublishSourceMirArtifactThroughPgyCompilerWorld('; do
    require_text "$COMPOSITION_OWNER" "$term"
done
for term in 'enum DriverRung2CliRequest' 'func DriverRung2CliRequestFromArgsOrDie(' \
    'DriverCliSourceMirStdout(String)' 'DriverCliSourceMirManifestStdout(String, String)' \
    'DriverCliSourceMirArtifact(String, String)' 'DriverCliSourceMirPressureArtifact(String, String)' \
    'args[2] == "--machine-manifest-json"' 'args[2] == "-o"' 'args[2] == "--pressure-owned-full-fixpoint"' \
    'driver rung-2 requires an explicit source or mode'; do
    require_text "$REQUEST_OWNER" "$term"
done
grep -Eq -- '(io_read|io_write|ReadFile\(|WriteFile\(|CompileSource|CompileMir|PublishSourceMir)' "$REQUEST_OWNER" && fail "pure CLI request admission regained compiler or I/O authority"
for term in 'func DriverRung2CliLogSourceMirPayloadOrDie(' 'ProduceSourceMirThroughPgyCompilerWorld(' \
    'DriverSourceMirExecutionOutcomePayloadReadyFor(' 'DriverSourceMirExecutionOutcomePayloadDiagnostic(' \
    'case DriverSourceMirProduced(receipt): Log(receipt.payload);' \
    'driver rung-2 artifact request requires installed composition root'; do
    require_text "$READ_OWNER" "$term"
done
[[ "$(grep -F -c -- 'ProduceSourceMirThroughPgyCompilerWorld(' "$READ_OWNER")" -eq 1 ]] || fail "read executor must delegate source-MIR stdout exactly once"
grep -Eq -- '(io_write|SelfMirArtifactCommitPayload|PublishSourceMirArtifact)' "$READ_OWNER" && fail "read executor regained artifact publication authority"
for term in 'func DriverRung2InstalledPublishSourceMir(' 'PublishSourceMirArtifactThroughPgyCompilerWorld(' \
    'DriverSourceMirExecutionOutcomeReadyFor(' 'DriverSourceMirExecutionOutcomeDiagnostic(' \
    'case DriverCliSourceMirArtifact(source_path, output_path):' \
    'case DriverCliSourceMirPressureArtifact(source_path, output_path):' \
    'SourceMirPressureObserved' 'SourceMirVerified'; do
    require_text "$INSTALLED_OWNER" "$term"
done
[[ "$(grep -F -c -- 'PublishSourceMirArtifactThroughPgyCompilerWorld(' "$INSTALLED_OWNER")" -eq 1 ]] || fail "installed executor must delegate source-MIR artifact publication exactly once"
for owner in "$MAIN_OWNER" "$CLI_OWNER" "$READ_OWNER" "$INSTALLED_OWNER"; do
    grep -Eq -- 'args\[[0-9]+\]' "$owner" && fail "raw argv indexing escaped the request owner: ${owner#"$ROOT_DIR/"}"
done
for term in 'import "driver_rung2_cli_request_owner.pgy";' 'import "driver_rung2_installed_cli_owner.pgy";' \
    'DriverRung2CliRequestFromArgsOrDie(Args())' \
    'DriverRung2ExecuteInstalledRequest(request);'; do
    require_text "$MAIN_OWNER" "$term"
done
grep -Eq -- '(PublishSourceMirArtifact|ProduceSourceMir|SelfMirArtifactCommitPayload|--emit-mir-json-verified)' "$MAIN_OWNER" && fail "installed Main regained source-MIR routing or publication"
for term in 'import "driver_rung2_cli_request_owner.pgy";' 'import "driver_rung2_cli_read_execution_owner.pgy";' \
    'DriverRung2CliRequestFromArgsOrDie(args)' \
    'DriverRung2ExecuteReadRequest(request);' 'with caps io_read {'; do
    require_text "$CLI_OWNER" "$term"
done
grep -Fq -- 'io_write' "$CLI_OWNER" && fail "standalone CLI wrapper regained io_write authority"
require_text "$BUILD_OWNER" 'DRIVER_SOURCE="src/self_hosted/compiler/driver_bootstrap_main.pgy"'
grep -Fq -- 'DRIVER_SOURCE="src/self_hosted/compiler/driver_rung2_main.pgy"' "$BUILD_OWNER" && fail "installed build returned to the test-only DRV-2 entrypoint"
require_text "$BUILD_OWNER" 'OUTPUT="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"'
require_text "$NATIVE_LAUNCHER" 'path_join_dup(directory, "pgy-self-driver")'
retired_file_helpers="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJsonFile(Verified|PressureObserved)\(' {} + || true
)"
[[ -z "$retired_file_helpers" ]] || fail "retired source-MIR file helper definition or call returned"
source_mir_call_files="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJson(Verified|PressureObserved)\(' {} + || true
)"
while IFS= read -r call_file; do
    [[ -n "$call_file" ]] || continue
    [[ "$call_file" == "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy" ]] ||
        fail "legacy source-MIR payload wrapper escaped its owner: ${call_file#"$ROOT_DIR/"}"
done <<<"$source_mir_call_files"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonVerified(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 2 ]] ||
    fail "verified source-MIR definition/internal source-to-C consumer inventory drifted"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonPressureObserved(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 1 ]] ||
    fail "pressure source-MIR definition inventory drifted"
require_text "$MIR_MANIFEST" 'examples/function_clause_order_minimal.pgy'
require_text "$ROOT_DIR/Makefile" 'function_clause_order_minimal'
echo "[driver-source-mir-execution-action] artifact+stdout source-MIR world/action/pressure/commit ratchet PASS"
