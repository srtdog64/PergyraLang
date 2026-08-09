#!/usr/bin/env bash
# Keeps hard self-host work tied to executable replacement instead of allowing
# unbounded SoT-only preparation.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROCESS_DOC="docs/self_hosted/16_hard_substitution_velocity_process.md"
LEDGER="docs/self_hosted/15_pre_self_host_expansion_ledger.md"

fail() {
    echo "[self-host-substitution-velocity] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

for rel in "$PROCESS_DOC" "$LEDGER" "src/self_hosted/PROGRESS.md" "AGENTS.md"; do
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
done

require_text "$PROCESS_DOC" "nine ACTIVE blockers"
require_text "$PROCESS_DOC" "Five are direct substitution blockers"
require_text "$PROCESS_DOC" "Process/evidence blockers:"
require_text "$PROCESS_DOC" "Released/default replacement is 0 percent"
require_text "$PROCESS_DOC" "Current target-specific status (2026-08-10)"
require_text "$PROCESS_DOC" \
    'local package compiler execution is bounded `SUBSTITUTING`'
require_text "$PROCESS_DOC" \
    "dispatch that returns before the ordinary source selector"
require_text "$PROCESS_DOC" \
    "never authorize a native retry."
require_text "$PROCESS_DOC" "two consecutive SoT-only commits"
require_text "$PROCESS_DOC" "70 percent executable hard substitution"
require_text "$PROCESS_DOC" "20 percent build and test feedback reduction"
require_text "$PROCESS_DOC" "10 percent SoT, process, and documentation maintenance"
require_text "$PROCESS_DOC" "60 seconds"
require_text "$PROCESS_DOC" "5 minutes"
require_text "$PROCESS_DOC" "30 minutes"
require_text "$PROCESS_DOC" "mixed AST-like expression bridge"
require_text "$PROCESS_DOC" 'leave `typed ? text` dual-read authority'

require_text "$LEDGER" "exactly nine ACTIVE rows"
require_text "src/self_hosted/PROGRESS.md" "Classification is target-specific"
require_text "src/self_hosted/PROGRESS.md" "70/20/10 effort split"
require_text "AGENTS.md" "SoT is a hard-substitution rung condition"
require_text "AGENTS.md" "more than two consecutive SoT-only commits"

active_rows="$({
    awk '
        /^## Active Blockers$/ { in_active = 1; next }
        in_active && /^TestHarness delta/ { exit }
        in_active && /^\| [^|]+ \| `?[^|]+ \|/ &&
            $0 !~ /^\| Blocker \|/ { count++ }
        END { print count + 0 }
    ' "$ROOT_DIR/$LEDGER"
} || true)"
[[ "$active_rows" == "9" ]] ||
    fail "active blocker table drifted: expected 9 rows, got $active_rows"

if grep -Fq -- "SoT must be fully closed before self-hosting" \
    "$ROOT_DIR/$PROCESS_DOC"; then
    fail "process reopened global SoT-before-self-host sequencing"
fi

echo "[self-host-substitution-velocity] 9 blockers (5 direct, 4 process/evidence); executable-first budgets locked"
