#!/usr/bin/env bash
set -euo pipefail
# Ratchets source_c_installed_direct_compile_commit.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
SOURCE="examples/composite_intent_orchestration/main.pgy"
WORK_REL=".tmp/self_hosted/driver_source_c_execution_action"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_execution_owner.pgy"
PROTOCOL_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_source_c_protocol_owner.pgy"
WORLD_OWNER="$ROOT_DIR/src/self_hosted/compiler/world.pgy"
COMPOSITION_OWNER="$ROOT_DIR/src/self_hosted/compiler/compiler_world_direct_mir_owner.pgy"
ROOT_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy"
INSTALLED_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_installed_cli_owner.pgy"
ARTIFACT_EXECUTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_artifact_request_execution_owner.pgy"

fail() { echo "[driver-source-c-execution-action] $1" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then PGY="${PGY}.exe"; fi
if [[ "$SELF_DRIVER" != *.exe ]] && pgy_binary_expects_windows_paths "${SELF_DRIVER}.exe"; then SELF_DRIVER="${SELF_DRIVER}.exe"; fi
[[ -x "$PGY" ]] || fail "missing public launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
MACHINE_MANIFEST="$(pgy_self_driver_machine_manifest_path "$SELF_DRIVER")"
[[ -s "$MACHINE_MANIFEST" ]] || fail "missing installed machine manifest"
# Match the production adapter's repository-relative companion spelling.
MACHINE_MANIFEST_ARG="${MACHINE_MANIFEST#"$ROOT_DIR"/}"

for owner in "$SOURCE_OWNER" "$PROTOCOL_OWNER" "$WORLD_OWNER" "$COMPOSITION_OWNER" \
    "$ROOT_OWNER" "$INSTALLED_OWNER" "$ARTIFACT_EXECUTION_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
for term in 'tobject DriverSourceCExecutionReceipt' \
    'tobject DriverSourceCExecutionRejection' \
    'enum DriverSourceCExecutionOutcome' 'func DriverSourceCExecutedOutcome(' 'func DriverSourceCArtifactRejectedOutcome('; do
    require_text "$PROTOCOL_OWNER" "$term"
done
for term in 'subject DriverSourceCExecution' 'let mut outcome: Option<DriverSourceCExecutionOutcome>;' 'action Compile(' \
    'within DriverSourceCZone' 'authorized by self' 'public intent CompilePergyraCArtifact(' \
    'public zone DriverSourceCZone' \
    'subject slot execution: DriverSourceCExecution' 'authority execution' \
    'source C execution subject identity is invalid' \
    'source C execution topology identity is invalid' \
    'source C artifact destination path is empty' \
    'CompileSourceToCVerified(' 'SelfMirArtifactCommitPayload(' \
    'case SelfMirArtifactCommitted(commit_receipt):' \
    'case SelfMirArtifactRejected(failure):'; do
    require_text "$SOURCE_OWNER" "$term"
done
[[ "$(grep -Fc 'CompileSourceToCVerified(' "$SOURCE_OWNER")" == "1" ]] ||
    fail "source-C action must consume the existing compiler exactly once"
[[ "$(grep -Fc 'SelfMirArtifactCommitPayload(' "$SOURCE_OWNER")" == "1" ]] ||
    fail "source-C action must commit through one artifact transaction"
grep -Fq 'Fallback' "$SOURCE_OWNER" && fail "source-C action regained a fallback branch"

for term in 'zone source_c: DriverSourceCZone' \
    'func CompileSourceToC(' 'CompilePergyraCArtifact(' \
    'DriverSourceCExecutionOutcomeReadyFor('; do
    require_text "$WORLD_OWNER" "$term"
done
for term in 'let compiler_world: PgyCompilerWorld = PgyCompilerWorld(' \
    'DriverSourceCZone(DriverSourceCExecution(' 'DriverSourceCExecutionTopologyIdentity(), None' \
    'DriverRung2ExecuteInstalledRequest(compiler_world, request, compatibility_receipt);'; do require_text "$ROOT_OWNER" "$term"; done
[[ "$(grep -Fc 'let compiler_world: PgyCompilerWorld = PgyCompilerWorld(' "$ROOT_OWNER")" == "1" ]] || fail "installed compiler root must own exactly one fresh world"
for term in 'inout compiler_world: PgyCompilerWorld' 'func CompileSourceToCThroughPgyCompilerWorld(' \
    'compiler_world.CompileSourceToC('; do require_text "$COMPOSITION_OWNER" "$term"; done
! grep -Fq 'let compiler_world: PgyCompilerWorld = PgyCompilerWorld(' "$COMPOSITION_OWNER" || fail "source-C route owner regained world reconstruction"
for term in 'func DriverRung2InstalledPublishSourceC(' 'CompileSourceToCThroughPgyCompilerWorld(' \
    'compiler_world, source_path, output_path, request' 'DriverSourceCExecutionOutcomeReadyFor(' \
    'DriverSourceCExecutionOutcomeDiagnostic('; do require_text "$ARTIFACT_EXECUTION_OWNER" "$term"; done
require_text "$INSTALLED_OWNER" 'case DriverCliSourceCArtifact(source_path, output_path, manifest_path, emit_json):'
require_text "$INSTALLED_OWNER" 'SourceCManifestVerified('
! grep -Fq 'DriverRung2InstalledCommitSourceC' "$ARTIFACT_EXECUTION_OWNER" ||
    fail "retired installed source-C direct commit returned"
installed_publish="$(awk '/^func DriverRung2InstalledPublishSourceC\(/{inside=1} inside{print} inside && /^}/{exit}' "$ARTIFACT_EXECUTION_OWNER")"
grep -Fq 'IntentHistoryCount() != history_before + 1' <<<"$installed_publish" && grep -Fq 'IntentLastName() != "CompilePergyraCArtifact"' <<<"$installed_publish" || fail "installed source-C intent trace escaped its consumer"
grep -Fq 'CompileSourceToCVerified(' <<<"$installed_publish" &&
    fail "installed CLI regained the direct source-C compiler bypass"
grep -Fq 'SelfMirArtifactCommitPayload(' <<<"$installed_publish" &&
    fail "installed CLI regained the direct source-C commit bypass"
grep -REq 'PublishSourceCArtifactThroughPgyCompilerWorld|\.PublishSourceCArtifact\(' \
    "$COMPOSITION_OWNER" "$WORLD_OWNER" "$ARTIFACT_EXECUTION_OWNER" && fail "retired direct source-C world publish returned"
rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-c-artifact-verified \
    "$SOURCE" "$WORK_REL/direct.c" --machine-manifest-json \
    "$MACHINE_MANIFEST_ARG")
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/public.c") \
    >"$WORK_DIR/public.out" 2>"$WORK_DIR/public.err"
