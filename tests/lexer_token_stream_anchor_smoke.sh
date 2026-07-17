#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

require_text() {
    local file="$1"
    local text="$2"
    if ! grep -Fq -- "$text" "$ROOT_DIR/$file"; then
        echo "[lexer-token-stream-anchor] missing $file contract: $text" >&2
        exit 1
    fi
}

require_text src/lexer/lexer.h "typedef struct"
require_text src/lexer/lexer.h "PgyTokenStreamHandle"
require_text src/lexer/lexer.h "source_fingerprint"
require_text src/lexer/lexer.h "uint32_t    ordinal;"
require_text src/lexer/lexer.c "lexer_source_fingerprint"
require_text src/lexer/lexer.c "token.stream = lexer->stream"
require_text src/parser/parser.c "parser token stream anchor changed during parse"
require_text src/parser/parser.c "lexer_token_stream_handle_equal"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[lexer-token-stream-anchor] missing compiler binary: $PGY" >&2
    exit 1
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_lexer_anchor.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
TOKENS="$WORK_DIR/tokens.txt"
(cd "$ROOT_DIR" && "$PGY" --tokens \
    "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/examples/basic.pgy")" \
    >"$TOKENS")
grep -Fq 'Token{type: FUNC, text: "func"' "$TOKENS"
if grep -Fq 'source_fingerprint' "$TOKENS" || grep -Fq 'stream=' "$TOKENS"; then
    echo "[lexer-token-stream-anchor] token output leaked anchor internals" >&2
    exit 1
fi

echo "[lexer-token-stream-anchor] lexer and parser share one stable token-stream anchor"
