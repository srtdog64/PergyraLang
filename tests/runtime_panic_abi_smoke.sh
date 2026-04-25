#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CC_BIN="${CC:-cc}"
PYTHON_BIN="${PYTHON_BIN:-}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-panic-abi.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for runtime panic ABI smoke" >&2
        exit 1
    fi
fi

EXE_EXT=""
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) EXE_EXT=".exe" ;;
esac

compile_case() {
    local name="$1"
    local source="$2"
    local bin="$WORK_DIR/$name$EXE_EXT"

    printf '%s\n' "$source" > "$WORK_DIR/$name.c"
    "$CC_BIN" -std=c11 -O0 -g -Isrc "$WORK_DIR/$name.c" -o "$bin" -pthread -lm
    printf '%s\n' "$bin"
}

expect_panic() {
    local name="$1"
    local bin="$2"
    local expected_class="$3"
    local stderr_path="$WORK_DIR/$name.stderr"
    local stdout_path="$WORK_DIR/$name.stdout"
    "$PYTHON_BIN" - "$name" "$bin" "$stdout_path" "$stderr_path" "$expected_class" <<'PY'
import pathlib
import subprocess
import sys

name, bin_path, stdout_path, stderr_path, expected_class = sys.argv[1:6]
proc = subprocess.run([bin_path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
pathlib.Path(stdout_path).write_bytes(proc.stdout)
pathlib.Path(stderr_path).write_bytes(proc.stderr)
stderr = proc.stderr.decode("utf-8", errors="replace")

if proc.returncode == 0:
    sys.stderr.write(f"runtime-panic-abi: {name} unexpectedly exited 0\n")
    sys.stderr.write(stderr)
    raise SystemExit(1)
if "[PGY PANIC]" not in stderr:
    sys.stderr.write(f"runtime-panic-abi: {name} missing panic prefix\n")
    sys.stderr.write(stderr)
    raise SystemExit(1)
needle = f"class={expected_class}"
if needle not in stderr:
    sys.stderr.write(f"runtime-panic-abi: {name} missing panic class {expected_class}\n")
    sys.stderr.write(stderr)
    raise SystemExit(1)
PY
}

inline_released_bin="$(compile_case inline_released_slot '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    pgy_release_Int(&slot);
    (void)pgy_read_Int(&slot);
    return 0;
}
')"

inline_invalid_token_bin="$(compile_case inline_invalid_secure_token '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int bad = token;
    bad.id ^= 1u;
    (void)pgy_secure_read_Int(&slot, &bad);
    return 0;
}
')"

inline_double_release_bin="$(compile_case inline_double_release '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    pgy_release_Int(&slot);
    pgy_release_Int(&slot);
    return 0;
}
')"

inline_secure_double_release_bin="$(compile_case inline_secure_double_release '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    pgy_secure_release_Int(&slot, &token);
    pgy_secure_release_Int(&slot, &token);
    return 0;
}
')"

inline_array_oob_bin="$(compile_case inline_array_oob '
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyArray_Int arr = pgy_array_new_Int(0);
    (void)pgy_array_get_Int(&arr, 0);
    return 0;
}
')"

inline_authority_mismatch_bin="$(compile_case inline_authority_mismatch '
#include "runtime/pgy_runtime.h"
int main(void) {
    PGY_ZONE_AUTHORITY_CHECK(NULL, NULL, "BattleZone", "owner");
    return 0;
}
')"

inline_oom_bin="$(compile_case inline_oom '
#include <stdint.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    (void)pgy_alloc(NULL, SIZE_MAX, _Alignof(max_align_t));
    return 0;
}
')"

inline_div_zero_bin="$(compile_case inline_div_zero '
#include "runtime/pgy_runtime.h"
int main(void) {
    (void)pgy_checked_div_i32_export(10, 0);
    return 0;
}
')"

