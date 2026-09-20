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

"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_string_storage_runtime.c -o "$WORK/runtime.exe" -lwinpthread -lm
"$CC_BIN" -std=c11 -O2 -Wall -Wextra -Werror=implicit-function-declaration \
    -Isrc tests/hashmap_string_raw_storage_runtime.c -o "$WORK/raw-runtime.exe" -lwinpthread -lws2_32 -lm

"$WORK/runtime.exe" >"$WORK/out" 2>"$WORK/err"
grep -Fxq 'hashmap string storage runtime: ok' "$WORK/out"
"$WORK/raw-runtime.exe" >"$WORK/raw.out" 2>"$WORK/raw.err"
grep -Fxq 'hashmap raw string storage runtime: ok' "$WORK/raw.out"
for file in "$WORK/err" "$WORK/raw.err"; do
    grep -Fq 'allocation failed' "$file" || grep -Fq 'duplication failed' "$file"
done

for exe in runtime raw-runtime; do
    status=0
    "$WORK/$exe.exe" mismatch >"$WORK/$exe-mismatch.out" 2>"$WORK/$exe-mismatch.err" || status=$?
    if [[ "$status" == 0 ]] || ! grep -Fq 'map key storage kind mismatch' "$WORK/$exe-mismatch.err" ||
        grep -Fq 'allocation failed' "$WORK/$exe-mismatch.err"; then
        echo "[hashmap-string-runtime] $exe mismatch did not fail before allocation" >&2; exit 1
    fi
    for mode in mapkeys-oom mapkeys-mid-oom; do
        status=0
        "$WORK/$exe.exe" "$mode" >"$WORK/$exe-$mode.out" 2>"$WORK/$exe-$mode.err" || status=$?
        if [[ "$status" == 0 ]] || ! grep -Fq 'class=oom' "$WORK/$exe-$mode.err"; then
            echo "[hashmap-string-runtime] $exe $mode was partial/silent" >&2; exit 1
        fi
    done
done

status=0
"$WORK/runtime.exe" generic-invalid-mapkeys >"$WORK/generic-invalid.out" 2>"$WORK/generic-invalid.err" || status=$?
if [[ "$status" == 0 ]] || ! grep -Fq 'map keys on invalid map' "$WORK/generic-invalid.err"; then
    echo '[hashmap-string-runtime] generic ctor OOM map masqueraded as an empty MapKeys snapshot' >&2; exit 1
fi

for required in PGY_HASHMAP_KEY_STORAGE_STRING pgy_runtime_strdup \
    'PGY_RUNTIME_PANIC(PGY_RUNTIME_PANIC_CLASS_OOM' pgy_map_keys_raw_export; do
    grep -R -Fq "$required" src/runtime || { echo "[hashmap-string-runtime] missing owner term: $required" >&2; exit 1; }
done

echo "[hashmap-string-runtime] inline+generic+raw owned-key/grow/update/dup-OOM/quarantine-self-alias/MapKeys/exact-drop/mismatch PASS: $WORK"
