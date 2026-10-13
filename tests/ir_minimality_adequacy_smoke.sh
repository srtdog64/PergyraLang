#!/usr/bin/env bash
#
# IR minimality adequacy smoke: backend codegen must not consume DIR/AIR.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

require_no_codegen_dependency() {
    local matches
    matches="$(
        grep -RInE -- \
            '\b(AIRProgram|DIRProgram|air_(synthesize|verify|collect|append|boundary|evidence|dump)|dir_(lower|validate|find|dump))\b' \
            "$ROOT_DIR/src/codegen" \
            || true
    )"
    if [[ -n "$matches" ]]; then
        echo "[ir-minimality] backend codegen reopened DIR/AIR as a codegen dependency" >&2
        printf '%s\n' "$matches" >&2
        exit 1
    fi
}

require_no_codegen_dependency

echo "[ir-minimality] backend codegen does not depend on DIR/AIR"
