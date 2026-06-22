#!/usr/bin/env bash
# Grammar cheat-sheet contract gate.
#
# Enforces the canonical surface rules consolidated in docs/grammar/00_cheatsheet.md
# so they cannot silently drift. Two parts:
#   (A) the cheat-sheet exists and still states the load-bearing rules;
#   (B) the SEMICOLON REGISTER is obeyed in AUTHORED example code: zone/world/
#       effect/relation bodies are the declarative "world layer" and their fact
#       declarations carry NO `;`. The unambiguous world-fact verbs (apply/refresh/
#       maintain/link/activate) must never end an authored line with `;`.
#
# SCOPE NOTE: the parser TOLERATES `;` in world-layer bodies today (the register is
# a canonical STYLE, not yet a hard parser rule -- see src/self_hosted/parser/
# fixture/zone_lifecycle.pgy, which uses `;` to exercise the tolerant path). So
# this STYLE gate scans authored canonical examples under tests/ only and exempts
# parser-tolerance fixtures under src/self_hosted/. Making the register
# parser-enforced (reject `;` there) is the future step recorded in the cheat-sheet.
#
# Governing principle (docs/grammar/00_cheatsheet.md, docs/134): same CONCEPT with
# different spelling/punctuation = bug; different LAYER/AXIS with different
# punctuation/vocabulary = orthogonality (fine). The semicolon split is the latter.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
SHEET="docs/grammar/00_cheatsheet.md"
fail=0

# ---- (A) cheat-sheet contract present ----
if [ ! -f "$SHEET" ]; then
    echo "[FAIL] missing canonical grammar cheat-sheet: $SHEET"
    echo "FAILED"; exit 1
fi
require_text() {
    if ! grep -qF "$2" "$1"; then
        echo "[FAIL] $1 no longer states: $2"
        fail=1
    fi
}
require_text "$SHEET" "zone·world·effect·relation 본문만"      # the semicolon register
require_text "$SHEET" "상위 세계"                               # the world-layer rationale
require_text "$SHEET" "선택적 zone 제약"                        # within (action) vs where (step)
require_text "$SHEET" "require x: Int"                          # fields -> require direction
require_text "$SHEET" "같은 *개념*이 다른 *철자/구두점* = 버그"   # governing principle
[ "$fail" -eq 0 ] && echo "[PASS] cheat-sheet contract present"

# ---- (B) semicolon register obeyed in authored examples (tests/ only) ----
# Robust single grep -r (no process substitution -- that silently no-ops under
# some bash builds). World-fact verbs that must never end an authored line with ';'.
WORLD_FACT='^[[:space:]]*(apply|refresh|maintain|link|activate)[[:space:]].*;[[:space:]]*$'
hits="$(grep -rnE "$WORLD_FACT" tests --include='*.pgy' 2>/dev/null)"
if [ -n "$hits" ]; then
    echo "[FAIL] world-layer fact carries ';' (register violation) in authored examples:"
    echo "$hits" | sed 's/^/    /'
    echo "  fix: drop the ';' -- zone/world/effect/relation bodies are the declarative"
    echo "  world layer (docs/grammar/00_cheatsheet.md §1)."
    fail=1
else
    echo "[PASS] semicolon register obeyed in tests/ (no world-fact line ends with ';')"
fi

if [ "$fail" -eq 0 ]; then
    echo "ALL PASS (grammar cheat-sheet contract)"; exit 0
fi
echo "FAILED"; exit 1
