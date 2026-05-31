#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY_WINDOWS_PS_PATH_PREFIX="$(pgy_windows_powershell_path_prefix)"
CC_BIN="${CC:-cc}"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-panic-abi.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

EXE_EXT=""
WINDOWS_SHELL=0
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*)
        EXE_EXT=".exe"
        WINDOWS_SHELL=1
        ;;
esac

compile_failure() {
    local name="$1"
    if [[ "$WINDOWS_SHELL" == "1" && -z "${CI:-}" && "${PGY_RUNTIME_PANIC_ABI_ALLOW_LOCAL_SKIP:-0}" == "1" ]]; then
        echo "[runtime-panic-abi] SKIP local Windows shell compiler failed for $name; explicit local skip enabled" >&2
        touch "$WORK_DIR/compile_skipped"
        printf '%s\n' "$WORK_DIR/__compile_skipped__"
        return 0
    fi
    echo "runtime-panic-abi: failed to compile $name with $CC_BIN" >&2
    exit 1
}

if grep -q "PGY_SECURE_SLOT_DEFINE_RELEASE" "$ROOT_DIR/src/runtime/pgy_runtime_builtin_storage_inline.h"; then
    echo "runtime panic ABI smoke: SecureSlot release-mode macro must not exist" >&2
    exit 1
fi

compile_case() {
    local name="$1"
    local source="$2"
    local bin="$WORK_DIR/$name$EXE_EXT"
    local cc_source="$WORK_DIR/$name.c"
    local cc_bin="$bin"
    local cc_include="src"
    local cc_source_win=""
    local cc_bin_win=""
    local cc_include_win=""
    local is_windows_shell=0
    local cc_exe_win="$CC_BIN"

    if [[ -f "$WORK_DIR/compile_skipped" ]]; then
        printf '%s\n' "$WORK_DIR/__compile_skipped__"
        return 0
    fi

    printf '%s\n' "$source" > "$cc_source"
    case "$(uname -s 2>/dev/null || echo unknown)" in
        MINGW*|MSYS*|CYGWIN*)
            is_windows_shell=1
            cc_source_win="$(pgy_path_for_windows_tool "$cc_source")"
            cc_bin_win="$(pgy_path_for_windows_tool "$cc_bin")"
            cc_include_win="$(pgy_path_for_windows_tool "$ROOT_DIR/src")"
            if command -v "$CC_BIN" >/dev/null 2>&1; then
                cc_exe_win="$(pgy_path_for_windows_tool "$(command -v "$CC_BIN")")"
            fi
            ;;
    esac
    if ! "$CC_BIN" -std=c11 -O0 -g -I"$cc_include" "$cc_source" -o "$cc_bin" -pthread -lm; then
        if [[ "$is_windows_shell" == "1" ]] && command -v powershell.exe >/dev/null 2>&1; then
            if ! powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \
                "\$ErrorActionPreference='Stop'; \$env:PATH='${PGY_WINDOWS_PS_PATH_PREFIX}' + \$env:PATH; & '$cc_exe_win' -std=c11 -O0 -g '-I$cc_include_win' '$cc_source_win' -o '$cc_bin_win' -pthread -lm; exit \$LASTEXITCODE"; then
                compile_failure "$name"
            fi
        else
            compile_failure "$name"
        fi
    fi
    if [[ -f "$WORK_DIR/compile_skipped" ]]; then
        printf '%s\n' "$WORK_DIR/__compile_skipped__"
        return 0
    fi
    if [[ ! -x "$bin" ]]; then
        echo "runtime-panic-abi: compiler did not produce executable for $name: $bin" >&2
        exit 1
    fi
    printf '%s\n' "$bin"
}

expect_panic() {
    local name="$1"
    local bin="$2"
    local expected_class="$3"
    local stderr_path="$WORK_DIR/$name.stderr"
    local stdout_path="$WORK_DIR/$name.stdout"
    local rc

    set +e
    "$bin" >"$stdout_path" 2>"$stderr_path"
    rc=$?
    set -e

    if [[ "$rc" -eq 0 ]]; then
        echo "runtime-panic-abi: $name unexpectedly exited 0" >&2
        cat "$stderr_path" >&2
        exit 1
    fi
    if ! grep -Fq "[PGY PANIC]" "$stderr_path"; then
        echo "runtime-panic-abi: $name missing panic prefix" >&2
        cat "$stderr_path" >&2
        exit 1
    fi
    if ! grep -Fq "class=$expected_class" "$stderr_path"; then
        echo "runtime-panic-abi: $name missing panic class $expected_class" >&2
        cat "$stderr_path" >&2
        exit 1
    fi
}

