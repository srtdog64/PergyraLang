#!/usr/bin/env bash
# The canonical Gate SoT is gate_dashboard_owner.pgy. This is a subcheck of
# sot-authority-edge-test-smoke, not a second dashboard gate identity.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OWNER_REL="src/self_hosted/compiler/gate_dashboard_owner.pgy"
OWNER="$ROOT_DIR/$OWNER_REL"

fail() {
    echo "[gate-sot-single-owner] $*" >&2
    exit 1
}

[[ -f "$OWNER" ]] || fail "missing canonical Gate SoT: $OWNER_REL"

grep -Fq 'pgy.selfhost.gate-dashboard.v1' "$OWNER" ||
    fail "canonical Gate SoT schema drifted"
grep -Fq 'func CompilerGateDashboardManifestCurrent(' "$OWNER" ||
    fail "canonical Gate SoT manifest owner drifted"

definitions="$({
    grep -RIl --include='*.pgy' 'func CompilerGateDashboardManifestCurrent(' \
        "$ROOT_DIR/src/self_hosted" || true
} | sed "s#^$ROOT_DIR/##")"
definition_count="$(printf '%s\n' "$definitions" | awk 'NF { count++ } END { print count + 0 }')"
[[ "$definition_count" -eq 1 && "$definitions" == "$OWNER_REL" ]] ||
    fail "gate manifest has more than one owner: ${definitions:-none}"

# Gate identity/status must not be copied into a second live inventory. The
# golden JSON and result TSV are allowed projections; the dashboard owner is
# the only place that defines CompilerGateDashboardManifestCurrent.
if grep -Eq 'CLOSED=[0-9]+ BRIDGE=[0-9]+ ACTIVE=[0-9]+' \
    "$ROOT_DIR/tests/proof_spine_smoke.sh"; then
    fail "proof spine copied a SoT status count"
fi
if grep -Fq 'Current registry contains 36 authority rows' \
    "$ROOT_DIR/docs/185_sot_gate_catalog.md"; then
    fail "gate catalog copied a historical authority count as current"
fi

# The parity consumer must derive its row count from the canonical manifest,
# not pin a second count in shell.
grep -Fq '[[ "$row_count" -gt 0 ]]' \
    "$ROOT_DIR/tests/self_hosted/parity/gate_dashboard_parity.sh" ||
    fail "dashboard parity still owns a copied manifest row count"

grep -Fq 'single Gate SoT' "$ROOT_DIR/docs/185_sot_gate_catalog.md" ||
    fail "gate catalog does not declare the single Gate SoT policy"
grep -Fq 'sot-authority-edge-test-smoke' "$ROOT_DIR/Makefile" ||
    fail "canonical SoT gate target is missing"
grep -Fq 'tests/gate_sot_single_owner_smoke.sh' "$ROOT_DIR/Makefile" ||
    fail "single-owner subcheck is not attached to the canonical SoT gate"
grep -Fq 'tests/protocol_registry_smoke.sh' "$ROOT_DIR/Makefile" ||
    fail "protocol crosswalk is not attached to the canonical SoT gate"
if grep -Fq 'protocol-registry-test-smoke:' "$ROOT_DIR/Makefile"; then
    fail "protocol crosswalk was reintroduced as a second gate target"
fi

echo "[gate-sot-single-owner] canonical gate_dashboard_owner.pgy is the only Gate SoT"
