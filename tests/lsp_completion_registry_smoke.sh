#!/usr/bin/env bash
# Proves LSP completion is a bounded projection of LanguageKeywordRegistry.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROTOCOL="$ROOT_DIR/src/lsp/pgy_lsp_protocol.c"
INTERNAL="$ROOT_DIR/src/lsp/pgy_lsp_internal.h"
CALLSITE="$ROOT_DIR/src/lsp/pgy_lsp.c"
CC_BIN="${PGY_CC:-${CC:-cc}}"
TMP_DIR="$(mktemp -d)"
PROBE="$TMP_DIR/lsp-completion-registry-probe"

cleanup() {
    rm -f -- "$TMP_DIR/lsp-completion-registry-probe" \
        "$TMP_DIR/lsp-completion-registry-probe.exe"
    rmdir -- "$TMP_DIR"
}
trap cleanup EXIT

fail() {
    echo "[lsp-completion-registry] $*" >&2
    exit 1
}

grep -Fq 'lexer_keyword_registry_count()' "$PROTOCOL" ||
    fail "completion owner does not enumerate the registry"
grep -Fq 'lexer_keyword_registry_row(row_index)' "$PROTOCOL" ||
    fail "completion owner does not read registry rows"
grep -Fq 'PGY_KEYWORD_TOOLING_COMPLETION' "$PROTOCOL" ||
    fail "completion owner does not select the registry completion flag"
grep -Fq 'lsp_completion_items_json()' "$CALLSITE" ||
    fail "completion request does not consume the registry projection"
grep -Fq 'lsp_build_completion_items_json' "$INTERNAL" ||
    fail "completion builder contract is not declared"

if grep -Eq '\\"label\\":\\"[A-Za-z_]' "$PROTOCOL"; then
    fail "word-specific completion JSON labels reappeared"
fi
if grep -Eq 'const[[:space:]]+char[[:space:]]*\*[[:space:]]*lsp_completion_items[[:space:]]*=' "$PROTOCOL"; then
    fail "the removed hardcoded completion array reappeared"
fi

"$CC_BIN" -std=c11 -Wall -Wextra -Werror -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/lsp_completion_registry_probe.c" \
    "$ROOT_DIR/src/lsp/pgy_lsp_protocol.c" \
    "$ROOT_DIR/src/lexer/lexer_keywords.c" \
    "$ROOT_DIR/src/common/numeric_parse.c" \
    -o "$PROBE"

if [[ -x "$PROBE.exe" ]]; then
    PROBE="$PROBE.exe"
fi
"$PROBE"
