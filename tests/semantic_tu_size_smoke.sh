#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_LIMIT="${SEMANTIC_TU_MAX_LINES:-1000}"

cd "$ROOT_DIR"

cap_for_path() {
    case "$1" in
        src/semantic/type_checker_decls_domain_helpers.c)
            echo 1700
            ;;
        src/semantic/type_checker_intent_helpers.c)
            echo 1600
            ;;
        src/semantic/slot_analyzer.c)
            echo 1250
            ;;
        src/semantic/type_checker_builtins_stdlib_body.c)
            echo 1200
            ;;
        src/semantic/type_checker_zone_decl.c)
            echo 1150
            ;;
        src/semantic/type_system.c|src/semantic/type_system_function.c)
            echo 599
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
done < <(find src/semantic -name '*.c' -print0)

if [ -n "$violations" ]; then
    echo "[semantic-tu-size] semantic TU size violation(s):" >&2
    printf '%s' "$violations" >&2
    echo "Split by owner axis instead of moving multiple behavior families into one TU." >&2
    exit 1
fi

echo "[semantic-tu-size] semantic .c files stay within owner-size caps"
