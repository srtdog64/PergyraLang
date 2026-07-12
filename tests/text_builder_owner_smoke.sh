#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[text-builder-owner] missing compiler: $PGY" >&2; exit 1; }

FIXTURES="$ROOT_DIR/tests/cases/text_builder_owner"
WORK_ROOT="$ROOT_DIR/.tmp/tests"
mkdir -p "$WORK_ROOT"
OUT_DIR="$(mktemp -d "$WORK_ROOT/text-builder-owner.XXXXXX")"
trap 'rm -rf "$OUT_DIR"' EXIT

expect_reject() {
    local backend="$1" fixture="$2" diagnostic="$3"
    local source output log rc
    source="$(pgy_path_for_compiler "$PGY" "$FIXTURES/$fixture.pgy")"
    output="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/${backend}_${fixture}")"
    log="$OUT_DIR/${backend}_${fixture}.log"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$source" --backend="$backend" \
        --error-format=json -o "$output") >"$log" 2>&1
    rc=$?
    set -e
    [[ $rc -ne 0 ]] || { echo "[text-builder-owner] $backend/$fixture compiled" >&2; exit 1; }
    grep -Fq "$diagnostic" "$log" || {
        echo "[text-builder-owner] $backend/$fixture missed $diagnostic" >&2
        tail -20 "$log" >&2
        exit 1
    }
}

expect_nested_append() {
    local backend="$1" output_native="$OUT_DIR/${backend}_nested_append.exe" log="$OUT_DIR/${backend}_nested_append.log"
    local source output
    source="$(pgy_path_for_compiler "$PGY" "$FIXTURES/nested_append.pgy")"
    output="$(pgy_path_for_compiler "$PGY" "$output_native")"
    (cd "$ROOT_DIR" && "$PGY" "$source" --backend="$backend" -o "$output") >"$log" 2>&1 || {
        echo "[text-builder-owner] $backend/nested_append failed to compile" >&2
        tail -20 "$log" >&2
        exit 1
    }
    local actual
    actual="$("$output_native" | tr -d '\r')"
    [[ "$actual" == "nested" ]] || {
        echo "[text-builder-owner] $backend/nested_append output drift: $actual" >&2
        exit 1
    }
}

for backend in c llvm; do
    expect_nested_append "$backend"
    expect_reject "$backend" copy PGY_SEM_ANCHORED_HANDLE_COPY
    expect_reject "$backend" missing_finish PGY_SEM_OWNER_NOT_CONSUMED
    expect_reject "$backend" nested_finish PGY_SEM_BUILTIN_ARGS_INVALID
    expect_reject "$backend" parameter PGY_SEM_TYPE_MISMATCH
done

echo "[text-builder-owner] nested append passes; copy, nested consume, parameter, and live-exit boundaries fail closed"