inline_device_released_bin="$(compile_case inline_device_released '
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    (void)pgy_device_read_Int(&slot);
    return 0;
}
')"

inline_device_double_release_bin="$(compile_case inline_device_double_release '
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    pgy_release_device_Int(&slot);
    return 0;
}
')"

exported_released_bin="$(compile_case exported_released_slot '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    pgy_release_Int(&slot);
    (void)pgy_read_Int(&slot);
    return 0;
}
')"

exported_double_release_bin="$(compile_case exported_double_release '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    pgy_release_Int(&slot);
    pgy_release_Int(&slot);
    return 0;
}
')"

exported_invalid_token_bin="$(compile_case exported_invalid_secure_token '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int bad = token;
    bad.id ^= 1u;
    (void)pgy_secure_read_Int(&slot, &bad);
    return 0;
}
')"

exported_secure_double_release_bin="$(compile_case exported_secure_double_release '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    pgy_secure_release_Int(&slot, &token);
    pgy_secure_release_Int(&slot, &token);
    return 0;
}
')"

exported_authority_mismatch_bin="$(compile_case exported_authority_mismatch '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    pgy_zone_authority_check_export(NULL, NULL, "BattleZone", "owner");
    return 0;
}
')"

exported_array_oom_bin="$(compile_case exported_array_oom '
#define PGY_LLVM_ENABLED 1
#include <stdint.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    (void)pgy_array_new_Int((SIZE_MAX / sizeof(int32_t)) + 1u);
    return 0;
}
')"

exported_array_slice_oob_bin="$(compile_case exported_array_slice_oob '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyArray_Int arr = pgy_array_new_Int(0);
    (void)pgy_array_slice_Int(&arr, 1, 1);
    return 0;
}
')"

exported_div_zero_bin="$(compile_case exported_div_zero '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    (void)pgy_checked_mod_i64_export(10, 0);
    return 0;
}
')"

exported_device_released_bin="$(compile_case exported_device_released '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    (void)pgy_device_read_Int(&slot);
    return 0;
}
')"

exported_device_double_release_bin="$(compile_case exported_device_double_release '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    pgy_release_device_Int(&slot);
    return 0;
}
')"

expect_panic inline_released_slot "$inline_released_bin" "released-slot"
expect_panic inline_invalid_secure_token "$inline_invalid_token_bin" "invalid-secure-token"
expect_panic inline_double_release "$inline_double_release_bin" "double-release"
expect_panic inline_secure_double_release "$inline_secure_double_release_bin" "double-release"
expect_panic inline_array_oob "$inline_array_oob_bin" "out-of-bounds"
expect_panic inline_authority_mismatch "$inline_authority_mismatch_bin" "authority-mismatch"
expect_panic inline_oom "$inline_oom_bin" "oom"
expect_panic inline_div_zero "$inline_div_zero_bin" "divide-by-zero"
expect_panic inline_device_released "$inline_device_released_bin" "released-slot"
expect_panic inline_device_double_release "$inline_device_double_release_bin" "double-release"
expect_panic exported_released_slot "$exported_released_bin" "released-slot"
expect_panic exported_invalid_secure_token "$exported_invalid_token_bin" "invalid-secure-token"
expect_panic exported_double_release "$exported_double_release_bin" "double-release"
expect_panic exported_secure_double_release "$exported_secure_double_release_bin" "double-release"
expect_panic exported_authority_mismatch "$exported_authority_mismatch_bin" "authority-mismatch"
expect_panic exported_array_oom "$exported_array_oom_bin" "oom"
expect_panic exported_array_slice_oob "$exported_array_slice_oob_bin" "out-of-bounds"
expect_panic exported_div_zero "$exported_div_zero_bin" "divide-by-zero"
expect_panic exported_device_released "$exported_device_released_bin" "released-slot"
expect_panic exported_device_double_release "$exported_device_double_release_bin" "double-release"

echo "[runtime-panic-abi] inline and exported panic classes are executable"
