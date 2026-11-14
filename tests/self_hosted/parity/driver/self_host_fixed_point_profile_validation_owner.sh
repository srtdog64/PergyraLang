#!/usr/bin/env bash
# Owns validation of one sealed fixed-point performance receipt.

pgy_selfhost_fixed_point_profile_validate() {
    local receipt="$1" fixed_receipt="$2" driver_source="$3"
    local driver_seed_c="$4" driver_oracle_c="$5" mir_seed="$6"
    local mir_oracle="$7" gen2_c="$8" gen3_c="$9" gen2_binary="${10}"
    local native_pgy="${11}" codegen_seed="${12}"
    local phase expected_count actual_count source_graph fixed_hash

    [[ -f "$receipt" ]] || return 1
    [[ "$(grep -Fc 'schema=pgy.selfhost.fixed-point-profile.v1' "$receipt")" -eq 1 ]] || return 1
    expected_count="$(pgy_selfhost_fixed_point_profile_required_phases | wc -l | tr -d ' ')"
    actual_count="$(grep -c '^phase=' "$receipt" || true)"
    [[ "$actual_count" -eq "$expected_count" ]] || return 1
    while IFS= read -r phase; do
        [[ "$(grep -Fc "phase=$phase|" "$receipt")" -eq 1 ]] || return 1
    done < <(pgy_selfhost_fixed_point_profile_required_phases)
    awk -F'|' '
        /^phase=/ {
            if ($3 !~ /^wall_ms=[0-9]+$/ ||
                $4 !~ /^user_ms=([0-9]+|unavailable)$/ ||
                $5 !~ /^system_ms=([0-9]+|unavailable)$/ ||
                $6 !~ /^max_rss_kb=([0-9]+|unavailable)$/ ||
                $7 != "exit_code=0") exit 1
        }
    ' "$receipt" || return 1
    for phase in full_mir_seed full_mir_oracle; do
        grep -F "stage=$phase|" "$receipt" | grep -Fq 'event=[driver-pressure-stage] ast:start' || return 1
        grep -F "stage=$phase|" "$receipt" | grep -Fq 'event=[driver-pressure-stage] json-write:done' || return 1
    done
    source_graph="$(sed -n 's/^source_graph=//p' "$fixed_receipt")"
    [[ -n "$source_graph" ]] || return 1
    grep -Fxq "source_graph=$source_graph" "$receipt" || return 1
    fixed_hash="$(pgy_selfhost_driver_receipt_hash_file "$fixed_receipt")" || return 1
    grep -Fxq "fixed_point_receipt=$fixed_hash" "$receipt" || return 1
    grep -Fxq "tool=native_pgy|sha256=$(pgy_selfhost_driver_receipt_hash_file "$native_pgy")" "$receipt" || return 1
    grep -Fxq "tool=codegen_seed|sha256=$(pgy_selfhost_driver_receipt_hash_file "$codegen_seed")" "$receipt" || return 1
    local spec name path expected
    for spec in \
        "driver_source|$driver_source" "driver_seed_c|$driver_seed_c" \
        "driver_oracle_c|$driver_oracle_c" "mir_seed|$mir_seed" \
        "mir_oracle|$mir_oracle" "gen2_c|$gen2_c" "gen3_c|$gen3_c" \
        "gen2_binary|$gen2_binary"; do
        name="${spec%%|*}"
        path="${spec#*|}"
        expected="$(pgy_selfhost_fixed_point_profile_artifact_row "$name" "$path")" || return 1
        grep -Fxq "$expected" "$receipt" || return 1
    done
    [[ "$(pgy_selfhost_driver_receipt_hash_file "$mir_seed")" == \
        "$(pgy_selfhost_driver_receipt_hash_file "$mir_oracle")" ]] || return 1
    [[ "$(pgy_selfhost_driver_receipt_hash_file "$gen2_c")" == \
        "$(pgy_selfhost_driver_receipt_hash_file "$gen3_c")" ]] || return 1
    if [[ "${PGY_SELFHOST_PROFILE_REQUIRE_RESOURCE:-0}" == "1" ]]; then
        grep -Fxq 'resource_metrics=gnu-time' "$receipt" || return 1
    fi
    if [[ "${PGY_SELFHOST_FULL_MIR_BULKHEAD:-0}" == "1" ]]; then
        local admitted_hash
        [[ "$(grep -c '^process_boundary=' "$receipt" || true)" == 2 ]] || return 1
        for phase in full_mir_seed full_mir_oracle; do
            admitted_hash="$(python3 "$PGY_SELFHOST_PROFILE_OWNER_DIR/../mir/full_mir_phase_process_owner.py" \
                --validate-receipt "${receipt}.${phase}.process.json")" || return 1
            grep -Fxq "process_boundary=$phase|sha256=$admitted_hash" "$receipt" || return 1
        done
    fi
}
