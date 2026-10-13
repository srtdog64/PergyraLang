#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

scope_lookup_escape="$(
    grep -R -n --include='*.c' --include='*.h' \
        'llvm_scope_lookup(ctx,' "$ROOT_DIR/src/codegen" \
        | grep -Fv 'src/codegen/llvm_registry.c:' || true
)"
if [[ -n "$scope_lookup_escape" ]]; then
    echo "[worker-boundary-ub] borrowed LLVM scope lookup escaped the registry owner" >&2
    echo "$scope_lookup_escape" >&2
    exit 1
fi

echo "[worker-boundary-ub] borrowed LLVM scope lookups stay inside the registry owner"
