#!/usr/bin/env bash
# The native MIR oracle predates target-neutral match materialization.  While
# that oracle lacks the compiler-owned local, exact MIR bytes are not a sound
# substitution condition: the Pergyra producer must be allowed to strengthen
# one scrutinee expression into one def plus synthetic case uses.  This owner
# admits only that named delta.  All other fixtures retain byte parity, and
# the waiver disappears automatically once the oracle carries the same def.

pgy_selfhost_driver_rung2_match_materialization_delta() {
    local backend="$1" base="$2" oracle_canonical="$3" self_mir_json="$4"
    local names name def_count

    grep -Fq '"arg0":"__pgy_match_' "$self_mir_json" || return 1
    grep -Fq '"arg0":"__pgy_match_' "$oracle_canonical" && return 1

    names="$(grep -oE '"arg0":"__pgy_match_[0-9]+"' "$self_mir_json" \
        | sed -E 's/^"arg0":"([^"]+)"$/\1/' | sort -u)"
    [[ -n "$names" ]] || {
        echo "[self-host-parity:driver-rung2] $backend/$base match materialization name inventory is empty" >&2
        exit 1
    }
    while IFS= read -r name; do
        [[ -n "$name" ]] || continue
        def_count="$(grep -oF "\"result\":\"$name.1\"" \
            "$self_mir_json" | wc -l | tr -d ' ')"
        if [[ "$def_count" -ne 1 ]] ||
            ! grep -Fq "\"expr0\":\"$name\",\"expr0_graph\":" \
                "$self_mir_json" ||
            ! grep -Fq "\"uses\":[\"$name.1\"]" "$self_mir_json"; then
            echo "[self-host-parity:driver-rung2] $backend/$base match materialization is not one def plus synthetic uses: $name" >&2
            exit 1
        fi
    done <<<"$names"
    echo "[self-host-parity:driver-rung2] $backend/$base admits the bounded legacy-oracle match materialization delta"
    return 0
}

pgy_selfhost_driver_rung2_consume_canonical_match_mir() {
    local machine_fixture="$1" backend="$2" base="$3" driver_bin="$4"
    local mir_json="$5" self_mir_json="$6" oracle_canonical="$7"
    local self_canonical="$8" actual_raw="$9" err="${10}"
    local canonical_consume="$oracle_canonical"

    if pgy_selfhost_driver_rung2_match_materialization_delta \
        "$backend" "$base" "$oracle_canonical" "$self_mir_json"; then
        canonical_consume="$self_canonical"
    else
        pgy_selfhost_verify_driver_rung2_canonical_declaration_order \
            "$backend" "$base" "$mir_json" "$self_mir_json" \
            "$oracle_canonical" "$self_canonical"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "driver-rung2:$backend:$base:mir-json" "$BUILD_DIR" \
            "$oracle_canonical" "$self_canonical" "mir_json"
    fi
    pgy_selfhost_driver_rung2_consume_mir "$machine_fixture" "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$canonical_consume")" \
        "$actual_raw" "$err"
}
