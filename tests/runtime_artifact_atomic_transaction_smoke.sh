#!/usr/bin/env bash
# Runtime C-inline / LLVM-linked twin parity for atomic compiler artifacts.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/.tmp/tests/artifact-atomic-runtime"
CC_BIN="${CC:-gcc}"

command -v "$CC_BIN" >/dev/null 2>&1 || {
    echo "[artifact-atomic-runtime] SKIP missing compiler: $CC_BIN"
    exit 0
}
mkdir -p "$BUILD_DIR"

cat >"$BUILD_DIR/probe.c" <<'C'
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef PGY_ARTIFACT_INLINE_PROBE
#include "runtime/pgy_runtime_artifact_transaction_inline.h"
#else
void pgy_cap_grant_all_export(void);
int32_t pgy_compiler_artifact_begin(const char *path);
bool pgy_compiler_artifact_write(int32_t handle, const char *data);
int32_t pgy_compiler_artifact_commit(int32_t handle);
int32_t pgy_compiler_artifact_abort(int32_t handle);
#endif

int main(int argc, char **argv)
{
    int32_t handle;
    bool write1;
    bool write2;
    int32_t status;

    if (argc != 2)
        return 64;
    pgy_cap_grant_all_export();
    handle = pgy_compiler_artifact_begin(argv[1]);
    printf("begin=%d\n", (int)handle);
    if (handle < 0)
        return 0;
    write1 = pgy_compiler_artifact_write(handle, "alpha-");
    write2 = pgy_compiler_artifact_write(handle, "beta");
    status = pgy_compiler_artifact_commit(handle);
    printf("write1=%d write2=%d status=%d\n",
           write1 ? 1 : 0, write2 ? 1 : 0, (int)status);
    if (status == 6) {
#ifdef _WIN32
        (void)_putenv_s("PGY_ARTIFACT_TXN_FAULT", "");
#else
        (void)unsetenv("PGY_ARTIFACT_TXN_FAULT");
#endif
        printf("cleanup_retry=%d\n",
               (int)pgy_compiler_artifact_abort(handle));
    }
    return 0;
}
C

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -DPGY_RUNTIME_ARTIFACT_TESTING -DPGY_ARTIFACT_INLINE_PROBE \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    "$BUILD_DIR/probe.c" -pthread -o "$BUILD_DIR/inline-probe.exe"

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -DPGY_RUNTIME_ARTIFACT_TESTING -DPGY_LLVM_ENABLED \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    "$BUILD_DIR/probe.c" "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
    -pthread -lm -o "$BUILD_DIR/export-probe.exe"

assert_no_temp() {
    local final_path="$1"
    if compgen -G "${final_path}.pgy-tmp-*" >/dev/null; then
        echo "artifact transaction left a temp file: ${final_path}.pgy-tmp-*" >&2
        return 1
    fi
}

run_case() {
    local probe="$1"
    local label="$2"
    local fault="$3"
    local expected_status="$4"
    local final_path="$BUILD_DIR/${label}.txt"
    local output="$BUILD_DIR/${label}.out"

    printf 'sentinel' >"$final_path"
    rm -f "${final_path}.pgy-tmp-"*
    if [[ -n "$fault" ]]; then
        PGY_IO_ALLOW_ABSOLUTE=1 PGY_ARTIFACT_TXN_FAULT="$fault" \
            "$probe" "$final_path" >"$output"
    else
        PGY_IO_ALLOW_ABSOLUTE=1 env -u PGY_ARTIFACT_TXN_FAULT \
            "$probe" "$final_path" >"$output"
    fi

    if [[ "$fault" == "open" ]]; then
        grep -Eq '^begin=-1$' "$output"
    else
        grep -Eq "status=${expected_status}$" "$output"
    fi
    if [[ -z "$fault" ]]; then
        [[ "$(cat "$final_path")" == "alpha-beta" ]]
    else
        [[ "$(cat "$final_path")" == "sentinel" ]]
    fi
    assert_no_temp "$final_path"
}

for leg in inline export; do
    probe="$BUILD_DIR/${leg}-probe.exe"
    run_case "$probe" "${leg}-success" "" 0
    run_case "$probe" "${leg}-open" open 0
    run_case "$probe" "${leg}-write" write 2
    run_case "$probe" "${leg}-flush" flush 3
    run_case "$probe" "${leg}-close" close 4
    run_case "$probe" "${leg}-publish" publish 5
    run_case "$probe" "${leg}-cleanup" cleanup 6
done

for suffix in success open write flush close publish cleanup; do
    sed -E 's/^begin=[0-9-]+$/begin=<handle>/' \
        "$BUILD_DIR/inline-${suffix}.out" >"$BUILD_DIR/inline-${suffix}.normalized"
    sed -E 's/^begin=[0-9-]+$/begin=<handle>/' \
        "$BUILD_DIR/export-${suffix}.out" >"$BUILD_DIR/export-${suffix}.normalized"
    [[ "$(sha256sum "$BUILD_DIR/inline-${suffix}.normalized" | awk '{print $1}')" == \
       "$(sha256sum "$BUILD_DIR/export-${suffix}.normalized" | awk '{print $1}')" ]]
done

echo "[artifact-atomic-runtime] C-inline/LLVM-export success and open/write/flush/close/publish/cleanup failures preserve final and clean temp"
