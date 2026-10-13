#!/usr/bin/env bash
# Live self-host progress inventory. LOC is implementation volume, never a
# replacement percentage; replacement requires an executable production path.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

fail() { echo "[self-host-progress] FAIL: $*" >&2; exit 1; }

count_lines() {
    find "$@" -type f -name '*.pgy' -print0 | xargs -0 cat | wc -l | tr -d ' '
}

count_c_lines() {
    find "$@" -type f \( -name '*.c' -o -name '*.h' \) -print0 |
        xargs -0 cat | wc -l | tr -d ' '
}

pgy_frontend_backend_loc="$(count_lines \
    "$ROOT_DIR/src/self_hosted/lexer" \
    "$ROOT_DIR/src/self_hosted/parser" \
    "$ROOT_DIR/src/self_hosted/semantic" \
    "$ROOT_DIR/src/self_hosted/codegen")"
pgy_compiler_core_loc="$(count_lines \
    "$ROOT_DIR/src/self_hosted/lexer" \
    "$ROOT_DIR/src/self_hosted/parser" \
    "$ROOT_DIR/src/self_hosted/semantic" \
    "$ROOT_DIR/src/self_hosted/codegen" \
    "$ROOT_DIR/src/self_hosted/hir" \
    "$ROOT_DIR/src/self_hosted/mir" \
    "$ROOT_DIR/src/self_hosted/mir_lower" \
    "$ROOT_DIR/src/self_hosted/compiler")"
c_reference_loc="$(count_c_lines \
    "$ROOT_DIR/src/lexer" "$ROOT_DIR/src/parser" \
    "$ROOT_DIR/src/semantic" "$ROOT_DIR/src/codegen" \
    "$ROOT_DIR/src/runtime" "$ROOT_DIR/src/compiler" "$ROOT_DIR/src/lsp")"

[ "$pgy_frontend_backend_loc" -gt 0 ] || fail "Pergyra implementation inventory is empty"
[ "$pgy_compiler_core_loc" -ge "$pgy_frontend_backend_loc" ] ||
    fail "compiler-core inventory is smaller than its frontend/backend subset"
[ "$c_reference_loc" -gt 0 ] || fail "C reference inventory is empty"

ratio="$(awk -v p="$pgy_frontend_backend_loc" -v c="$c_reference_loc" \
    'BEGIN { printf "%.2f", (100.0 * p) / c }')"
printf '[self-host-progress] implementation_frontend_backend_loc=%s compiler_core_loc=%s c_reference_loc=%s implementation_volume_ratio=%s%%\n' \
    "$pgy_frontend_backend_loc" "$pgy_compiler_core_loc" "$c_reference_loc" "$ratio"
