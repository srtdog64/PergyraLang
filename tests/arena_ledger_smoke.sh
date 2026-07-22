#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CC_BIN=${CC:-gcc}
BIN="$ROOT_DIR/build/pergyra_arena_ledger_smoke_$$"
trap 'rm -f "$BIN"' EXIT

"$CC_BIN" -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR/src" \
    "$ROOT_DIR/tests/arena_ledger_smoke.c" \
    "$ROOT_DIR/src/common/arena.c" \
    -o "$BIN"
"$BIN"