expect_success_stdout_contains() {
    local name="$1"
    local bin="$2"
    local expected="$3"
    local stderr_path="$WORK_DIR/$name.stderr"
    local stdout_path="$WORK_DIR/$name.stdout"

    "$bin" >"$stdout_path" 2>"$stderr_path"
    if ! grep -Fq "$expected" "$stdout_path"; then
        echo "runtime-panic-abi: $name missing stdout term $expected" >&2
        cat "$stdout_path" >&2
        cat "$stderr_path" >&2
        exit 1
    fi
}

inline_try_slot_bin="$(compile_case inline_try_slot_status '
#define PGY_SAFE_SLOTS 1
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_read_Int(&slot, &out)))
        return 1;
    if (pgy_try_release_Int(&slot) != PGY_RUNTIME_SLOT_STATUS_OK)
        return 2;
    PgyRuntimeSlotResult_Int result = pgy_try_read_result_Int(&slot);
    if (result.tag != PGY_RUNTIME_SLOT_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", result.err.stage, result.err.operation, result.err.name);
    return result.err.status == PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT ? 0 : 4;
}
')"

inline_try_device_slot_bin="$(compile_case inline_try_device_slot_status '
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_device_read_Int(&slot, &out)))
        return 1;
    if (pgy_try_release_device_Int(&slot) != PGY_RUNTIME_SLOT_STATUS_OK)
        return 2;
    PgyRuntimeSlotResult_Int result = pgy_try_device_read_result_Int(&slot);
    if (result.tag != PGY_RUNTIME_SLOT_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", result.err.stage, result.err.operation, result.err.name);
    return result.err.status == PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT ? 0 : 4;
}
')"

inline_remote_released_device_result_bin="$(compile_case inline_remote_released_device_result '
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    PgyTaskHandle pending = pgy_submit_device_read_Int(&slot);
    PgyResult_Int result = pgy_await_result_take(pending, Int, int32_t);
    if (result.tag != PgyResultErr)
        return 1;
    printf("%s\n", result.err);
    return 0;
}
')"

inline_try_secure_slot_bin="$(compile_case inline_try_secure_slot_status '
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_secure_read_Int(&slot, &token, &out)))
        return 1;
    PgyToken_Int bad = token;
    bad.id ^= 1u;
    PgyRuntimeSlotResult_Int read_result = pgy_try_secure_read_result_Int(&slot, &bad);
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_ERR ||
        read_result.err.status != PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN)
        return 2;
    PgyToken_Int no_read = token;
    no_read.can_read = false;
    read_result = pgy_try_secure_read_result_Int(&slot, &no_read);
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_ERR ||
        read_result.err.status != PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ)
        return 3;
    PgyToken_Int no_write = token;
    no_write.can_write = false;
    PgyRuntimeSlotStatus status = pgy_try_secure_write_Int(&slot, 9, &no_write);
    printf("%s:%s:%s\n", read_result.err.stage, read_result.err.operation, read_result.err.name);
    printf("%s\n", pgy_runtime_slot_status_name(status));
    return status == PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE ? 0 : 4;
}
')"

