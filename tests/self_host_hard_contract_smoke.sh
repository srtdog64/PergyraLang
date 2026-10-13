#!/usr/bin/env bash
# Gates the hard self-host contract: SoT closure is a substitution pass
# condition, C/LLVM remain the oracle pair, and active hard rungs stay wired
# into the preparation gate.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

fail() {
    echo "[self-host-hard-contract] $*" >&2
    exit 1
}

require_file() {
    local rel="$1"
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
}

bash "$ROOT_DIR/tests/mir_json_expression_graph_owner_smoke.sh"

shared_nominal_total=0
while IFS='|' read -r owner cap; do
    require_file "src/self_hosted/compiler/$owner"
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "src/self_hosted/compiler/$owner")" || fail "source-size measurement failed"
    [[ "$lines" -le "$cap" ]] || fail "$owner hard cap exceeded: $lines/$cap"
    shared_nominal_total=$((shared_nominal_total + lines))
done <<'SHARED_NOMINAL_OWNER_CAPS'
direct_mir_nominal_literal_declaration_fact_owner.pgy|180
direct_mir_nominal_literal_graph_fact_owner.pgy|150
direct_mir_nominal_literal_program_identity_owner.pgy|200
direct_mir_nominal_literal_route_fact_owner.pgy|110
direct_mir_nominal_literal_instruction_envelope_owner.pgy|100
direct_mir_nominal_literal_abi_absence_owner.pgy|120
direct_mir_nominal_literal_program_admission_owner.pgy|220
direct_mir_nominal_literal_projection_owner.pgy|110
SHARED_NOMINAL_OWNER_CAPS
[[ "$shared_nominal_total" -le 900 ]] || \
    fail "shared nominal literal owner cap exceeded: $shared_nominal_total/900"
mutable_identity_total=0
while IFS='|' read -r owner cap; do
    require_file "src/self_hosted/compiler/$owner"
    lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "src/self_hosted/compiler/$owner")" ||
        fail "source-size measurement failed: $owner"
    [[ "$lines" -le "$cap" ]] || fail "$owner hard cap exceeded: $lines/$cap"
    mutable_identity_total=$((mutable_identity_total + lines))
done <<'MUTABLE_IDENTITY_OWNER_CAPS'
direct_mir_mutable_nominal_identity_plan_owner.pgy|180
direct_mir_mutable_nominal_identity_target_projection_owner.pgy|100
direct_mir_mutable_nominal_identity_c_emission_owner.pgy|90
direct_mir_mutable_nominal_identity_llvm_emission_owner.pgy|90
MUTABLE_IDENTITY_OWNER_CAPS
[[ "$mutable_identity_total" -le 384 ]] || \
    fail "mutable nominal identity owner cap exceeded: $mutable_identity_total/384"
require_file \
    "src/self_hosted/compiler/direct_mir_exact_json_array_cardinality_owner.pgy"
source_size_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" src/self_hosted/compiler/direct_mir_exact_json_array_cardinality_owner.pgy)" || fail "source-size measurement failed"
[[ "$source_size_lines" -le 90 ]] || \
    fail "exact JSON-array cardinality owner hard cap exceeded"

echo "[self-host-hard-contract] hard substitution contract is wired"
