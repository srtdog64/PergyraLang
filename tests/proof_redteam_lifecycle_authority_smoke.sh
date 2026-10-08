#!/usr/bin/env bash
# Independent fresh kernel gate for the lifecycle/authority red-team repairs.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
snapshot="$(mktemp -d)"
trap 'rm -rf "$snapshot"' EXIT
owners=(PergyraCore PergyraCoreComposition PergyraCoreZoneBridge UnifiedCore
        WholeProgramCore AIRBinding BinaryAdequacy MachineLayerCore
        CapabilityFlowCore ModuleAuthority
        BindingIdentityScope PartySlotBinding EvidenceLifecycleCore
        ResourceMachineBridge DelegationBoundaryCore AxisOwnership
        AuthorityIrreducibility ReadingConfluence)
for owner in "${owners[@]}"; do
    cp "$ROOT_DIR/docs/semantics/proofs/$owner.v" "$snapshot/"
done
cp "$ROOT_DIR/tests/coq/LifecycleAuthorityRedteamRegression.v" "$snapshot/"
if grep -Fq 'Definition with_target_deleg' "$snapshot/PergyraCore.v"; then
    echo 'lifecycle-authority: FAIL -- retired bulk capability fabrication returned' >&2
    exit 1
fi
PGY_COQ_PROOFS_DIR="$snapshot" PGY_COQ_EXPECTED_AXIOMS='' \
    bash "$ROOT_DIR/tests/coq_kernel_check.sh"
