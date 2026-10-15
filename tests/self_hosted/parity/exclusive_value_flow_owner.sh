#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
cd "$ROOT"
source tests/pgy_binary_path_helpers.sh
pgy_prepend_windows_runtime_paths
if [[ $# < 1 || $# > 2 ]]; then
    echo "usage: exclusive_value_flow_owner.sh <C source probe> [LLVM source probe]" >&2
    exit 2
fi
receipt="$(mktemp -d "${TMPDIR:-/tmp}/exclusive-value-flow.XXXXXX")"
fixtures=(
    exclusive_map_exit_branch_positive.pgy
    exclusive_map_loop_branch_restore_positive.pgy
    exclusive_map_exit_branch_missing_restore_negative.pgy
    exclusive_map_exit_branch_deferred_read_negative.pgy
    exclusive_map_loop_old_read_negative.pgy
    exclusive_map_loop_old_read_after_negative.pgy
    exclusive_map_loop_missing_restore_negative.pgy
    exclusive_map_loop_break_restore_negative.pgy
    exclusive_map_loop_continue_restore_negative.pgy
    exclusive_map_observed_alias_mutation_negative.pgy
    exclusive_map_observed_source_growth_positive.pgy
    exclusive_map_observed_source_growth_negative.pgy
    exclusive_map_observed_forward_growth_positive.pgy
    exclusive_map_observed_forward_growth_negative.pgy
    exclusive_map_observed_source_backedge_negative.pgy
    exclusive_map_assigned_formal_direct_has_negative.pgy
    exclusive_map_assigned_formal_direct_get_negative.pgy
    exclusive_map_assigned_formal_forward_has_negative.pgy
    exclusive_map_assigned_formal_forward_get_negative.pgy
    exclusive_map_assigned_formal_direct_has_positive.pgy
    exclusive_map_assigned_formal_direct_get_positive.pgy
    exclusive_map_assigned_formal_forward_has_positive.pgy
    exclusive_map_assigned_formal_forward_get_positive.pgy
)
base=tests/self_hosted/parity/fixture/collection_field_lifetime
sha256sum "$@" > "$receipt/probes.sha256"
for fixture in "${fixtures[@]}"; do
    sha256sum "$base/$fixture"
done > "$receipt/inputs.sha256"
backend=C
for probe in "$@"; do
    pgy_reject_wsl_windows_pgy_parity_mix exclusive-value-flow "$probe"
    [[ -x "$probe" ]] || { echo "source probe unavailable: $probe" >&2; exit 2; }
    for fixture in "${fixtures[@]}"; do
        raw="$receipt/$backend-$fixture.raw"
        timeout 60s "$probe" "$base/$fixture" diagnostic > "$raw" 2> "$raw.err"
        tr -d '\r' < "$raw" > "$raw.normalized"
        expected=false
        [[ "$fixture" == *_positive.pgy ]] && expected=true
        if [[ "$fixture" == exclusive_map_assigned_formal_* ]]; then
            grep -qx 'formal_ready=true' "$raw.normalized"
        fi
        if ! grep -qx "body_ok=$expected" "$raw.normalized"; then
            echo "unexpected $backend source verdict: $fixture (receipt $receipt)" >&2
            exit 1
        fi
        if [[ "$expected" == false ]]; then
            grep -qx 'body_diagnostic=borrow_boundary_escape' "$raw.normalized"
            boundary=unproved_exclusive_storage_mutation
            if [[ "$fixture" == exclusive_map_observed_forward_growth_negative.pgy ||
                "$fixture" == exclusive_map_assigned_formal_forward_*_negative.pgy ]]; then
                boundary=unproved_exclusive_sink_entry
            fi
            grep -qx -- "- boundary: $boundary" "$raw.normalized"
            if [[ "$fixture" == exclusive_map_assigned_formal_direct_*_negative.pgy ]]; then
                grep -qx 'body_atom=MapSet(map, "new", 2)' "$raw.normalized"
            elif [[ "$fixture" == exclusive_map_assigned_formal_forward_*_negative.pgy ]]; then
                grep -qx 'body_atom=Grow(map)' "$raw.normalized"
            fi
        else
            grep -qx 'body_diagnostic=' "$raw.normalized"
        fi
    done
    backend=LLVM
done
sha256sum -c "$receipt/probes.sha256" > "$receipt/probes.recheck"
sha256sum -c "$receipt/inputs.sha256" > "$receipt/inputs.recheck"
lanes=C
[[ $# == 2 ]] && lanes=C/LLVM
echo "[exclusive-value-flow] PASS: $lanes source observations, receipt $receipt"
