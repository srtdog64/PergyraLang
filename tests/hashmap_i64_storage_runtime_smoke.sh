#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-i64-runtime.XXXXXX)"
CC_BIN="${CC:-gcc}"
RUNTIME_LINK_FLAGS=(-pthread -lm)
RAW_RUNTIME_LINK_FLAGS=(-pthread -lm)
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) RAW_RUNTIME_LINK_FLAGS+=(-lws2_32) ;;
esac

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_i64_storage_runtime.c -o "$WORK/runtime.exe" \
    "${RUNTIME_LINK_FLAGS[@]}"
"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_i64_raw_storage_runtime.c -o "$WORK/raw-runtime.exe" \
    "${RAW_RUNTIME_LINK_FLAGS[@]}"

fail() { echo "[hashmap-i64-runtime] $*" >&2; exit 1; }

# The success run injects no failure: exit 0, the ok line, nothing on stderr.
expect_success_run() {
    local exe="$1" ok_line="$2" status=0
    "$WORK/$exe.exe" >"$WORK/$exe.out" 2>"$WORK/$exe.err" || status=$?
    if [[ "$status" != 0 ]] || ! grep -Fxq -- "$ok_line" "$WORK/$exe.out" ||
        [[ -s "$WORK/$exe.err" ]]; then
        cat "$WORK/$exe.err" >&2; fail "$exe success run failed (exit $status)"
    fi
}
expect_success_run runtime 'hashmap i64 storage runtime: ok'
expect_success_run raw-runtime 'hashmap raw i64 storage runtime: ok'

# A failed allocation panics (docs/105_runtime_panic_contract.md, class oom),
# so each injected failure runs as a child mode that must abort: exit 0 means
# it continued, 99 that the operation returned, 98 an unknown mode. <detail>
# is the text after "[PGY PANIC] collection "; an empty detail means the
# panic site has no collection detail line and none may appear.
expect_child_panic() {
    local exe="$1" mode="$2" panic="$3" detail="$4"
    local err="$WORK/$exe-$mode.err" status=0
    "$WORK/$exe.exe" "$mode" >"$WORK/$exe-$mode.out" 2>"$err" || status=$?
    case "$status" in
        0|98|99) cat "$err" >&2; fail "$exe $mode exited $status instead of panicking" ;;
    esac
    grep -Fq -- "$panic" "$err" || { cat "$err" >&2; fail "$exe $mode lacks: $panic"; }
    if [[ -n "$detail" ]]; then
        grep -Fq -- "[PGY PANIC] collection $detail" "$err" ||
            { cat "$err" >&2; fail "$exe $mode lacks detail: $detail"; }
    elif grep -Fq -- '[PGY PANIC] collection ' "$err"; then
        cat "$err" >&2; fail "$exe $mode printed an unexpected collection detail"
    fi
}
OOM='class=oom reason=allocation failed'
INVALID='class=internal-invariant reason=invalid collection operation'
MISMATCH='class=internal-invariant reason=map key storage kind mismatch'

for n in 1 2 3; do
    expect_child_panic runtime "ctor-oom-$n" "$OOM" 'op=map_new_i64_int reason=allocation failed'
    expect_child_panic runtime "grow-oom-$n" "$OOM" 'op=map_grow_int reason=allocation failed'
    expect_child_panic raw-runtime "ctor-oom-$n" "$OOM" 'op=map_new reason=allocation failed'
    expect_child_panic raw-runtime "grow-oom-$n" "$OOM" 'op=map_grow reason=allocation failed'
done
expect_child_panic runtime mismatch "$MISMATCH" ''
for mode in mismatch mapkeys-mismatch; do
    expect_child_panic raw-runtime "$mode" "$MISMATCH" ''
done
expect_child_panic runtime set-invalid-map "$INVALID" 'op=map_set_i64_int reason=map is not initialized'
expect_child_panic raw-runtime set-invalid-map "$INVALID" 'op=map_set_i64 reason=map is not initialized'

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
    'pgy_map_raw_require_storage_kind' \
    'pgy_hashmap_rebuild_capacity'; do
    grep -R -Fq "$required" src/runtime || {
        echo "[hashmap-i64-runtime] required owner term missing: $required" >&2
        exit 1
    }
done

echo "[hashmap-i64-runtime] inline+raw collision/delete/grow/extremes/high-half-hash/OOM/update-no-allocation/mismatch PASS: $WORK"
