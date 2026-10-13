#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

"${BASH:-bash}" "$ROOT_DIR/tests/lexer_block_comment_smoke.sh"
