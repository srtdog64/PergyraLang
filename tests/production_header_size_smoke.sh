#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_LIMIT="${PRODUCTION_HEADER_MAX_LINES:-1000}"

cd "$ROOT_DIR"

cap_for_path() {
    case "$1" in
        src/codegen/llvm_internal.h)
            echo 1600
            ;;
        *)
            echo "$DEFAULT_LIMIT"
            ;;
    esac
}

violations=""
while IFS= read -r -d '' path; do
    lines="$(wc -l < "$path" | tr -d '[:space:]')"
    limit="$(cap_for_path "$path")"
    if [ "$lines" -gt "$limit" ]; then
        violations="${violations}${lines} ${path} > ${limit}"$'\n'
    fi
done < <(find src/codegen src/runtime src/compiler src/semantic -name '*.h' -type f -print0)

if [ -n "$violations" ]; then
    echo "[production-header-size] header owner size violation(s):" >&2
    printf '%s' "$violations" >&2
    echo "Split by feature owner instead of growing behavior-heavy headers." >&2
    exit 1
fi

echo "[production-header-size] production headers stay within owner-size caps"
