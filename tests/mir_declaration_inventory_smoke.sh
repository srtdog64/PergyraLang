#!/usr/bin/env bash
# Regression gate for C/LLVM declaration-side MIR inventory usage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[mir-decl-inventory] FAIL" >&2
    echo "  - $*" >&2
    exit 1
}

for owner in \
    "src/compiler/mir_source_local_type_shape.c" \
    "src/compiler/mir_source_local_expr_binding_facts.c" \
    "src/compiler/mir_source_local_expr_call_facts.c" \
    "src/compiler/mir_source_local_expr_types.c"; do
    owner_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$ROOT_DIR/$owner")" || fail "source-size measurement failed"
    if (( owner_lines >= 600 )); then
        fail "$owner is ${owner_lines} LOC; source-local type owners must stay below 600"
    fi
done

echo "[mir-decl-inventory] OK: source-local type owners stay below 600 lines"
