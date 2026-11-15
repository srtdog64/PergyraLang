#!/usr/bin/env bash
# HIR must consume the semantic function-parameter summary snapshot by
# stable SyntaxNodeId and reject malformed or unattachable rows. Both cases
# execute in test_hir; a failed expectation makes it exit non-zero.

set -euo pipefail

SCRIPT_DIR="${BASH_SOURCE[0]%/*}"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
HIR_TEST="${HIR_TEST_BIN:-$ROOT_DIR/bin/test_hir}"
if [[ -n "${HIR_TEST_BIN:-}" ]]; then
    : # caller supplied the platform-specific test path
elif [[ ! -x "$HIR_TEST" && -x "$ROOT_DIR/bin/test_hir.exe" ]]; then
    HIR_TEST="$ROOT_DIR/bin/test_hir.exe"
fi

if [[ ! -x "$HIR_TEST" ]]; then
    echo "[hir-function-param-flow] missing HIR test binary: $HIR_TEST" >&2
    exit 1
fi

OUTPUT="$($HIR_TEST)"
[[ "$OUTPUT" == *"HIR carries function parameter flow summaries by stable SyntaxNodeId"* ]]
[[ "$OUTPUT" == *"HIR rejects a function parameter flow fact for an unknown routine"* ]]

echo "[hir-function-param-flow] semantic summary rows reach HIR with stable identity and validation"
