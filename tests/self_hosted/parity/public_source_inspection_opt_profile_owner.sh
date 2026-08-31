#!/usr/bin/env bash
# Source inspection is a Pergyra-owned read boundary. Codegen optimization
# profile may not change its request identity, bytes, or execution lane.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
SELF_DRIVER="${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
CC="${CC:-cc}"
WORK_REL=".tmp/self_hosted/public_source_inspection_opt_profile"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="examples/hello.pgy"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_selection_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"

fail() {
    echo "[self-host-source-inspection-opt] $*" >&2
    exit 1
}

PGY="$(pgy_select_optional_exe_binary "$PGY")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "$SELF_DRIVER")"
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "installed compiler pair is missing"
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

while IFS='|' read -r case_name public_mode direct_mode; do
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
        "$PGY" "$public_mode" "$SOURCE") \
        >"$WORK_DIR/$case_name.release" 2>"$WORK_DIR/$case_name.release.err"
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
        "$PGY" "$public_mode" "$SOURCE" --opt=dev) \
        >"$WORK_DIR/$case_name.dev" 2>"$WORK_DIR/$case_name.dev.err"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" "$direct_mode" "$SOURCE") \
        >"$WORK_DIR/$case_name.direct" 2>"$WORK_DIR/$case_name.direct.err"
    (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$public_mode" "$SOURCE" --opt=dev) \
        >"$WORK_DIR/$case_name.native" 2>"$WORK_DIR/$case_name.native.err"
    cmp -s "$WORK_DIR/$case_name.release" "$WORK_DIR/$case_name.dev" ||
        fail "$case_name changed bytes under --opt=dev"
    cmp -s "$WORK_DIR/$case_name.direct" "$WORK_DIR/$case_name.dev" ||
        fail "$case_name dev request bypassed its installed owner"
    cmp -s "$WORK_DIR/$case_name.native" "$WORK_DIR/$case_name.dev" ||
        fail "$case_name installed/native dev stdout differs"
    [[ ! -s "$WORK_DIR/$case_name.dev.err" ]] ||
        fail "$case_name installed dev request wrote unexpected stderr"
done <<'CASES'
tokens|--tokens|--tokens
ast|--ast|--ast
dir|--dir|--emit-dir-verified
capability|--capability-manifest|--emit-capability-manifest-verified
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
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_source_stdout_driver.c" \
    -o "$COUNT_DRIVER"

while IFS='|' read -r public_mode direct_mode; do
    (cd "$ROOT_DIR" && unset PGY_NATIVE_PIPELINE &&
        PGY_SELF_DRIVER_BIN="$COUNT_DRIVER_FOR_LAUNCHER" \
        PGY_SELF_DRIVER_COUNT_FILE="$COUNT_FILE_FOR_DRIVER" \
        PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$public_mode" "$SOURCE" --opt=dev) \
        >"$WORK_DIR/counting.out" 2>"$WORK_DIR/counting.err"
    grep -Fxq "source-inspection-shim:$direct_mode" "$WORK_DIR/counting.out" ||
        fail "$public_mode did not invoke the typed installed mode"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/counting.err" ||
        fail "$public_mode re-entered the native pipeline"
done <<'CASES'
--tokens|--tokens
--ast|--ast
--dir|--emit-dir-verified
--capability-manifest|--emit-capability-manifest-verified
CASES
[[ "$(wc -l <"$COUNT_FILE" | tr -d ' ')" == "4" ]] ||
    fail "dev-profile source inspection did not invoke one child per request"

while read -r public_mode; do
    set +e
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$WORK_REL/missing-driver" \
        PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$public_mode" "$SOURCE" --opt=dev) \
        >"$WORK_DIR/missing.out" 2>"$WORK_DIR/missing.err"
    missing_rc=$?
    (cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
        "$PGY" "$public_mode" "$SOURCE" --opt=dev --verbose) \
        >"$WORK_DIR/verbose.out" 2>"$WORK_DIR/verbose.err"
    verbose_rc=$?
    set -e
    [[ "$missing_rc" -ne 0 && ! -s "$WORK_DIR/missing.out" ]] ||
        fail "$public_mode missing child published output"
    grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.err" ||
        fail "$public_mode missing child lost its boundary diagnostic"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/missing.err" ||
        fail "$public_mode missing child retried native compilation"
    [[ "$verbose_rc" -ne 0 && ! -s "$WORK_DIR/verbose.out" ]] ||
        fail "$public_mode accepted unrelated --verbose"
    grep -Fq 'outside the installed self-host driver contract' \
        "$WORK_DIR/verbose.err" || fail "$public_mode lost unsupported-option diagnostic"
done <<'MODES'
--tokens
--ast
--dir
--capability-manifest
MODES

selector_body="$(sed -n '/driver_self_host_source_stdout_mode(/,/^}/p' "$SELECTION_OWNER")"
! grep -Fq 'opt_profile' <<<"$selector_body" ||
    fail "C selector still assigns optimization semantics to source inspection"
grep -Fq 'flags->runtime_mode != RUNTIME_DEFAULT' <<<"$selector_body" ||
    fail "source inspection lost its explicit runtime boundary"
for variant in DriverCliSourceTokensStdout DriverCliSourceAstStdout \
    DriverCliSourceCapabilityManifestStdout DriverCliSourceDirStdout; do
    grep -Fq "$variant(String)," "$REQUEST_OWNER" ||
        fail "$variant lost its one-path request shape"
    ! grep -Fq "$variant(String," "$REQUEST_OWNER" ||
        fail "$variant gained a second policy input"
done
! grep -Fq 'driver_run_pipeline(' "$ROOT_DIR/src/compiler/self_host_driver.c" ||
    fail "installed sibling launcher regained a native fallback"

echo "[self-host-source-inspection-opt] dev profile is Pergyra-owned and optimization-neutral"
