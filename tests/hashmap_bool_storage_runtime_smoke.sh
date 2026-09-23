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

fail() { echo "[hashmap-bool-runtime] $*" >&2; exit 1; }

# The success run injects no failure: exit 0, the ok line, nothing on stderr.
expect_success_run() {
    local exe="$1" ok_line="$2" status=0
    "$WORK/$exe.exe" >"$WORK/$exe.out" 2>"$WORK/$exe.err" || status=$?
    if [[ "$status" != 0 ]] || ! grep -Fxq -- "$ok_line" "$WORK/$exe.out" ||
        [[ -s "$WORK/$exe.err" ]]; then
        cat "$WORK/$exe.err" >&2; fail "$exe success run failed (exit $status)"
    fi
}
expect_success_run runtime 'hashmap bool storage runtime: ok'
expect_success_run raw-runtime 'hashmap raw bool storage runtime: ok'

# A failed allocation panics (docs/105_runtime_panic_contract.md, class oom),
# so each injected failure runs as a child mode that must abort: exit 0 means
# it continued, 99 that the operation returned, 98 an unknown mode. <detail>
# is the text after "[PGY PANIC] collection "; an empty detail means the
# panic site has no collection detail line and none may appear.
expect_child_panic() {
    local exe="$1" mode="$2" panic="$3" detail="$4" forbidden="${5:-}"
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
    if [[ -n "$forbidden" ]] && grep -Fq -- "$forbidden" "$err"; then
        cat "$err" >&2; fail "$exe $mode must not print: $forbidden"
    fi
}
OOM='class=oom reason=allocation failed'
INVALID='class=internal-invariant reason=invalid collection operation'
MISMATCH='class=internal-invariant reason=map key storage kind mismatch'

for n in 1 2 3; do
    expect_child_panic runtime "ctor-oom-$n" "$OOM" 'op=map_new_bool_int reason=allocation failed'
    expect_child_panic raw-runtime "ctor-oom-$n" "$OOM" 'op=map_new reason=allocation failed'
done
for exe in runtime raw-runtime; do
    # Kind validation must precede any allocation, which is armed to fail.
    for mode in mismatch mapkeys-mismatch; do
        expect_child_panic "$exe" "$mode" "$MISMATCH" '' 'allocation failed'
    done
done
expect_child_panic runtime set-invalid-map "$INVALID" 'op=map_set_bool_int reason=map is not initialized'
expect_child_panic raw-runtime set-invalid-map "$INVALID" 'op=map_set_bool reason=map is not initialized'

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
