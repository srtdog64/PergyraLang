#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-i64-runtime.XXXXXX)"
CC_BIN="${CC:-gcc}"

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_i64_storage_runtime.c -o "$WORK/runtime.exe" \
    -lwinpthread -lm
"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_i64_raw_storage_runtime.c -o "$WORK/raw-runtime.exe" \
    -lwinpthread -lws2_32 -lm
"$WORK/runtime.exe" >"$WORK/out" 2>"$WORK/err"
grep -Fxq 'hashmap i64 storage runtime: ok' "$WORK/out"
if ! grep -Fq 'allocation failed' "$WORK/err"; then
    echo '[hashmap-i64-runtime] allocation failure was not observable' >&2
    exit 1
fi

"$WORK/raw-runtime.exe" >"$WORK/raw.out" 2>"$WORK/raw.err"
grep -Fxq 'hashmap raw i64 storage runtime: ok' "$WORK/raw.out"
if ! grep -Fq 'allocation failed' "$WORK/raw.err"; then
    echo '[hashmap-i64-runtime] raw allocation failure was not observable' >&2
    exit 1
fi
for mode in mismatch mapkeys-mismatch; do
    status=0
    "$WORK/raw-runtime.exe" "$mode" >"$WORK/raw-$mode.out" \
        2>"$WORK/raw-$mode.err" || status=$?
    if [[ "$status" == 0 ]] ||
        ! grep -Fq 'map key storage kind mismatch' "$WORK/raw-$mode.err"; then
        echo "[hashmap-i64-runtime] raw $mode did not fail closed" >&2
        exit 1
    fi
done

status=0
"$WORK/runtime.exe" mismatch >"$WORK/mismatch.out" \
    2>"$WORK/mismatch.err" || status=$?
if [[ "$status" == 0 ]] ||
    ! grep -Fq 'map key storage kind mismatch' "$WORK/mismatch.err"; then
    echo '[hashmap-i64-runtime] key storage mismatch did not fail closed' >&2
    exit 1
fi

for forbidden in pgy_map_format_i64_key pgy_map_i64_key_string_export strtoll; do
    if grep -R -Fq --include='*.h' --include='*.c' "$forbidden" src/runtime; then
        echo "[hashmap-i64-runtime] retired Long string bridge returned: $forbidden" >&2
        exit 1
    fi
done
for required in \
    'pgy_map_new_i64_int' \
    'pgy_hashmap_hash_i64' \
    'PGY_HASHMAP_KEY_STORAGE_I64' \
    'PGY_MAP_RAW_I64_KEYS' \
    'pgy_map_raw_require_storage_kind'; do
    grep -R -Fq "$required" src/runtime || {
        echo "[hashmap-i64-runtime] required owner term missing: $required" >&2
        exit 1
    }
done

echo "[hashmap-i64-runtime] inline+raw collision/delete/grow/extremes/high-half-hash/OOM/update-no-allocation/mismatch PASS: $WORK"
