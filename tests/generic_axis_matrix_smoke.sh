#!/usr/bin/env bash
#
# generic_axis_matrix_smoke.sh — docs/151 is a CONTRACT now that Decision-0
# and GATE are closed (BDFL 2026-07-04). This locks:
#   1. the closure records themselves (a future edit cannot silently reopen
#      or drop the signed decisions),
#   2. the GATE anti-abuse clause (the array-covariance firewall),
#   3. §5 constructor rows (they may change verdicts, not vanish),
#   4. §6 sketch tier (speculative constructors stay verdict-free),
#   5. §8 G-rung ladder honesty (landed => artifact+gate exist on disk;
#      planned => no claims) — same discipline as docs/150.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOC="$ROOT_DIR/docs/151_generic_axis_composition.md"

fail() { echo "[generic-axis-matrix] FAIL: $*" >&2; exit 1; }
require() { grep -Fq "$1" "$DOC" || fail "docs/151 lost required text: $1"; }

[ -f "$DOC" ] || fail "missing docs/151_generic_axis_composition.md"

# 1) closed decisions stay recorded (and cannot silently reopen)
require "Decision-0 — 축의 운반 방식 (CLOSED, BDFL 2026-07-04)"
require "carriage 기본값 = **positional**"
require "판정값 — 5값 (CLOSED, BDFL 2026-07-04)"
if grep -Eq "Decision-0[^(]*\(OPEN|GATE, OPEN" "$DOC"; then
    fail "docs/151 reopened a closed decision without a new closure record"
fi

# 2) GATE anti-abuse clause (array-covariance firewall)
require "GATE 남용 금지 조항"
require "정적 판정이 *아직 안 만들어진* 곳은 GATE가 아니라 DEFER"

# 2b) axis-set revision record (BDFL 2026-07-04): 6 axes — axis-6 is
# spelled lowercase `slot` (type stays Slot<T>), and Phase is NOT an
# axis (it is the ERASE codomain). A silent revert to 7 axes or to the
# "Site" spelling must trip this gate.
require "6축 — 재심(BDFL 2026-07-04): 7축 → 6축"
require "축은 소문자 slot, 타입은 항상"
require "**Phase 축 제거.**"
require "축 입장 조건"
require "표면 착지점"
require "정적 경계 vs 런타임 존재"
if grep -Fq "| **GATE** (제안)" "$DOC"; then
    fail "docs/151 still marks closed GATE verdict as a proposal"
fi

# 3) §5 constructor rows may change verdicts, never vanish
for row in 'Option<T>' 'List<T>' 'Slot<T>' 'Channel<T>' 'own' 'StrView'; do
    grep -Fq "$row" "$DOC" || fail "§5 lost constructor row: $row"
done

# 4) sketch tier stays verdict-free
require "판정 금지, 어휘 선점만"

# 5) rung ladder honesty
rows="$(sed -n '/GENERIC-RUNG-BEGIN/,/GENERIC-RUNG-END/p' "$DOC" \
    | grep -E '^\| G-[0-9]')"
[ -n "$rows" ] || fail "docs/151 §8 rung block has no rows"
for rung in G-1 G-2 G-3 G-4 G-5 G-6; do
    printf '%s\n' "$rows" | grep -Fq "| $rung " ||
        fail "rung table lost row '$rung' (rows change status, not vanish)"
done
while IFS='|' read -r _ rung _cell status artifact gate _; do
    rung="$(echo "$rung" | tr -d ' ')"
    status="$(echo "$status" | tr -d ' ')"
    artifact="$(echo "$artifact" | tr -d ' ')"
    gate="$(echo "$gate" | tr -d ' ')"
    case "$status" in
        landed)
            [ "$artifact" != "-" ] || fail "$rung landed but names no artifact"
            [ "$gate" != "-" ] || fail "$rung landed but names no gate"
            [ -e "$ROOT_DIR/$artifact" ] || fail "$rung artifact '$artifact' missing"
            [ -e "$ROOT_DIR/$gate" ] || fail "$rung gate '$gate' missing"
            ;;
        planned)
            [ "$artifact" = "-" ] && [ "$gate" = "-" ] ||
                fail "$rung planned but claims artifact/gate"
            ;;
        *) fail "$rung: unknown status '$status'" ;;
    esac
done < <(printf '%s\n' "$rows")

# ERASE duty survives (every future ERASE cell must name its docs/14 bucket)
require "docs/14 버킷 명명 의무"

# concept-audit locks (2026-07-04): measured edges registered, DEFER duty,
# advisory-is-not-a-verdict firewall
require "world ⊃ zone"
require "intent ⊨ transfer"
require "DEFER 명명 의무"
require "advisory는 판정값이 아니다"
require "저장-매개 흐름"

echo "[generic-axis-matrix] contract locked (decisions closed, rows stable, rungs honest)"
