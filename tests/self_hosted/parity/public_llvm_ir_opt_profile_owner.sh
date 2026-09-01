#!/usr/bin/env bash
# Emitted LLVM IR is profile-neutral; final binary compilation remains
# profile-sensitive at its separate toolchain boundary.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${CC:-cc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/public_llvm_ir_opt_profile"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE="src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
SELECTION_OWNER="$ROOT_DIR/src/compiler/driver_self_host_llvm_selection_owner.c"
REQUEST_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_cli_request_owner.pgy"

fail() { echo "[self-host-llvm-ir-opt] $*" >&2; exit 1; }

[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || fail "installed compiler pair is missing"
command -v "$CC" >/dev/null || fail "missing C compiler"
command -v "$CLANG" >/dev/null || fail "missing clang"
PGY="$(cd "$(dirname "$PGY")" && pwd -P)/$(basename "$PGY")"
SELF_DRIVER="$(cd "$(dirname "$SELF_DRIVER")" && pwd -P)/$(basename "$SELF_DRIVER")"
suffix=""; installed_name="pgy-self-driver"
if [[ "$PGY" == *.exe ]]; then suffix=".exe"; installed_name="pgy-self-driver.exe"; fi
[[ "$SELF_DRIVER" == "$(dirname "$PGY")/$installed_name" ]] ||
    fail "self-host driver is not installed beside the public launcher"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/counting-install" "$WORK_DIR/missing-install"

(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE" --emit-llvm) >"$WORK_DIR/release.stdout.ll" \
    2>"$WORK_DIR/release.stdout.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE" --emit-llvm --opt=dev) >"$WORK_DIR/dev.stdout.ll" \
    2>"$WORK_DIR/dev.stdout.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE" --emit-llvm -o "$WORK_REL/release.file.ll") \
    >"$WORK_DIR/release.file.out" 2>"$WORK_DIR/release.file.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN PGY_NATIVE_PIPELINE &&
    "$PGY" "$SOURCE" --emit-llvm --opt=dev -o "$WORK_REL/dev.file.ll") \
    >"$WORK_DIR/dev.file.out" 2>"$WORK_DIR/dev.file.err"
cmp -s "$WORK_DIR/release.stdout.ll" "$WORK_DIR/dev.stdout.ll" ||
    fail "stdout LLVM IR changed under --opt=dev"
cmp -s "$WORK_DIR/release.file.ll" "$WORK_DIR/dev.file.ll" ||
    fail "file LLVM IR changed under --opt=dev"
cmp -s "$WORK_DIR/dev.stdout.ll" "$WORK_DIR/dev.file.ll" ||
    fail "dev stdout/file LLVM IR forms differ"
[[ ! -s "$WORK_DIR/dev.stdout.err" && ! -s "$WORK_DIR/dev.file.err" ]] ||
    fail "dev LLVM IR publication wrote unexpected stderr"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE" --emit-llvm \
    --opt=release) >"$WORK_DIR/native.release.ll" 2>"$WORK_DIR/native.release.err"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE" --emit-llvm \
    --opt=dev) >"$WORK_DIR/native.dev.ll" 2>"$WORK_DIR/native.dev.err"
cmp -s "$WORK_DIR/native.release.ll" "$WORK_DIR/native.dev.ll" ||
    fail "native LLVM IR contract became profile-sensitive"

"$CLANG" "$WORK_DIR/dev.stdout.ll" -o "$WORK_DIR/dev-program$suffix" \
    2>"$WORK_DIR/clang.err"
"$WORK_DIR/dev-program$suffix" | tr -d '\r' >"$WORK_DIR/program.out"
printf '7\n11\n5\n' >"$WORK_DIR/expected.out"
cmp -s "$WORK_DIR/expected.out" "$WORK_DIR/program.out" ||
    fail "dev LLVM IR did not execute exact 7/11/5"

cp "$PGY" "$WORK_DIR/counting-install/pgy$suffix"
"$CC" -std=c11 -Wall -Wextra -Werror \
    "$ROOT_DIR/tests/self_hosted/parity/fixture/counting_self_host_llvm_driver.c" \
    -o "$WORK_DIR/counting-install/$installed_name"
