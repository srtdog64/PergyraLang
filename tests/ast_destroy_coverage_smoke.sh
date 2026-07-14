#!/usr/bin/env bash
set -euo pipefail

# Meta-gate: every ASTNodeType must state a destroy policy.
#
# ast_destroy splits its switch in two -- ast_destroy_domain_node() takes the
# domain kinds and returns true, ast_destroy() handles the rest -- so neither
# switch can drop its `default:` label, and -Wswitch cannot see an omission.
# That is how AST_ARRAY_LITERAL came to sit in `default: break;` and leak its
# entire element subtree on every parse: nothing failed, because a leak fails
# no test. The ASan gate (make test-asan) found it; this gate keeps the class
# shut, by requiring each kind to appear in one of the two switches. A kind
# that owns nothing on the heap still gets a case label with a bare `break;`
# -- "nothing to free" is a decision, and decisions are on the record.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TYPES_H="$ROOT_DIR/src/parser/ast_types.h"
DESTROY_C="$ROOT_DIR/src/parser/ast_destroy.c"
DESTROY_DOMAIN_C="$ROOT_DIR/src/parser/ast_destroy_domain.c"

for f in "$TYPES_H" "$DESTROY_C" "$DESTROY_DOMAIN_C"; do
    if [[ ! -f "$f" ]]; then
        echo "[ast-destroy-coverage] missing owner file: $f" >&2
        echo "  The destroy switches moved. Re-point this gate at their new" >&2
        echo "  home rather than deleting it." >&2
        exit 1
    fi
done

# Kinds: the ASTNodeType enum body only (the file has other enums).
mapfile -t KINDS < <(
    awk '/^typedef enum$/,/} ASTNodeType;/' "$TYPES_H" \
    | grep -oE '\bAST_[A-Z0-9_]+\b' \
    | sort -u
)

if (( ${#KINDS[@]} == 0 )); then
    echo "[ast-destroy-coverage] parsed zero AST kinds from $TYPES_H" >&2
    echo "  The enum shape changed; a gate that matches nothing passes" >&2
    echo "  vacuously, so this is a failure, not a skip." >&2
    exit 1
fi

HANDLED="$(grep -hoE 'case[[:space:]]+AST_[A-Z0-9_]+[[:space:]]*:' \
    "$DESTROY_C" "$DESTROY_DOMAIN_C" \
    | grep -oE 'AST_[A-Z0-9_]+' | sort -u)"

missing=()
for kind in "${KINDS[@]}"; do
    if ! grep -qxF "$kind" <<< "$HANDLED"; then
        missing+=("$kind")
    fi
done

if (( ${#missing[@]} > 0 )); then
    echo "[ast-destroy-coverage] AST kinds with no destroy policy:" >&2
    for kind in "${missing[@]}"; do
        echo "  - $kind" >&2
    done
    echo >&2
    echo "Each kind must appear as a 'case' in ast_destroy.c or" >&2
    echo "ast_destroy_domain.c. If the kind owns heap payload, free it there." >&2
    echo "If it owns nothing, add the case with a bare 'break;' so the" >&2
    echo "verdict is recorded instead of inherited from 'default:'." >&2
    exit 1
fi

echo "[ast-destroy-coverage] ${#KINDS[@]} AST kinds, all with a destroy policy"
