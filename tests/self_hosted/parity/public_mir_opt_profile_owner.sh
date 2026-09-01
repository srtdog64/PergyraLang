#!/usr/bin/env bash
# MIR read projections are Pergyra-owned. Backend optimization profile may not
# change their typed request, bytes, or execution lane.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public_mir_opt_profile"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="examples/hello.pgy"
SOURCE_CANONICAL="$ROOT_DIR/$SOURCE"
DIAGNOSTIC_OWNER="$ROOT_DIR/src/compiler/self_host_mir_diagnostic_stdout_owner.c"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"

fail() {
    echo "[self-host-mir-opt] $*" >&2
    exit 1
}

run_public() {
    local case_name="$1"
    local profile="$2"
    local opt=()
    local extra=()
    [[ "$profile" == dev ]] && opt=(--opt=dev)
    [[ "$#" -ge 3 ]] && extra=("$3")
    case "$case_name" in
        mir-text)
            (cd "$ROOT_DIR" && "$PGY" --mir "$SOURCE" \
                "${opt[@]}" "${extra[@]}") ;;
        mir-json-diagnostic)
            (cd "$ROOT_DIR" && "$PGY" --mir "$SOURCE" \
                --error-format=json "${opt[@]}" "${extra[@]}") ;;
        mir-json)
            (cd "$ROOT_DIR" && "$PGY" --mir-json "$SOURCE" \
                "${opt[@]}" "${extra[@]}") ;;
        *) fail "unknown case: $case_name" ;;
    esac
}

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "installed compiler pair is missing"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

while IFS='|' read -r case_name direct_mode; do
    (unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE; \
        run_public "$case_name" release) \
        >"$WORK_DIR/$case_name.release" 2>"$WORK_DIR/$case_name.release.err"
    (unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE; \
        run_public "$case_name" dev) \
        >"$WORK_DIR/$case_name.dev" 2>"$WORK_DIR/$case_name.dev.err"
    (cd "$ROOT_DIR" && unset PGY_IO_ROOT && PGY_IO_ALLOW_ABSOLUTE=1 \
        "$SELF_DRIVER" "$direct_mode" "$SOURCE_CANONICAL") \
        >"$WORK_DIR/$case_name.direct" 2>"$WORK_DIR/$case_name.direct.err"
    cmp -s "$WORK_DIR/$case_name.release" "$WORK_DIR/$case_name.dev" ||
        fail "$case_name changed bytes under --opt=dev"
    cmp -s "$WORK_DIR/$case_name.direct" "$WORK_DIR/$case_name.dev" ||
        fail "$case_name dev request bypassed its installed owner"
    [[ ! -s "$WORK_DIR/$case_name.dev.err" ]] ||
        fail "$case_name installed dev request wrote unexpected stderr"
done <<'CASES'
mir-text|--emit-mir-diagnostic-verified
mir-json-diagnostic|--emit-mir-json-diagnostic-verified
mir-json|--emit-mir-json-verified
CASES

suffix=""
[[ "$PGY" == *.exe ]] && suffix=".exe"
COUNT_DRIVER="$WORK_DIR/counting-driver$suffix"
COUNT_DRIVER_FOR_LAUNCHER="$WORK_REL/counting-driver$suffix"
COUNT_FILE="$WORK_DIR/count.txt"
COUNT_FILE_FOR_DRIVER="$COUNT_FILE"
[[ "$suffix" == ".exe" ]] &&
    COUNT_FILE_FOR_DRIVER="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_mir_read_driver.c" \
    -o "$COUNT_DRIVER"

while IFS='|' read -r case_name direct_mode; do
    PGY_SELF_DRIVER_BIN="$COUNT_DRIVER_FOR_LAUNCHER" \
        PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        PGY_DEBUG_PIPELINE_TIMING=1 run_public "$case_name" dev \
        >"$WORK_DIR/counting.out" 2>"$WORK_DIR/counting.err"
    grep -Fxq "mir-read-shim:$direct_mode" "$WORK_DIR/counting.out" ||
        fail "$case_name did not invoke its typed installed mode"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/counting.err" ||
        fail "$case_name re-entered the native pipeline"
done <<'CASES'
mir-text|--emit-mir-diagnostic-verified
mir-json-diagnostic|--emit-mir-json-diagnostic-verified
mir-json|--emit-mir-json-verified
CASES
[[ "$(wc -l <"$COUNT_FILE" | tr -d ' ')" == 3 ]] ||
    fail "dev-profile MIR reads did not invoke one child per request"

for case_name in mir-text mir-json-diagnostic mir-json; do
    set +e
    PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
        PGY_DEBUG_PIPELINE_TIMING=1 run_public "$case_name" dev \
        >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
    missing_rc=$?
    (unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE; \
        run_public "$case_name" dev --verbose) \
        >"$WORK_DIR/verbose.out" 2>"$WORK_DIR/verbose.err"
    verbose_rc=$?
    set -e
    [[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
        fail "$case_name missing child published output"
    grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
        fail "$case_name missing child lost its boundary diagnostic"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/missing.err" ||
        fail "$case_name missing child retried native compilation"
    [[ "$verbose_rc" -ne 0 && ! -s "$WORK_DIR/verbose.out" ]] ||
        fail "$case_name accepted unrelated --verbose"
    grep -Fq 'outside the installed self-host driver contract' \
        "$WORK_DIR/verbose.err" || fail "$case_name lost unsupported-option diagnostic"
done

diagnostic_body="$(sed -n \
    '/driver_self_host_mir_diagnostic_request_supported(/,/^}/p' \
    "$DIAGNOSTIC_OWNER")"
json_body="$(sed -n \
    '/driver_self_host_mir_json_request_supported(/,/^}/p' "$SELECTION_OWNER")"
! grep -Fq 'opt_profile' <<<"$diagnostic_body$json_body" ||
    fail "C admission still assigns optimization semantics to a MIR read"
for variant in DriverCliSourceMirDiagnosticStdout \
    DriverCliSourceMirJsonDiagnosticStdout DriverCliSourceMirStdout; do
    grep -Fq "$variant(String)," "$REQUEST_OWNER" ||
        fail "$variant lost its one-path request shape"
    ! grep -Fq "$variant(String," "$REQUEST_OWNER" ||
        fail "$variant gained a second policy input"
done
! grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/self_host_driver.c" ||
    fail "installed sibling launcher regained a native fallback"

echo "[self-host-mir-opt] dev profile is Pergyra-owned and optimization-neutral"
