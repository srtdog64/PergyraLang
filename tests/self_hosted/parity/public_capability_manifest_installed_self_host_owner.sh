#!/usr/bin/env bash
# Public source capability manifests are derived by the installed Pergyra
# semantic owner. The native implementation remains only the explicit oracle.
# Closed fallbacks: public_capability_manifest_native_fallback,
# public_capability_manifest_oracle_self_compare,
# capability_declared_as_used, capability_renderer_builtin_rescan,
# capability_missing_call_target_success,
# capability_program_mask_without_callable_fixed_point,
# immutable_source_capability_manifest_companion,
# missing_capability_driver_native_retry.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
WORK_REL=".tmp/self_hosted/public_capability_manifest_installed"
WORK_DIR="$ROOT_DIR/$WORK_REL"
DIRECT_MODE="--emit-capability-manifest-verified"

fail() {
    echo "[self-host-public-capability-manifest] $*" >&2
    exit 1
}

require_text() {
    grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"
}

normalize() {
    pgy_selfhost_normalize_text_artifact <"$1" >"$2"
}

json_suffix() {
    awk 'seen || /^\{/ { seen=1; print }' "$1"
}

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" ]] || fail "missing public pgy launcher: $PGY"
[[ -x "$SELF_DRIVER" ]] || fail "missing installed self-host driver: $SELF_DRIVER"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

for case_name in manifest_clean manifest_declared_ok; do
    source_path="tests/capability/${case_name}.pgy"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE" "$source_path") \
        >"$WORK_DIR/${case_name}.direct" 2>"$WORK_DIR/${case_name}.direct.err"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
        "$PGY" --capability-manifest "$source_path") \
        >"$WORK_DIR/${case_name}.public" 2>"$WORK_DIR/${case_name}.public.err"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline \
        --capability-manifest "$source_path") \
        >"$WORK_DIR/${case_name}.native" 2>"$WORK_DIR/${case_name}.native.err"
    cmp -s "$WORK_DIR/${case_name}.direct" "$WORK_DIR/${case_name}.public" ||
        fail "$case_name public bytes differ from the installed semantic owner"
    normalize "$WORK_DIR/${case_name}.direct" "$WORK_DIR/${case_name}.direct.norm"
    normalize "$WORK_DIR/${case_name}.native" "$WORK_DIR/${case_name}.native.norm"
    cmp -s "$WORK_DIR/${case_name}.direct.norm" "$WORK_DIR/${case_name}.native.norm" ||
        fail "$case_name installed manifest differs from the native oracle"
done

while IFS='|' read -r case_name function_name missing_capability; do
    source_path="tests/capability/${case_name}.pgy"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE" "$source_path") \
        >"$WORK_DIR/${case_name}.direct" 2>"$WORK_DIR/${case_name}.direct.err"
    direct_rc=$?
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
        "$PGY" --capability-manifest "$source_path") \
        >"$WORK_DIR/${case_name}.public" 2>"$WORK_DIR/${case_name}.public.err"
    public_rc=$?
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline \
        --capability-manifest "$source_path") \
        >"$WORK_DIR/${case_name}.native" 2>"$WORK_DIR/${case_name}.native.err"
    native_rc=$?
    set -e
    [[ "$direct_rc" -ne 0 && "$public_rc" -ne 0 && "$native_rc" -ne 0 ]] ||
        fail "$case_name under-declaration did not fail all three voices"
    cmp -s "$WORK_DIR/${case_name}.direct" "$WORK_DIR/${case_name}.public" ||
        fail "$case_name public diagnostic differs from installed output"
    grep -Fq "missing declared capabilities" "$WORK_DIR/${case_name}.direct" ||
        fail "$case_name lost the declared-vs-used diagnostic"
    grep -Fq "$function_name" "$WORK_DIR/${case_name}.direct" ||
        fail "$case_name diagnostic lost callable identity"
    grep -Fq "$missing_capability" "$WORK_DIR/${case_name}.direct" ||
        fail "$case_name diagnostic lost missing capability identity"
    json_suffix "$WORK_DIR/${case_name}.direct" >"$WORK_DIR/${case_name}.direct.json"
    normalize "$WORK_DIR/${case_name}.direct.json" "$WORK_DIR/${case_name}.direct.norm"
    normalize "$WORK_DIR/${case_name}.native" "$WORK_DIR/${case_name}.native.norm"
    cmp -s "$WORK_DIR/${case_name}.direct.norm" "$WORK_DIR/${case_name}.native.norm" ||
        fail "$case_name installed manifest payload differs from native oracle"
done <<'CASES'
manifest_violation|GetTimestamp|clock
manifest_interproc|entry|clock
file_handle_write_violation|StreamWrite|io_write
file_handle_read_violation|StreamRead|io_read
file_handle_dynamic_mode_violation|OpenDynamic|io_write
file_handle_read_write_violation|OpenReadWrite|io_read
file_exists_violation|ProbeExists|io_read
print_violation|EmitProtocol|io_write
CASES

set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
    PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" \
    --capability-manifest tests/capability/manifest_clean.pgy) \
    >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
missing_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE && \
    "$PGY" --capability-manifest tests/capability/manifest_clean.pgy --verbose) \
    >"$WORK_DIR/options.out" 2>"$WORK_DIR/options.err"
options_rc=$?
(cd "$ROOT_DIR" && "$SELF_DRIVER" "$DIRECT_MODE") \
    >"$WORK_DIR/arity.out" 2>"$WORK_DIR/arity.err"
arity_rc=$?
set -e

[[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
    fail "missing installed sibling silently entered native capability analysis"
grep -Fq "self-host driver is unavailable" "$WORK_DIR/missing.err" ||
    fail "missing sibling lost the installed-boundary diagnostic"
! grep -Fq "[pipeline timing]" "$WORK_DIR/missing.err" ||
    fail "missing sibling retried the native pipeline"
[[ "$options_rc" -ne 0 && ! -s "$WORK_DIR/options.out" ]] ||
    fail "unsupported capability-manifest options entered a compiler path"
grep -Fq -- "--capability-manifest options are outside" \
    "$WORK_DIR/options.err" || fail "unsupported options lost selector diagnostic"
[[ "$arity_rc" -ne 0 ]] || fail "installed capability mode accepted no source"
grep -Fq "installed capability manifest mode requires one source path" \
    "$WORK_DIR/arity.out" "$WORK_DIR/arity.err" ||
    fail "installed arity lost its typed diagnostic"

require_text "$ROOT_DIR/src/pgy_driver.c" \
    'driver_self_host_source_stdout_mode(&flags)'
require_text "$ROOT_DIR/src/pgy_driver.c" \
    'return driver_run_self_host_source_stdout('
require_text "$ROOT_DIR/src/compiler/self_host_driver.c" \
    '"--emit-capability-manifest-verified"'
require_text "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy" \
    'DriverCliSourceCapabilityManifestStdout(String)'
require_text "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_read_execution_owner.pgy" \
    'CompileSourceCapabilityManifestVerified(source_path)'
require_text "$ROOT_DIR/src/self_hosted/compiler/capability_manifest_owner.pgy" \
    'SemanticAstCapabilityFactsFromAdmittedBody('
if grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/self_host_driver.c"; then
    fail "installed sibling launcher regained a native fallback"
fi

echo "[self-host-public-capability-manifest] installed semantic facts own public output and fail closed"