COUNT_FILE="$WORK_DIR/count.txt"
count_for_driver="$(pgy_path_for_compiler "$PGY" "$COUNT_FILE")"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm --opt=dev \
    -o "$WORK_REL/counting.file.ll") >"$WORK_DIR/counting.file.out" \
    2>"$WORK_DIR/counting.file.err"
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; PGY_DEBUG_PIPELINE_TIMING=1 \
    PGY_SELF_DRIVER_COUNT_FILE="$count_for_driver" \
    "$WORK_DIR/counting-install/pgy$suffix" "$SOURCE" --emit-llvm --opt=dev) \
    >"$WORK_DIR/counting.stdout.ll" 2>"$WORK_DIR/counting.stdout.err"
printf 'intent\nintent\n' >"$WORK_DIR/count.expected"
cmp -s "$WORK_DIR/count.expected" "$COUNT_FILE" ||
    fail "dev file/stdout did not invoke one intent each"
[[ -s "$WORK_DIR/counting.file.ll" && -s "$WORK_DIR/counting.stdout.ll" ]] ||
    fail "counting owner published no LLVM IR"
! grep -Fq '[pipeline timing]' "$WORK_DIR/counting.file.err" \
    "$WORK_DIR/counting.stdout.err" || fail "dev LLVM IR re-entered native compilation"

cp "$PGY" "$WORK_DIR/missing-install/pgy$suffix"
printf 'stale\n' >"$WORK_DIR/missing.file.ll"
set +e
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; PGY_DEBUG_PIPELINE_TIMING=1 \
    "$WORK_DIR/missing-install/pgy$suffix" "$SOURCE" --emit-llvm --opt=dev \
    -o "$WORK_REL/missing.file.ll") >"$WORK_DIR/missing.file.out" \
    2>"$WORK_DIR/missing.file.err"
missing_file_rc=$?
(cd "$ROOT_DIR" && unset PGY_SELF_DRIVER_BIN; PGY_DEBUG_PIPELINE_TIMING=1 \
    "$WORK_DIR/missing-install/pgy$suffix" "$SOURCE" --emit-llvm --opt=dev) \
    >"$WORK_DIR/missing.stdout.ll" 2>"$WORK_DIR/missing.stdout.err"
missing_stdout_rc=$?
(cd "$ROOT_DIR" && "$PGY" "$SOURCE" --emit-llvm --opt=dev --verbose) \
    >"$WORK_DIR/verbose.ll" 2>"$WORK_DIR/verbose.err"
verbose_rc=$?
set -e
[[ "$missing_file_rc" -ne 0 && ! -e "$WORK_DIR/missing.file.ll" ]] ||
    fail "missing sibling retained a stale/partial dev file"
[[ "$missing_stdout_rc" -ne 0 && ! -s "$WORK_DIR/missing.stdout.ll" ]] ||
    fail "missing sibling published dev stdout"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.file.err" ||
    fail "missing file sibling lost its diagnostic"
grep -Fq 'self-host driver is unavailable' "$WORK_DIR/missing.stdout.err" ||
    fail "missing stdout sibling lost its diagnostic"
! grep -Fq '[pipeline timing]' "$WORK_DIR/missing.file.err" \
    "$WORK_DIR/missing.stdout.err" || fail "missing sibling retried native LLVM"
[[ "$verbose_rc" -ne 0 && ! -s "$WORK_DIR/verbose.ll" ]] ||
    fail "dev LLVM stdout accepted --verbose"

selector_body="$(sed -n '/driver_self_host_llvm_ir_request_supported(/,/^}/p' \
    "$SELECTION_OWNER")"
! grep -Fq 'opt_profile' <<<"$selector_body" ||
    fail "C selector still assigns optimization semantics to emitted LLVM IR"
grep -Fq 'DriverCliSourceLlvmArtifact(String, String, Bool),' "$REQUEST_OWNER" ||
    fail "typed source-LLVM request shape drifted"
! grep -Fq 'DriverCliSourceLlvmArtifact(String, String, Bool,' "$REQUEST_OWNER" ||
    fail "typed source-LLVM request gained profile carriage"
grep -Fq 'opt_profile == PGY_OPT_RELEASE ? "-O3" : "-O0"' \
    "$ROOT_DIR/src/compiler/compiler_self_host_artifact.c" ||
    fail "final executable compile/link lost profile-sensitive optimization"

echo "[self-host-llvm-ir-opt] dev file/stdout publication is installed and profile-neutral"
