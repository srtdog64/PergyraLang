#!/usr/bin/env bash
# Proves the canonical TextMate grammar is an exact projection of registry
# rows owned for syntax highlighting.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/src/lexer/language_keyword_registry.def"
PROJECTION="$ROOT_DIR/src/self_hosted/lexer/language_keyword_registry_projection_owner.pgy"
GRAMMAR="$ROOT_DIR/editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B \
    "$ROOT_DIR/scripts/render_language_keyword_registry.py" \
    "$REGISTRY" "$PROJECTION" --check --textmate-grammar "$GRAMMAR"
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[vscode-language-graph] python3/python is required" >&2
    exit 1
fi
