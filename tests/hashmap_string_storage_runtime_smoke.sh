#!/usr/bin/env bash
# CLOSED release-ABI falsifiers: value_type_blind_release,
# release_without_initialized_storage, duplicate_release_nonzero_descriptor,
# mapkeys_snapshot_backing_only_release.
set -euo pipefail
# Named falsifiers for the CLOSED owner row: borrowed_source_key_pointer,
# post_grow_key_duplication, partial_mapkeys_snapshot,
# mapkeys_on_invalid_storage, backend_local_string_key_policy.
# The next ACTIVE release row forbids parameter_descriptor_drop,
# inout_descriptor_drop, returned_descriptor_drop,
# unproven_shallow_alias_drop, mapkeys_result_as_borrowed_array, and
# mapkeys_owned_snapshot_mutated_by_shallow_array_ops.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK="$(mktemp -d .tmp/self_hosted/hashmap-string-runtime.XXXXXX)"
CC_BIN="${CC:-gcc}"
RUNTIME_LINK_FLAGS=(-pthread -lm)
RAW_RUNTIME_LINK_FLAGS=(-pthread -lm)
case "$(uname -s 2>/dev/null || echo unknown)" in
    MINGW*|MSYS*|CYGWIN*) RAW_RUNTIME_LINK_FLAGS+=(-lws2_32) ;;
esac

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_string_storage_runtime.c -o "$WORK/runtime.exe" \
    "${RUNTIME_LINK_FLAGS[@]}"
"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_string_raw_storage_runtime.c -o "$WORK/raw-runtime.exe" \
    "${RAW_RUNTIME_LINK_FLAGS[@]}"

fail() { echo "[hashmap-string-runtime] $*" >&2; exit 1; }

# The success run injects no failure: exit 0, the ok line, nothing on stderr.
expect_success_run() {
    local exe="$1" ok_line="$2" status=0
    "$WORK/$exe.exe" >"$WORK/$exe.out" 2>"$WORK/$exe.err" || status=$?
    if [[ "$status" != 0 ]] || ! grep -Fxq -- "$ok_line" "$WORK/$exe.out" ||
        [[ -s "$WORK/$exe.err" ]]; then
        cat "$WORK/$exe.err" >&2; fail "$exe success run failed (exit $status)"
    fi
}
expect_success_run runtime 'hashmap string storage runtime: ok'
expect_success_run raw-runtime 'hashmap raw string storage runtime: ok'

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
    expect_child_panic runtime "ctor-oom-$n" "$OOM" 'op=map_new_int reason=allocation failed'
    expect_child_panic runtime "generic-ctor-oom-$n" "$OOM" 'op=map_new_TestValue reason=allocation failed'
    expect_child_panic runtime "grow-oom-$n" "$OOM" 'op=map_grow_int reason=allocation failed'
    expect_child_panic raw-runtime "ctor-oom-$n" "$OOM" 'op=map_new reason=allocation failed'
    expect_child_panic raw-runtime "grow-oom-$n" "$OOM" 'op=map_grow reason=allocation failed'
done
expect_child_panic runtime insert-dup-oom "$OOM" 'op=map_set_int reason=key duplication failed'
# The grow allocation is armed too: the key copy must fail before growth.
expect_child_panic runtime dup-before-grow-oom "$OOM" \
    'op=map_set_int reason=key duplication failed' 'op=map_grow_int'
expect_child_panic runtime string-update-oom "$OOM" \
    'op=map_set_string reason=value duplication failed'
expect_child_panic raw-runtime dup-before-grow-oom "$OOM" \
    'op=map_set reason=key duplication failed' 'op=map_grow'
for n in 1 2; do
    expect_child_panic raw-runtime "string-dup-before-grow-oom-$n" "$OOM" \
        'op=map_set_string_value reason=key/value duplication failed' 'op=map_grow'
done
expect_child_panic raw-runtime string-update-oom "$OOM" \
    'op=map_set_string_value reason=value duplication failed'
expect_child_panic runtime set-invalid-map "$INVALID" 'op=map_set_int reason=invalid map or key'
expect_child_panic raw-runtime set-invalid-map "$INVALID" 'op=map_set reason=map is not initialized'

for exe in runtime raw-runtime; do
    # Kind validation precedes the key copy, whose allocation is armed.
    expect_child_panic "$exe" mismatch "$MISMATCH" '' 'allocation failed'
    # MapKeys panics directly with class oom and prints no collection detail.
    for mode in mapkeys-oom mapkeys-mid-oom; do
        expect_child_panic "$exe" "$mode" "$OOM" ''
    done
done
expect_child_panic runtime generic-invalid-mapkeys \
    'class=internal-invariant reason=map keys on invalid map' ''

for required in PGY_HASHMAP_KEY_STORAGE_STRING pgy_runtime_strdup \
    'PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM' pgy_map_keys_raw_export \
    pgy_runtime_panic_collection_oom pgy_hashmap_rebuild_capacity; do
    grep -R -Fq "$required" src/runtime || { echo "[hashmap-string-runtime] missing owner term: $required" >&2; exit 1; }
done

echo "[hashmap-string-runtime] inline+generic+raw owned-key/grow/update/dup-OOM/quarantine-self-alias/MapKeys/exact-drop/mismatch PASS: $WORK"
