#!/usr/bin/env bash
set -euo pipefail

# Ratchets source_mir_main_direct_commit and source_mir_file_helper_fallback.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
MAIN_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
CLI_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_owner.pgy"
RUNG2_MAIN="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy"
SOURCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_execution_owner.pgy"
PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_mir_protocol_owner.pgy"
WORLD_OWNER="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
COMPOSITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
MIR_MANIFEST="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_mir_manifest_owner.pgy"
NATIVE_LAUNCHER="$ROOT_DIR/src/compiler/self_host_driver.c"
BUILD_OWNER="$ROOT_DIR/tests/self_hosted/parity/self_host_compiler_build.sh"
fail() { echo "[driver-source-mir-execution-action] $1" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
for owner in "$MAIN_OWNER" "$CLI_OWNER" "$RUNG2_MAIN" "$SOURCE_OWNER" "$PROTOCOL_OWNER" "$WORLD_OWNER" "$COMPOSITION_OWNER" "$MIR_MANIFEST" "$NATIVE_LAUNCHER" "$BUILD_OWNER"; do
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
for term in 'func DriverSourceMirProducePayloadAdmitted(' \
    'subject DriverSourceMirExecution' 'action ProduceSourceMir(' \
    'action PublishSourceMirArtifact(' 'within DriverSourceMirZone' \
    'authorized by self' 'public zone DriverSourceMirZone' \
    'subject slot execution: DriverSourceMirExecution' 'authority execution' \
    'full driver MIR production requires pressure observation' \
    'pressure-observed source MIR is only valid for the full driver' \
    'source MIR execution subject identity is invalid' \
    'source MIR execution topology identity is invalid' \
    'source MIR artifact destination path is empty' \
    'SelfMirArtifactCommitPayload(' 'output_path, payload_receipt.payload' \
    'case SelfMirArtifactCommitted(receipt):' 'case SelfMirArtifactRejected(failure):'; do
    require_text "$SOURCE_OWNER" "$term"
done
[[ "$(grep -Ec -- '^[[:space:]]*subject DriverSourceMirExecution[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one execution subject"
[[ "$(grep -Ec -- '^[[:space:]]*action ProduceSourceMir\(' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one payload action"
[[ "$(grep -Ec -- '^[[:space:]]*action PublishSourceMirArtifact\(' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one artifact action"
[[ "$(grep -Ec -- '^[[:space:]]*public zone DriverSourceMirZone[[:space:]]*\{' "$SOURCE_OWNER")" -eq 1 ]] || fail "source-MIR owner must declare exactly one execution zone"
[[ "$(grep -F -c -- 'SelfMirArtifactCommitPayload(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must own exactly one atomic payload commit"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonVerified(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must consume the verified payload owner exactly once"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonPressureObserved(' "$SOURCE_OWNER")" -eq 1 ]] ||
    fail "source-MIR action must consume the pressure payload owner exactly once"
[[ "$(grep -F -c -- 'DriverSourceMirProducePayloadAdmitted(' "$SOURCE_OWNER")" -eq 3 ]] ||
    fail "one payload admission owner must be defined once and consumed by both actions"
payload_action="$(awk '/^[[:space:]]*action ProduceSourceMir\(/{inside=1} inside{print} inside && /^[[:space:]]*}[[:space:]]*$/{exit}' "$SOURCE_OWNER")"
artifact_action="$(awk '/^[[:space:]]*action PublishSourceMirArtifact\(/{inside=1} inside{print} inside && /^[[:space:]]*}[[:space:]]*$/{exit}' "$SOURCE_OWNER")"
grep -Fq -- 'with caps io_read {' <<<"$payload_action" ||
    fail "payload action must require io_read only"
grep -Fq -- 'io_write' <<<"$payload_action" &&
    fail "payload action regained io_write authority"
grep -Fq -- 'with caps io_read, io_write {' <<<"$artifact_action" ||
    fail "artifact action must require io_read and io_write"
empty_path_line="$(grep -nF -- 'if output_path == "" {' "$SOURCE_OWNER" | cut -d: -f1)"
payload_call_line="$(grep -nF -- 'DriverSourceMirProducePayloadAdmitted(' "$SOURCE_OWNER" | sed -n '3p' | cut -d: -f1)"
[[ -n "$empty_path_line" && -n "$payload_call_line" && "$empty_path_line" -lt "$payload_call_line" ]] ||
    fail "artifact destination must fail closed before source-MIR production"
if grep -Eq -- '(^|[^[:alnum:]_])(WriteFile|FileOpen|FileWrite|FileClose)\(' "$SOURCE_OWNER" ||
    grep -Eq -- '(SelfMirArtifactBegin|CompilerArtifactWrite|SelfMirArtifactCommit|SelfMirArtifactAbort)\(' "$SOURCE_OWNER"; then
    fail "source-MIR action bypassed the atomic payload owner"
fi
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
for term in 'PublishSourceMirArtifactThroughPgyCompilerWorld(' 'DriverSourceMirExecutionOutcomeReadyFor(' \
    'DriverSourceMirExecutionOutcomeDiagnostic(' 'SourceMirPressureObserved' 'SourceMirVerified' \
    'args[1], args[2], machine_declaration, source_request' \
    'args[3] != "--pressure-owned-full-fixpoint"'; do
    require_text "$MAIN_OWNER" "$term"
done
[[ "$(grep -F -c -- 'PublishSourceMirArtifactThroughPgyCompilerWorld(' "$MAIN_OWNER")" -eq 1 ]] ||
    fail "Main must delegate source-MIR execution exactly once"
source_branch="$(awk '/args\[0\] == "--emit-mir-json-verified"/{inside=1} inside{print} inside && /^[[:space:]]*return;/{exit}' "$MAIN_OWNER")"
[[ -n "$source_branch" ]] || fail "Main source-MIR branch is missing"
grep -Fq -- 'SelfMirArtifactCommitPayload(' <<<"$source_branch" &&
    fail "Main source-MIR branch retained a direct artifact commit"
grep -Eq -- 'CompileSourceToMirJson(Verified|PressureObserved)\(' <<<"$source_branch" &&
    fail "Main source-MIR branch retained direct payload compilation"
for term in 'import "compiler_world_direct_mir_owner.pgy";' \
    'ProduceSourceMirThroughPgyCompilerWorld(' \
    'DriverSourceMirExecutionOutcomePayloadReadyFor(' \
    'DriverSourceMirExecutionOutcomePayloadDiagnostic(' \
    'case DriverSourceMirProduced(receipt): Log(receipt.payload);'; do
    require_text "$CLI_OWNER" "$term"
done
[[ "$(grep -F -c -- 'ProduceSourceMirThroughPgyCompilerWorld(' "$CLI_OWNER")" -eq 1 ]] ||
    fail "installed DRV-2 CLI must delegate source-MIR execution exactly once"
cli_source_branch="$(awk '/args\[0\] == "--emit-mir-json-verified"/{inside=1} inside{print} inside && /^[[:space:]]*return;/{exit}' "$CLI_OWNER")"
[[ -n "$cli_source_branch" ]] || fail "installed DRV-2 CLI source-MIR branch is missing"
grep -Eq -- 'CompileSourceToMirJson(Verified|PressureObserved)\(' <<<"$cli_source_branch" &&
    fail "installed DRV-2 CLI retained direct source-MIR compilation"
grep -Eq -- '(SelfMirArtifactCommitPayload|WriteFile|PublishSourceMirArtifact|[.]tmp)' <<<"$cli_source_branch" &&
    fail "stdout source-MIR mode must not round-trip through an artifact path"
require_text "$CLI_OWNER" 'with caps io_read {'
grep -Fq -- 'with caps io_read, io_write {' "$CLI_OWNER" &&
    fail "installed stdout CLI regained io_write authority"
require_text "$RUNG2_MAIN" 'func Main() -> Void with caps env, io_read {'
require_text "$RUNG2_MAIN" 'import "driver_rung2_cli_owner.pgy";'
require_text "$BUILD_OWNER" 'DRIVER_SOURCE="src/self_hosted/compiler/driver_rung2_main.pgy"'
require_text "$BUILD_OWNER" 'OUTPUT="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"'
require_text "$NATIVE_LAUNCHER" 'path_join_dup(directory, "pgy-self-driver")'
retired_file_helpers="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJsonFile(Verified|PressureObserved)\(' {} + || true
)"
if [[ -n "$retired_file_helpers" ]]; then
    fail "retired source-MIR file helper definition or call returned"
fi
source_mir_call_files="$(
    find "$ROOT_DIR/src/self_hosted" -type f -name '*.pgy' \
        -exec grep -lE 'CompileSourceToMirJson(Verified|PressureObserved)\(' {} + || true
)"
while IFS= read -r call_file; do
    [[ -n "$call_file" ]] || continue
    case "$call_file" in
        "$SOURCE_OWNER"|"$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy") ;;
        *) fail "source-MIR payload owner escaped its two-file allow-list: ${call_file#"$ROOT_DIR/"}" ;;
    esac
done <<<"$source_mir_call_files"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonVerified(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 2 ]] ||
    fail "verified source-MIR definition/internal source-to-C consumer inventory drifted"
[[ "$(grep -F -c -- 'CompileSourceToMirJsonPressureObserved(' "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy")" -eq 1 ]] ||
    fail "pressure source-MIR definition inventory drifted"
require_text "$MIR_MANIFEST" 'examples/function_clause_order_minimal.pgy'
require_text "$ROOT_DIR/Makefile" 'function_clause_order_minimal'
echo "[driver-source-mir-execution-action] artifact+stdout source-MIR world/action/pressure/commit ratchet PASS"