inline_try_io_result_bin="$(compile_case inline_try_io_result '
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyRuntimeIoIntResult open_result =
        pgy_try_file_open_result("missing-dir/nope.txt", "r");
    if (open_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 1;
    printf("%s:%s:%s\n", open_result.err.stage,
           open_result.err.operation, open_result.err.name);

    PgyRuntimeIoStringResult read_result =
        pgy_try_read_file_result("missing-dir/nope.txt");
    if (read_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 2;
    printf("%s:%s:%s\n", read_result.err.stage,
           read_result.err.operation, read_result.err.name);

    read_result = pgy_try_file_read_result(-77);
    if (read_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", read_result.err.stage,
           read_result.err.operation, read_result.err.name);

    PgyRuntimeIoVoidResult write_result =
        pgy_try_file_write_result(-77, "x");
    if (write_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 4;
    printf("%s:%s:%s\n", write_result.err.stage,
           write_result.err.operation, write_result.err.name);

    write_result = pgy_try_write_file_result("missing-dir/nope.txt", "x");
    if (write_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 5;
    printf("%s:%s:%s\n", write_result.err.stage,
           write_result.err.operation, write_result.err.name);
    return write_result.err.status == PGY_RUNTIME_IO_STATUS_OPEN_FAILED
        ? 0 : 6;
}
')"

inline_channel_result_bin="$(compile_case inline_channel_result '
#include <stdio.h>
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyChannel_Int ch;
    pgy_channel_init_Int(&ch, 1);
    PgyRuntimeChannelIntResult empty =
        pgy_channel_try_recv_result_Int(&ch);
    if (empty.tag != PGY_RUNTIME_CHANNEL_RESULT_ERR ||
        empty.err.status != PGY_RUNTIME_CHANNEL_STATUS_EMPTY)
        return 1;
    if (!pgy_channel_send_Int(&ch, 42))
        return 2;
    PgyRuntimeChannelIntResult value =
        pgy_channel_recv_result_Int(&ch);
    if (value.tag != PGY_RUNTIME_CHANNEL_RESULT_OK || value.ok != 42)
        return 3;
    pgy_channel_close_Int(&ch);
    PgyRuntimeChannelIntResult closed =
        pgy_channel_recv_result_Int(&ch);
    printf("%s:%s:%s\n", closed.err.stage,
           closed.err.operation, closed.err.name);
    pgy_channel_destroy_Int(&ch);
    return closed.tag == PGY_RUNTIME_CHANNEL_RESULT_ERR &&
        closed.err.status == PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY ? 0 : 4;
}
')"

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

inline_forged_token_read_bin="$(compile_case inline_forged_secure_token_read '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    (void)pgy_secure_read_Int(&slot, &forged);
    return 0;
}
')"

inline_forged_token_write_bin="$(compile_case inline_forged_secure_token_write '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    pgy_secure_write_Int(&slot, 7, &forged);
    return 0;
}
')"

inline_forged_token_release_bin="$(compile_case inline_forged_secure_token_release '
#define PGY_SAFE_SLOTS 1
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    pgy_secure_release_Int(&slot, &forged);
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

inline_release_mode_secure_token_denied_bin="$(compile_case inline_release_mode_secure_token_denied '
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    token.can_read = false;
    (void)pgy_secure_read_Int(&slot, &token);
    return 0;
}
')"

inline_release_mode_secure_released_bin="$(compile_case inline_release_mode_secure_released '
#include "runtime/pgy_runtime.h"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    pgy_secure_release_Int(&slot, &token);
    (void)pgy_secure_read_Int(&slot, &token);
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

exported_forged_token_read_bin="$(compile_case exported_forged_secure_token_read '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    (void)pgy_secure_read_Int(&slot, &forged);
    return 0;
}
')"

exported_forged_token_write_bin="$(compile_case exported_forged_secure_token_write '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    pgy_secure_write_Int(&slot, 7, &forged);
    return 0;
}
')"

exported_forged_token_release_bin="$(compile_case exported_forged_secure_token_release '
#define PGY_LLVM_ENABLED 1
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    PgyToken_Int forged = {0};
    forged.can_read = true;
    forged.can_write = true;
    pgy_secure_release_Int(&slot, &forged);
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

exported_try_slot_bin="$(compile_case exported_try_slot_status '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgySlot_Int slot = pgy_claim_Int();
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_read_Int(&slot, &out)))
        return 1;
    if (pgy_try_release_Int(&slot) != PGY_RUNTIME_SLOT_STATUS_OK)
        return 2;
    PgyRuntimeSlotResult_Int result = pgy_try_read_result_Int(&slot);
    if (result.tag != PGY_RUNTIME_SLOT_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", result.err.stage, result.err.operation, result.err.name);
    return result.err.status == PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT ? 0 : 4;
}
')"

exported_try_device_slot_bin="$(compile_case exported_try_device_slot_status '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_device_read_Int(&slot, &out)))
        return 1;
    if (pgy_try_release_device_Int(&slot) != PGY_RUNTIME_SLOT_STATUS_OK)
        return 2;
    PgyRuntimeSlotResult_Int result = pgy_try_device_read_result_Int(&slot);
    if (result.tag != PGY_RUNTIME_SLOT_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", result.err.stage, result.err.operation, result.err.name);
    return result.err.status == PGY_RUNTIME_SLOT_STATUS_RELEASED_SLOT ? 0 : 4;
}
')"

exported_remote_released_device_result_bin="$(compile_case exported_remote_released_device_result '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyDeviceSlot_Int slot = pgy_claim_device_Int();
    pgy_release_device_Int(&slot);
    PgyTaskHandle pending = pgy_submit_device_read_Int(&slot);
    void *raw = pgy_await(pending);
    if (raw != NULL) {
        free(raw);
        return 1;
    }
    printf("%s\n", "remote operation failed");
    return 0;
}
')"

