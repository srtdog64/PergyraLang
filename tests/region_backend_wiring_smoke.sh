#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
WORK="$(mktemp -d "$ROOT_DIR/.tmp/region_backend_wiring.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

if [[ ! -x "$PGY_BIN" && ! -x "${PGY_BIN}.exe" ]]; then
    echo "[region-backend] missing compiler: $PGY_BIN" >&2
    exit 1
fi
if [[ ! -x "$PGY_BIN" && -x "${PGY_BIN}.exe" ]]; then
    PGY_BIN="${PGY_BIN}.exe"
fi

direct="$ROOT_DIR/tests/cases/backend_compare/region_string_concat/main.pgy"
heap="$ROOT_DIR/tests/cases/backend_compare/region_string_concat_heap/main.pgy"

"$PGY_BIN" "$direct" --emit-c -o "$WORK/direct.c" >/dev/null
"$PGY_BIN" "$direct" --emit-llvm -o "$WORK/direct.ll" >/dev/null
"$PGY_BIN" "$heap" --emit-c -o "$WORK/heap.c" >/dev/null
"$PGY_BIN" "$heap" --emit-llvm -o "$WORK/heap.ll" >/dev/null

direct_c="$(awk '/void Main\(\)/,/^}/' "$WORK/direct.c")"
direct_ll="$(awk '/define .*@Main/,/^}/' "$WORK/direct.ll")"
heap_c="$(awk '/void Main\(\)/,/^}/' "$WORK/heap.c")"
heap_ll="$(awk '/define .*@Main/,/^}/' "$WORK/heap.ll")"

grep -Fq 'pgy_region_create(0)' <<<"$direct_c"
grep -Fq 'pgy_region_string_concat(&__pgy_region_' <<<"$direct_c"
grep -Fq 'pgy_region_destroy(&__pgy_region_' <<<"$direct_c"
! grep -Fq 'StringConcat(' <<<"$direct_c"

grep -Fq '@pgy_region_create_export' <<<"$direct_ll"
grep -Fq '@pgy_region_string_concat_export' <<<"$direct_ll"
grep -Fq '@pgy_region_destroy_export' <<<"$direct_ll"
! grep -Fq '@StringConcat' <<<"$direct_ll"

grep -Fq 'StringConcat(' <<<"$heap_c"
! grep -Fq 'pgy_region_' <<<"$heap_c"
grep -Fq 'call ptr @StringConcat' <<<"$heap_ll"
! grep -Fq '@pgy_region_string_concat_export' <<<"$heap_ll"

echo "[region-backend] PASS certified direct Print concat is region-backed; non-certified binding stays heap"
