#!/usr/bin/env bash
# Pins the canonical grammar cheat-sheet and its semicolon register.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
SHEET="docs/grammar/00_cheatsheet.md"
fail=0

require_text() {
    if ! grep -qF "$2" "$1"; then
        echo "[FAIL] $1 no longer states: $2"
        fail=1
    fi
}

if [ ! -f "$SHEET" ]; then
    echo "[FAIL] missing canonical grammar cheat-sheet: $SHEET"
    exit 1
fi

require_text "$SHEET" '`zone` / `world` / `effect` / `relation` 본문의 *사실 선언*은 `;` 없음'
require_text "$SHEET" '상위 세계'
require_text "$SHEET" '선택적 zone 계약 fact'
require_text "$SHEET" 'require x: Int'
require_text "$SHEET" '같은 *개념*이 다른 *철자/구두점* = 버그'
require_text "$SHEET" 'hosted receiver는 별도 축이다'
require_text "$SHEET" 'grammar-cheatsheet-contract-test-smoke'
[ "$fail" -eq 0 ] && echo "[PASS] cheat-sheet contract present"

# The parser still tolerates semicolons in this layer, so this style ratchet
# scans authored tests while parser-tolerance fixtures remain out of scope.
WORLD_FACT='^[[:space:]]*(apply|refresh|maintain|link|activate)[[:space:]].*;[[:space:]]*$'
hits="$(grep -rnE "$WORLD_FACT" tests --include='*.pgy' 2>/dev/null \
    | grep -v '^tests/self_hosted/parity/fixture/domain_topology_semicolon_legacy.pgy:' \
    || true)"
if [ -n "$hits" ]; then
    echo "[FAIL] world-layer fact carries ';' in authored tests:"
    echo "$hits" | sed 's/^/    /'
    fail=1
else
    echo "[PASS] semicolon register obeyed in tests/"
fi

if [ "$fail" -eq 0 ]; then
    echo "ALL PASS (grammar cheat-sheet contract)"
    exit 0
fi

echo "FAILED"
exit 1
