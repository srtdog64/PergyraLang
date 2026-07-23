#!/usr/bin/env bash
# Owns DRV-2 class/enum composition as one Pergyra semantic seam.

pgy_selfhost_class_enum_reject_missing_target() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local target="$5" missing_target
    missing_target="$BUILD_DIR/${base}_${backend}.${target}.missing-target.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_target" \
        "\"call_target_kind\":\"$6\",\"call_target_name\":\"$target\"" \
        '"call_target_kind":"none","call_target_name":""'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
        >"$missing_target.out" 2>"$missing_target.err"); then
        echo "[self-host-parity:driver-rung2] $backend class/enum target accepted: $target" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$missing_target.err" "$missing_target.out" || {
        echo "[self-host-parity:driver-rung2] $backend class/enum target diagnostic drifted: $target" >&2
        cat "$missing_target.out" "$missing_target.err" >&2
        exit 1
    }
}

pgy_selfhost_class_enum_reject_missing_variant() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local variant="$5" missing_variant
    missing_variant="$BUILD_DIR/${base}_${backend}.${variant}.missing-variant.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_variant" \
        "\"name\":\"$variant\",\"param_count\":0" \
        "\"name\":\"Missing$variant\",\"param_count\":0"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_variant")" \
        >"$missing_variant.out" 2>"$missing_variant.err"); then
        echo "[self-host-parity:driver-rung2] $backend class/enum missing variant accepted: $variant" >&2
        exit 1
    fi
    grep -Fq "match enum variant declaration fact is missing" \
        "$missing_variant.err" "$missing_variant.out" || {
        echo "[self-host-parity:driver-rung2] $backend class/enum variant diagnostic drifted: $variant" >&2
        cat "$missing_variant.out" "$missing_variant.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_class_enum_composition() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local fact pattern target first_variant target_kind
    local -a patterns=() targets=()

    if [[ "$base" == "enum_to_class_match" ]]; then
        patterns=(Tank DPS Healer)
        targets=(Stat StatOf)
        first_variant=Tank
        target_kind=direct
        if [[ "$(grep -Fo '"kind":"call","text":"Stat()","call_target_kind":"direct","call_target_name":"Stat"' \
            "$self_mir_json" | wc -l | tr -d ' ')" -ne 3 ]]; then
            echo "[self-host-parity:driver-rung2] $backend enum-to-class constructor spine drifted" >&2
            exit 1
        fi
        for fact in \
            '"result":"s.1","arg0":"s","arg1":"Stat","abi_type_name":null' \
            '"name":"s","type":"Stat"' \
            '"kind":"member_access","text":"s.val"' \
            '"kind":"member_access","text":"s.scale"'; do
            grep -Fq "$fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend enum-to-class fact drifted: $fact" >&2
                exit 1
            }
        done
    elif [[ "$base" == "class_method_enum_classify" ]]; then
        patterns=(Zero Small Big Negative)
        targets=(Counter_IsZero Counter_IsBig Counter_IsPos Classify)
        first_variant=Zero
        for fact in \
            '"name":"Classify","kind":"function"' \
            '"return":"Verdict"' \
            '"name":"c","type":"Counter","carriage":"value"' \
            '"kind":"member_access","text":"c.value"'; do
            grep -Fq "$fact" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend class-to-enum fact drifted: $fact" >&2
                exit 1
            }
        done
    else
        return 0
    fi

    for pattern in "${patterns[@]}"; do
        grep -Fq "\"match_patterns\":[\"$pattern\"],\"match_variant\":null" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend class/enum pattern drifted: $pattern" >&2
            exit 1
        }
    done
    for target in "${targets[@]}"; do
        target_kind=direct
        [[ "$target" == Counter_* ]] && target_kind=member
        pgy_selfhost_class_enum_reject_missing_target \
            "$backend" "$base" "$self_mir_json" "$driver_bin" "$target" "$target_kind"
    done
    pgy_selfhost_class_enum_reject_missing_variant \
        "$backend" "$base" "$self_mir_json" "$driver_bin" "$first_variant"
}
