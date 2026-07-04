#!/usr/bin/env bash
#
# selfhost_driver_lsp_wiring_smoke.sh — docs/150's rung ladder is a CONTRACT.
# A rung marked `landed` must name an artifact and a gate that exist on disk;
# a rung marked `blocked` must name both plus an explicit blocker; a rung
# marked `planned` must claim neither. This blocks fake self-host
# progress in either direction: claiming what does not exist, and building
# what the ladder never registered.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DOC="$ROOT_DIR/docs/150_selfhost_driver_lsp_wiring.md"

fail() { echo "[driver-lsp-wiring] FAIL: $*" >&2; exit 1; }

[ -f "$DOC" ] || fail "missing docs/150_selfhost_driver_lsp_wiring.md"

rows="$(sed -n '/DRIVER-LSP-RUNG-BEGIN/,/DRIVER-LSP-RUNG-END/p' "$DOC" \
    | grep -E '^\| (driver|lsp) \|')"
[ -n "$rows" ] || fail "docs/150 rung block has no rows"

expected_rungs="DRV-0 DRV-1 DRV-2 DRV-3 LSP-0 LSP-1 LSP-2 LSP-3"
for rung in $expected_rungs; do
    printf '%s\n' "$rows" | grep -Fq "| $rung |" ||
        fail "rung table lost row '$rung' (ladder rows may change status, not vanish)"
done

while IFS='|' read -r _ track rung status artifact gate _; do
    track="$(echo "$track" | tr -d ' ')"
    rung="$(echo "$rung" | tr -d ' ')"
    status="$(echo "$status" | tr -d ' ')"
    artifact="$(echo "$artifact" | tr -d ' ')"
    gate="$(echo "$gate" | tr -d ' ')"
    case "$track" in
        driver|lsp) ;;
        *) fail "$rung: unknown track '$track'" ;;
    esac
    case "$status" in
        landed)
            [ "$artifact" != "-" ] || fail "$rung is landed but names no artifact"
            [ "$gate" != "-" ] || fail "$rung is landed but names no gate"
            [ -e "$ROOT_DIR/$artifact" ] ||
                fail "$rung: landed artifact '$artifact' does not exist"
            [ -e "$ROOT_DIR/$gate" ] ||
                fail "$rung: landed gate '$gate' does not exist"
            ;;
        blocked)
            [ "$artifact" != "-" ] || fail "$rung is blocked but names no artifact"
            [ "$gate" != "-" ] || fail "$rung is blocked but names no gate"
            [ -e "$ROOT_DIR/$artifact" ] ||
                fail "$rung: blocked artifact '$artifact' does not exist"
            [ -e "$ROOT_DIR/$gate" ] ||
                fail "$rung: blocked gate '$gate' does not exist"
            grep -Fq "$rung blocker" "$DOC" ||
                fail "$rung is blocked but its blocker is not documented"
            ;;
        planned)
            [ "$artifact" = "-" ] && [ "$gate" = "-" ] ||
                fail "$rung is planned but claims artifact/gate (land it or clear the claim)"
            ;;
        *) fail "$rung: unknown status '$status'" ;;
    esac
done < <(printf '%s\n' "$rows")

# The gap register must stay visible until its rungs land: G-EXEC blocks
# DRV-2 and G-STDIN blocks LSP-2 — deleting the register while those rungs
# are planned would hide the honest boundary.
if printf '%s\n' "$rows" | grep -Fq "| DRV-2 | planned |"; then
    grep -Fq "G-EXEC" "$DOC" || fail "DRV-2 is planned but the G-EXEC gap entry vanished"
fi
if printf '%s\n' "$rows" | grep -Fq "| LSP-2 | planned |"; then
    grep -Fq "G-STDIN" "$DOC" || fail "LSP-2 is planned but the G-STDIN gap entry vanished"
fi

echo "[driver-lsp-wiring] rung ladder honest (landed==exists, blocked==documented, planned==unclaimed, gaps visible)"
