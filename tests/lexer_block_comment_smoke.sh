#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy_lexer_comment.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I "$ROOT_DIR/src" \
    "$ROOT_DIR/tests/lexer_block_comment_test.c" \
    "$ROOT_DIR/src/lexer/lexer.c" "$ROOT_DIR/src/lexer/lexer_keywords.c" \
    "$ROOT_DIR/src/common/arena.c" -o "$WORK_DIR/lexer-comment"
if [[ -f "$WORK_DIR/lexer-comment.exe" ]]; then
    "$WORK_DIR/lexer-comment.exe"
else
    "$WORK_DIR/lexer-comment"
fi