cmp -s "$WORK_DIR/direct.c" "$WORK_DIR/public.c" ||
    fail "public source-C artifact differs from the installed driver"

pgy_selfhost_select_emitted_c_compile_profile ||
    fail "emitted C compile profile is invalid"
compile_command=("$CC" -x c -std=c11)
compile_command+=("${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}")
if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/public.c"; then
    compile_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
compile_command+=("$WORK_DIR/public.c" -o "$WORK_DIR/public-program")
"${compile_command[@]}"
"$WORK_DIR/public-program" | tr -d '\r' >"$WORK_DIR/public-program.out"
grep -Fxq '[Intent] ProcessOrder=true' "$WORK_DIR/public-program.out" ||
    fail "public source-C execution lost the canonical intent result"
grep -Fxq '[Trace] [intent] enter ProcessOrder' "$WORK_DIR/public-program.out" ||
    fail "public source-C execution lost the canonical trace"

set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN && \
    "$PGY" "$SOURCE" --emit-c -o "$WORK_REL/missing-parent/out.c") \
    >"$WORK_DIR/rejected.out" 2>"$WORK_DIR/rejected.err"
rejected_rc=$?
set -e
[[ "$rejected_rc" -ne 0 && ! -e "$WORK_DIR/missing-parent/out.c" ]] ||
    fail "artifact transaction failure published source C"
grep -Fq 'artifact transaction rejected:' "$WORK_DIR/rejected.out" "$WORK_DIR/rejected.err" ||
    fail "artifact transaction failure lost its typed diagnostic"

source "$ROOT_DIR/tests/self_hosted/parity/driver_source_c_stdout_execution_action_gate.sh"

echo "[driver-source-c-execution-action] world/action artifact parity, execution, and transaction rejection: PASS"
