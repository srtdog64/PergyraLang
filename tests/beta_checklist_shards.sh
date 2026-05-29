#!/usr/bin/env bash
# Shared helpers for the split beta readiness checklist.

pgy_beta_checklist_files() {
    printf '%s\n' \
        "$ROOT_DIR/docs/100_beta_readiness_checklist.md" \
        "$ROOT_DIR/docs/100a_beta_active_status.md" \
        "$ROOT_DIR/docs/100b_beta_p0_semantics_systems_air.md" \
        "$ROOT_DIR/docs/100c_beta_dag_mir_abi_runtime.md" \
        "$ROOT_DIR/docs/100d_beta_execution_log.md"
}

pgy_beta_checklist_contains() {
    local term="$1"
    local path

    while IFS= read -r path; do
        if grep -Fq -- "$term" "$path"; then
            return 0
        fi
    done < <(pgy_beta_checklist_files)
    return 1
}

pgy_beta_checklist_cat() {
    local path

    while IFS= read -r path; do
        cat "$path"
        printf '\n'
    done < <(pgy_beta_checklist_files)
}
