#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here hashmap-owned-string-provenance "$PGY"
cd "$ROOT_DIR"

WORK="$(mktemp -d .tmp/hashmap-owned-string-provenance.XXXXXX)"
for name in borrowed_string_array_deep_drop map_keys_shallow_push \
    map_keys_shallow_set map_keys_shallow_pop map_keys_shallow_copy \
    map_keys_double_drop; do
    status=0
    "$PGY" --native-pipeline --mir-json --error-format=json \
        "tests/concept_semantics/hashmap/$name.pgy" \
        >"$WORK/$name.out" 2>"$WORK/$name.err" || status=$?
    if [[ "$status" != 1 ]] || grep -Fq '"pgy.mir.v1"' "$WORK/$name.out" ||
        ! grep -Fq 'PGY_SEM_BORROW_ESCAPE' "$WORK/$name.out" "$WORK/$name.err"; then
        echo "[hashmap-owned-string-provenance] missing refusal: $name (status $status)" >&2
        exit 1
    fi
done

"$PGY" --native-pipeline --mir-json --error-format=json \
    tests/concept_semantics/hashmap/map_keys_owned_drop_valid.pgy \
    >"$WORK/valid.out" 2>"$WORK/valid.err"
grep -Fq '"pgy.mir.v1"' "$WORK/valid.out"
[[ ! -s "$WORK/valid.err" ]]

echo "[hashmap-owned-string-provenance] borrowed drops, MapKeys shallow mutation/copy/double-drop refusals and owned-drop control PASS"
