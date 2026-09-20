#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-i32-runtime.XXXXXX)"
CC_BIN="${CC:-gcc}"
RUNTIME_LINK_FLAGS=(-pthread -lm)

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_i32_storage_runtime.c -o "$WORK/runtime.exe" \
    "${RUNTIME_LINK_FLAGS[@]}"
"$WORK/runtime.exe" >"$WORK/out" 2>"$WORK/err"
grep -Fxq 'hashmap i32 storage runtime: ok' "$WORK/out"
if ! grep -Fq 'allocation failed' "$WORK/err"; then
    echo '[hashmap-i32-runtime] allocation failure was not observable' >&2
    exit 1
fi

status=0
"$WORK/runtime.exe" mismatch >"$WORK/mismatch.out" \
    2>"$WORK/mismatch.err" || status=$?
if [[ "$status" == 0 ]] ||
    ! grep -Fq 'map key storage kind mismatch' "$WORK/mismatch.err"; then
    echo '[hashmap-i32-runtime] key storage mismatch did not fail closed' >&2
    exit 1
fi

for forbidden in pgy_map_format_i32_key pgy_map_i32_key_string_export; do
    if grep -R -Fq "$forbidden" src/runtime; then
        echo "[hashmap-i32-runtime] retired Int string bridge returned: $forbidden" >&2
        exit 1
    fi
done
for required in \
    'pgy_map_new_i32_int' \
    'pgy_hashmap_hash_i32' \
    'PGY_HASHMAP_KEY_STORAGE_I32' \
    'map->deleted_count' \
    'pgy_map_raw_require_storage_kind'; do
    grep -R -Fq "$required" src/runtime || {
        echo "[hashmap-i32-runtime] required owner term missing: $required" >&2
        exit 1
    }
done

echo "[hashmap-i32-runtime] collision/delete/grow/extremes/OOM/update-no-allocation/mismatch PASS: $WORK"
