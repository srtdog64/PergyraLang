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

SCRIPT_PATH="${BASH_SOURCE[0]}"
SCRIPT_DIR="${SCRIPT_PATH%/*}"
if [ "$SCRIPT_DIR" = "$SCRIPT_PATH" ]; then
    SCRIPT_DIR="."
fi
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
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
check_gates READY  "4 arena/ownership ergonomics"  verify_arena_closure.sh runtime_abi_lifetime_smoke.sh abi_ownership_shape_smoke.sh self_host_text_builder_emission_smoke.sh
check_gates READY  "5 CFG/MIR body as SoT"         cfg_body_dataflow_smoke.sh ast_read_surface_smoke.sh mir_or_abort_invariant_smoke.sh tests/self_hosted/parity/ast_read_surface_checker_parity.sh tests/self_hosted/parity/mir_json_parity.sh
check_gates READY  "6 AIR as verifier"             air_json_schema_smoke.sh air_drift_smoke.sh air_backend_nonimpact_smoke.sh
check_gates READY  "7 DAG type resolution SoT"     type_resolution_dag_smoke.sh type_resolution_resolver_inventory_smoke.sh
check_gates READY  "8 scoped unsafe/raw escape"    raw_escape_contract_smoke.sh
check_gates READY  "9 debug info Phase 1"          debug_hygiene_smoke.sh
check_gates READY  "10 runtime profile selection"  runtime_none_contract_smoke.sh

forbid_current_doc_text() {
    local rel="$1"
    local term="$2"
    if grep -Fq -- "$term" "$rel"; then
        echo "[scorecard] stale current-status wording in ${rel}: ${term}" >&2
        missing=1
    fi
}

forbid_current_doc_text "docs/self_hosted/05_compiler_core_gap_analysis.md" \
    "capability 5 remains ACTIVE"
forbid_current_doc_text "docs/self_hosted/05_compiler_core_gap_analysis.md" \
    "capability 5 ACTIVE"
forbid_current_doc_text "docs/self_hosted/07_hard_self_host_scorecard.md" \
    "Capability 5 (CFG/MIR SoT, task 74). ACTIVE"
forbid_current_doc_text "docs/self_hosted/07_hard_self_host_scorecard.md" \
    "remaining ACTIVE tail"
forbid_current_doc_text "docs/self_hosted/07_hard_self_host_scorecard.md" \
    "| 4 | Arena/ownership ergonomics | SUBSET |"
forbid_current_doc_text "docs/00_progress.md" \
    "| self-host substrate | 9/10 READY |"
forbid_current_doc_text "docs/self_hosted/08_domain_mobility_rationale.md" \
    "finish the source_ast retirement"

echo
echo "[scorecard] critical path: CFG/MIR body SoT is READY for the measured frontier; keep the self-hosted MIR-lowering fact-only ratchet green while broadening the supported subset"
echo "[scorecard] CFG/MIR SoT is closed for source_ast/source_decl, residual STMT payload emit, raw source-statement re-dispatch, public-surface provenance, lifecycle dump source-text emission, and C preserved-statement helper surface"
echo "[scorecard] arena/ownership Phase 1 is READY; remaining compiler-scale String lifetime work is an efficiency frontier, not a missing substrate capability"
if [ "$missing" -ne 0 ]; then
    echo "[scorecard] FAIL: one or more capability gates are missing" >&2
    exit 1
fi
echo "[scorecard] all named capability gates present"
