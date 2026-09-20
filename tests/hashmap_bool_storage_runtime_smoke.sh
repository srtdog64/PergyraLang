#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-bool-runtime.XXXXXX)"
CC_BIN="${CC:-gcc}"
RUNTIME_LINK_FLAGS=(-pthread -lm)
RAW_RUNTIME_LINK_FLAGS=(-pthread -lm)
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) RAW_RUNTIME_LINK_FLAGS+=(-lws2_32) ;;
esac

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_bool_storage_runtime.c -o "$WORK/runtime.exe" \
    "${RUNTIME_LINK_FLAGS[@]}"
"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_bool_raw_storage_runtime.c -o "$WORK/raw-runtime.exe" \
    "${RAW_RUNTIME_LINK_FLAGS[@]}"

"$WORK/runtime.exe" >"$WORK/out" 2>"$WORK/err"
grep -Fxq 'hashmap bool storage runtime: ok' "$WORK/out"
grep -Fq 'allocation failed' "$WORK/err"
"$WORK/raw-runtime.exe" >"$WORK/raw.out" 2>"$WORK/raw.err"
grep -Fxq 'hashmap raw bool storage runtime: ok' "$WORK/raw.out"
grep -Fq 'allocation failed' "$WORK/raw.err"

for exe in runtime raw-runtime; do
    for mode in mismatch mapkeys-mismatch; do
        status=0
        "$WORK/$exe.exe" "$mode" >"$WORK/$exe-$mode.out" \
            2>"$WORK/$exe-$mode.err" || status=$?
        if [[ "$status" == 0 ]] ||
            ! grep -Fq 'map key storage kind mismatch' "$WORK/$exe-$mode.err"; then
            echo "[hashmap-bool-runtime] $exe $mode did not fail closed" >&2
            exit 1
        fi
        if grep -Fq 'allocation failed' "$WORK/$exe-$mode.err"; then
            echo "[hashmap-bool-runtime] $exe $mode allocated before kind validation" >&2
            exit 1
        fi
    done
done

for forbidden in pgy_map_format_bool_key pgy_map_bool_key_string_export \
    'invalid stored bool key'; do
    if grep -R -Fq --include='*.h' --include='*.c' "$forbidden" src/runtime; then
        echo "[hashmap-bool-runtime] retired Bool string bridge returned: $forbidden" >&2
        exit 1
    fi
done
for required in pgy_map_new_bool_int pgy_map_new_bool_string \
    pgy_hashmap_hash_bool PGY_HASHMAP_KEY_STORAGE_BOOL \
    PGY_MAP_RAW_BOOL_KEYS pgy_map_raw_require_storage_kind; do
    grep -R -Fq "$required" src/runtime || {
        echo "[hashmap-bool-runtime] required owner term missing: $required" >&2
        exit 1
    }
done

echo "[hashmap-bool-runtime] inline+raw true/false/update/delete-reinsert/MapKeys/OOM/string-alias/mismatch PASS: $WORK"
