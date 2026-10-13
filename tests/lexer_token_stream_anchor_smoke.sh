#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

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

echo "[lexer-token-stream-anchor] lexer_parser_anchor_and_byte_output: one stable token-stream anchor"
