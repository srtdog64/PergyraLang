#!/usr/bin/env bash
#
# Hard self-host readiness scorecard.
#
# The compiler-core gap analysis (docs/self_hosted/05) lists ten non-negotiable
# capabilities that must be available and smoked before a hard self-host can
# start. This scorecard checks that each capability's gate is present and
# reports a readiness tier. It is static: it verifies the gates exist, it does
# not run them, so it needs no build. Run the named gates to exercise behavior.
#
# Tiers:
#   READY    capability is gated and its Phase 1 / mechanism is complete
#   SUBSET   capability works over a limited surface; substrate maturity remains
#   ACTIVE   capability is on the critical path and still in progress
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

missing=0
check_gates() {
    tier="$1"; capability="$2"; shift 2
    present=""
    for g in "$@"; do
        if [ -f "tests/$g" ] || [ -f "$g" ]; then
            present="${present} ${g}"
        else
            echo "[scorecard] MISSING gate for '${capability}': ${g}" >&2
            missing=1
        fi
    done
    printf '  %-7s  %-34s %s\n' "$tier" "$capability" "$present"
}

echo "[scorecard] hard self-host non-negotiable capabilities"
echo
check_gates READY  "1 module/package resolver"    module_smoke.sh package_module_resolver_smoke.sh type_resolution_resolver_inventory_smoke.sh
check_gates READY  "2 collections + iteration"     stdlib_surface_smoke.sh stage4_determinism_smoke.sh
check_gates READY  "3 string/path/unicode policy"  unicode_policy_smoke.sh source_utf8_smoke.sh memory_string_safety_smoke.sh filesystem_directory_walk_smoke.sh
check_gates READY  "4 arena/ownership ergonomics"  verify_arena_closure.sh runtime_abi_lifetime_smoke.sh abi_ownership_shape_smoke.sh
check_gates ACTIVE "5 CFG/MIR body as SoT"         cfg_body_dataflow_smoke.sh ast_read_surface_smoke.sh mir_or_abort_invariant_smoke.sh src/self_hosted/parity/ast_read_surface_checker_parity.sh
check_gates READY  "6 AIR as verifier"             air_json_schema_smoke.sh air_drift_smoke.sh air_backend_nonimpact_smoke.sh
check_gates READY  "7 DAG type resolution SoT"     type_resolution_dag_smoke.sh type_resolution_resolver_inventory_smoke.sh
check_gates READY  "8 scoped unsafe/raw escape"    raw_escape_contract_smoke.sh
check_gates READY  "9 debug info Phase 1"          debug_hygiene_smoke.sh
check_gates READY  "10 runtime profile selection"  runtime_none_contract_smoke.sh

echo
echo "[scorecard] critical path: CFG/MIR body SoT remains ACTIVE until self-hosted MIR lowering replaces transitional ast text with explicit MIR facts"
echo "[scorecard] CFG/MIR SoT is closed for source_ast/source_decl, residual STMT payload emit, raw source-statement re-dispatch, public-surface provenance, and lifecycle dump source-text emission"
if [ "$missing" -ne 0 ]; then
    echo "[scorecard] FAIL: one or more capability gates are missing" >&2
    exit 1
fi
echo "[scorecard] all named capability gates present"
