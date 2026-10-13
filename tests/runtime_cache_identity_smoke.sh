#!/usr/bin/env bash
# Runtime-object cache identity gate (review 2026-07-20).
#
# Source mtimes remain a freshness check, but they are not a cache identity.
# Changing the compiler revision must select a different runtime object path so
# an old object cannot be linked merely because its inputs are not newer.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

LABEL="runtime-cache-identity"
FIXTURE="$ROOT_DIR/examples/basic.pgy"
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler: $PGY" >&2; exit 1; }
[[ -f "$FIXTURE" ]] || { echo "[$LABEL] missing fixture: $FIXTURE" >&2; exit 1; }

cache_tmp="${TMPDIR:-${TMP:-${TEMP:-/tmp}}}"
cache_tmp="${cache_tmp//\\//}"
cache_glob="$cache_tmp/pgy_runtime_cext_v2_"*
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

drop_cache() {
    rm -f $cache_glob 2>/dev/null || true
}

cache_path() {
    compgen -G "$cache_glob" | head -n 1 || true
}

build_with_revision() {
    local revision="$1"
    local output="$OUT_DIR/$revision.exe"
    local source_path binary_path
    source_path="$(pgy_path_for_compiler "$PGY" "$FIXTURE")"
    binary_path="$(pgy_path_for_compiler "$PGY" "$output")"
    if ! (cd "$ROOT_DIR" && PGY_COMPILER_REVISION="$revision" \
        "$PGY" "$source_path" --native-pipeline --backend=c -o "$binary_path") \
        >"$OUT_DIR/$revision.log" 2>&1; then
        echo "[$LABEL] revision $revision build failed" >&2
        tail -20 "$OUT_DIR/$revision.log" >&2 || true
        exit 1
    fi
    local path
    path="$(cache_path)"
    [[ -n "$path" ]] || { echo "[$LABEL] revision $revision did not publish a v2 cache object" >&2; exit 1; }
    printf '%s\n' "$path"
}

drop_cache
first="$(build_with_revision review-a)"
drop_cache
second="$(build_with_revision review-b)"

if [[ "$first" == "$second" ]]; then
    echo "[$LABEL] compiler revision did not change cache identity" >&2
    exit 1
fi
[[ "$first" == *"pgy_runtime_cext_v2_"* ]] || { echo "[$LABEL] first path missing v2 identity" >&2; exit 1; }
[[ "$second" == *"pgy_runtime_cext_v2_"* ]] || { echo "[$LABEL] second path missing v2 identity" >&2; exit 1; }

echo "[$LABEL] PASS compiler revision selects distinct runtime cache identities"
