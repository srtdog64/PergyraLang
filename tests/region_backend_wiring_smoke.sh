#!/usr/bin/env bash
set -euo pipefail

# Subject of this gate: native C/LLVM region-plan backend wiring.
# Delegating would turn a self-host coverage gap into a region regression.
# This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

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
user_good="$ROOT_DIR/tests/cases/backend_compare/region_user_callee/main.pgy"
user_bad="$ROOT_DIR/tests/cases/backend_compare/region_user_callee_bad/main.pgy"

"$PGY_BIN" "$direct" --emit-c -o "$WORK/direct.c" >/dev/null
"$PGY_BIN" "$direct" --emit-llvm -o "$WORK/direct.ll" >/dev/null
"$PGY_BIN" "$heap" --emit-c -o "$WORK/heap.c" >/dev/null
"$PGY_BIN" "$heap" --emit-llvm -o "$WORK/heap.ll" >/dev/null
"$PGY_BIN" "$user_good" --emit-c -o "$WORK/user_good.c" >/dev/null
"$PGY_BIN" "$user_good" --emit-llvm -o "$WORK/user_good.ll" >/dev/null
"$PGY_BIN" "$user_bad" --emit-c -o "$WORK/user_bad.c" >/dev/null
"$PGY_BIN" "$user_bad" --emit-llvm -o "$WORK/user_bad.ll" >/dev/null

direct_c="$(awk '/void Main\(\)/,/^}/' "$WORK/direct.c")"
direct_ll="$(awk '/define .*@Main/,/^}/' "$WORK/direct.ll")"
heap_c="$(awk '/void Main\(\)/,/^}/' "$WORK/heap.c")"
heap_ll="$(awk '/define .*@Main/,/^}/' "$WORK/heap.ll")"
user_good_c="$(awk '/void Main\(\)/,/^}/' "$WORK/user_good.c")"
user_good_ll="$(awk '/define .*@Main/,/^}/' "$WORK/user_good.ll")"
user_bad_c="$(awk '/void Main\(\)/,/^}/' "$WORK/user_bad.c")"
user_bad_ll="$(awk '/define .*@Main/,/^}/' "$WORK/user_bad.ll")"

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

grep -Fq 'pgy_region_create(0)' <<<"$user_good_c"
grep -Fq 'pgy_region_string_concat(&__pgy_region_' <<<"$user_good_c"
grep -Fq 'pgy_region_destroy(&__pgy_region_' <<<"$user_good_c"
! grep -Fq 'StringConcat(' <<<"$user_good_c"

grep -Fq '@pgy_region_create_export' <<<"$user_good_ll"
grep -Fq '@pgy_region_string_concat_export' <<<"$user_good_ll"
grep -Fq '@pgy_region_destroy_export' <<<"$user_good_ll"
! grep -Fq '@StringConcat' <<<"$user_good_ll"

grep -Fq 'StringConcat(' <<<"$user_bad_c"
! grep -Fq 'pgy_region_' <<<"$user_bad_c"
grep -Fq 'call ptr @StringConcat' <<<"$user_bad_ll"
! grep -Fq '@pgy_region_string_concat_export' <<<"$user_bad_ll"

echo "[region-backend] PASS builtin and direct user-callee sinks are region-backed; non-certified bindings stay heap"