exported_try_secure_slot_bin="$(compile_case exported_try_secure_slot_status '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyToken_Int token;
    PgySecureSlot_Int slot = pgy_claim_secure_Int(&token);
    int32_t out = 0;
    if (!pgy_runtime_slot_status_ok(pgy_try_secure_read_Int(&slot, &token, &out)))
        return 1;
    PgyToken_Int bad = token;
    bad.id ^= 1u;
    PgyRuntimeSlotResult_Int read_result = pgy_try_secure_read_result_Int(&slot, &bad);
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_ERR ||
        read_result.err.status != PGY_RUNTIME_SLOT_STATUS_INVALID_TOKEN)
        return 2;
    PgyToken_Int no_read = token;
    no_read.can_read = false;
    read_result = pgy_try_secure_read_result_Int(&slot, &no_read);
    if (read_result.tag != PGY_RUNTIME_SLOT_RESULT_ERR ||
        read_result.err.status != PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_READ)
        return 3;
    PgyToken_Int no_write = token;
    no_write.can_write = false;
    PgyRuntimeSlotStatus status = pgy_try_secure_write_Int(&slot, 9, &no_write);
    printf("%s:%s:%s\n", read_result.err.stage, read_result.err.operation, read_result.err.name);
    printf("%s\n", pgy_runtime_slot_status_name(status));
    return status == PGY_RUNTIME_SLOT_STATUS_TOKEN_DENIES_WRITE ? 0 : 4;
}
')"

