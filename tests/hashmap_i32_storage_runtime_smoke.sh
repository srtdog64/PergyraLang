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

fail() { echo "[hashmap-i32-runtime] $*" >&2; exit 1; }

# The success run injects no failure: exit 0, the ok line, nothing on stderr.
status=0
"$WORK/runtime.exe" >"$WORK/runtime.out" 2>"$WORK/runtime.err" || status=$?
if [[ "$status" != 0 ]] ||
    ! grep -Fxq 'hashmap i32 storage runtime: ok' "$WORK/runtime.out" ||
    [[ -s "$WORK/runtime.err" ]]; then
    cat "$WORK/runtime.err" >&2; fail "success run failed (exit $status)"
fi

# A failed allocation panics (docs/105_runtime_panic_contract.md, class oom),
# so each injected failure runs as a child mode that must abort: exit 0 means
# it continued, 99 that the operation returned, 98 an unknown mode. <detail>
# is the text after "[PGY PANIC] collection "; an empty detail means the
# panic site has no collection detail line and none may appear.
expect_child_panic() {
    local mode="$1" panic="$2" detail="$3"
    local err="$WORK/$mode.err" status=0
    "$WORK/runtime.exe" "$mode" >"$WORK/$mode.out" 2>"$err" || status=$?
    case "$status" in
        0|98|99) cat "$err" >&2; fail "$mode exited $status instead of panicking" ;;
    esac
    grep -Fq -- "$panic" "$err" || { cat "$err" >&2; fail "$mode lacks: $panic"; }
    if [[ -n "$detail" ]]; then
        grep -Fq -- "[PGY PANIC] collection $detail" "$err" ||
            { cat "$err" >&2; fail "$mode lacks detail: $detail"; }
    elif grep -Fq -- '[PGY PANIC] collection ' "$err"; then
        cat "$err" >&2; fail "$mode printed an unexpected collection detail"
    fi
}

for n in 1 2 3; do
    expect_child_panic "ctor-oom-$n" 'class=oom reason=allocation failed' \
        'op=map_new_i32_int reason=allocation failed'
    expect_child_panic "grow-oom-$n" 'class=oom reason=allocation failed' \
        'op=map_grow_int reason=allocation failed'
done
expect_child_panic mismatch \
    'class=internal-invariant reason=map key storage kind mismatch' ''
expect_child_panic set-invalid-map \
    'class=internal-invariant reason=invalid collection operation' \
    'op=map_set_i32_int reason=map is not initialized'

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