exported_try_io_result_bin="$(compile_case exported_try_io_result '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyRuntimeIoIntResult open_result =
        pgy_try_file_open_result("missing-dir/nope.txt", "r");
    if (open_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 1;
    printf("%s:%s:%s\n", open_result.err.stage,
           open_result.err.operation, open_result.err.name);

    PgyRuntimeIoStringResult read_result =
        pgy_try_read_file_result("missing-dir/nope.txt");
    if (read_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 2;
    printf("%s:%s:%s\n", read_result.err.stage,
           read_result.err.operation, read_result.err.name);

    read_result = pgy_try_file_read_result(-77);
    if (read_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 3;
    printf("%s:%s:%s\n", read_result.err.stage,
           read_result.err.operation, read_result.err.name);

    PgyRuntimeIoVoidResult write_result =
        pgy_try_file_write_result(-77, "x");
    if (write_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 4;
    printf("%s:%s:%s\n", write_result.err.stage,
           write_result.err.operation, write_result.err.name);

    write_result = pgy_try_write_file_result("missing-dir/nope.txt", "x");
    if (write_result.tag != PGY_RUNTIME_IO_RESULT_ERR)
        return 5;
    printf("%s:%s:%s\n", write_result.err.stage,
           write_result.err.operation, write_result.err.name);
    return write_result.err.status == PGY_RUNTIME_IO_STATUS_OPEN_FAILED
        ? 0 : 6;
}
')"

exported_channel_result_bin="$(compile_case exported_channel_result '
#define PGY_LLVM_ENABLED 1
#include <stdio.h>
#include <string.h>
#include "runtime/pgy_runtime_lib.c"
int main(void) {
    PgyChannel_String_RT ch;
    pgy_channel_init_String(&ch, 1);
    PgyRuntimeChannelStringResult empty =
        pgy_channel_try_recv_result_String(&ch);
    if (empty.tag != PGY_RUNTIME_CHANNEL_RESULT_ERR ||
        empty.err.status != PGY_RUNTIME_CHANNEL_STATUS_EMPTY)
        return 1;
    if (!pgy_channel_send_String(&ch, "ok"))
        return 2;
    PgyRuntimeChannelStringResult value =
        pgy_channel_recv_result_String(&ch);
    if (value.tag != PGY_RUNTIME_CHANNEL_RESULT_OK ||
        strcmp(value.ok, "ok") != 0)
        return 3;
    free(value.ok);
    pgy_channel_close_String(&ch);
    PgyRuntimeChannelStringResult closed =
        pgy_channel_recv_result_String(&ch);
    printf("%s:%s:%s\n", closed.err.stage,
           closed.err.operation, closed.err.name);
    pgy_channel_destroy_String(&ch);
    return closed.tag == PGY_RUNTIME_CHANNEL_RESULT_ERR &&
        closed.err.status == PGY_RUNTIME_CHANNEL_STATUS_CLOSED_EMPTY ? 0 : 4;
}
')"

if [[ -f "$WORK_DIR/compile_skipped" ]]; then
    echo "[runtime-panic-abi] SKIP local Windows shell compiler probe; explicit local skip enabled"
    exit 0
fi

expect_success_stdout_contains inline_try_slot_status "$inline_try_slot_bin" "released-slot"
expect_success_stdout_contains inline_try_device_slot_status "$inline_try_device_slot_bin" "released-slot"
expect_success_stdout_contains inline_remote_released_device_result "$inline_remote_released_device_result_bin" "remote operation failed"
expect_success_stdout_contains inline_try_secure_slot_status "$inline_try_secure_slot_bin" "token-denies-write"
expect_success_stdout_contains inline_try_io_result "$inline_try_io_result_bin" "io-boundary:file-read:invalid-handle"
expect_success_stdout_contains inline_channel_result "$inline_channel_result_bin" "channel-boundary:recv_Int:closed-empty"
expect_success_stdout_contains exported_try_slot_status "$exported_try_slot_bin" "released-slot"
expect_success_stdout_contains exported_try_device_slot_status "$exported_try_device_slot_bin" "released-slot"
expect_success_stdout_contains exported_remote_released_device_result "$exported_remote_released_device_result_bin" "remote operation failed"
expect_success_stdout_contains exported_try_secure_slot_status "$exported_try_secure_slot_bin" "token-denies-write"
expect_success_stdout_contains exported_try_io_result "$exported_try_io_result_bin" "io-boundary:file-read:invalid-handle"
expect_success_stdout_contains exported_channel_result "$exported_channel_result_bin" "channel-boundary:recv_String:closed-empty"

expect_panic inline_released_slot "$inline_released_bin" "released-slot"
expect_panic inline_invalid_secure_token "$inline_invalid_token_bin" "invalid-secure-token"
expect_panic inline_forged_secure_token_read "$inline_forged_token_read_bin" "invalid-secure-token"
expect_panic inline_forged_secure_token_write "$inline_forged_token_write_bin" "invalid-secure-token"
expect_panic inline_forged_secure_token_release "$inline_forged_token_release_bin" "invalid-secure-token"
expect_panic inline_double_release "$inline_double_release_bin" "double-release"
expect_panic inline_secure_double_release "$inline_secure_double_release_bin" "double-release"
expect_panic inline_release_mode_secure_token_denied "$inline_release_mode_secure_token_denied_bin" "invalid-secure-token"
expect_panic inline_release_mode_secure_released "$inline_release_mode_secure_released_bin" "released-slot"
expect_panic inline_array_oob "$inline_array_oob_bin" "out-of-bounds"
expect_panic inline_authority_mismatch "$inline_authority_mismatch_bin" "authority-mismatch"
expect_panic inline_oom "$inline_oom_bin" "oom"
expect_panic inline_div_zero "$inline_div_zero_bin" "divide-by-zero"
expect_panic inline_device_released "$inline_device_released_bin" "released-slot"
expect_panic inline_device_double_release "$inline_device_double_release_bin" "double-release"
expect_panic exported_released_slot "$exported_released_bin" "released-slot"
expect_panic exported_invalid_secure_token "$exported_invalid_token_bin" "invalid-secure-token"
expect_panic exported_forged_secure_token_read "$exported_forged_token_read_bin" "invalid-secure-token"
expect_panic exported_forged_secure_token_write "$exported_forged_token_write_bin" "invalid-secure-token"
expect_panic exported_forged_secure_token_release "$exported_forged_token_release_bin" "invalid-secure-token"
expect_panic exported_double_release "$exported_double_release_bin" "double-release"
expect_panic exported_secure_double_release "$exported_secure_double_release_bin" "double-release"
expect_panic exported_authority_mismatch "$exported_authority_mismatch_bin" "authority-mismatch"
expect_panic exported_array_oom "$exported_array_oom_bin" "oom"
expect_panic exported_array_slice_oob "$exported_array_slice_oob_bin" "out-of-bounds"
expect_panic exported_div_zero "$exported_div_zero_bin" "divide-by-zero"
expect_panic exported_device_released "$exported_device_released_bin" "released-slot"
expect_panic exported_device_double_release "$exported_device_double_release_bin" "double-release"

echo "[runtime-panic-abi] inline and exported panic classes are executable"
