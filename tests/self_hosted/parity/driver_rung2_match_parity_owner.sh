#!/usr/bin/env bash
# Owns DRV-2 scalar-match MIR facts and missing-pattern rejection.

pgy_selfhost_verify_driver_rung2_match() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local match_pattern missing_match_pattern missing_phi_input unknown_phi_input
    local missing_match_binding missing_enum_variant

    if [[ "$base" != "match_case_int" &&
        "$base" != "match_case_assign" &&
        "$base" != "option_match" &&
        "$base" != "enum_match" &&
        "$base" != "class_holds_enum_field" ]]; then
        return 0
    fi
    if [[ "$base" == "enum_match" ||
        "$base" == "class_holds_enum_field" ]]; then
        local enum_patterns=(North East South)
        local first_variant="North"
        if [[ "$base" == "class_holds_enum_field" ]]; then
            enum_patterns=(Bronze Silver Gold)
            first_variant="Bronze"
        fi
        for match_pattern in "${enum_patterns[@]}"; do
            match_pattern="\"match_patterns\":[\"$match_pattern\"],\"match_variant\":null,\"match_bindings\":[]"
            grep -Fq "$match_pattern" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend enum match fact was lost: $match_pattern" >&2
                exit 1
            }
        done
        missing_enum_variant="$BUILD_DIR/${base}_${backend}.missing-enum-variant.mir.json"
        pgy_replace_first_literal "$self_mir_json" "$missing_enum_variant" \
            "\"name\":\"$first_variant\",\"param_count\":0" \
            "\"name\":\"Missing$first_variant\",\"param_count\":0"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_enum_variant")" \
            >"$missing_enum_variant.out" 2>"$missing_enum_variant.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing enum variant declaration was accepted" >&2
            exit 1
        fi
        grep -Fq "match enum variant declaration fact is missing" \
            "$missing_enum_variant.err" "$missing_enum_variant.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing enum declaration diagnostic drifted" >&2
            cat "$missing_enum_variant.out" "$missing_enum_variant.err" >&2
            exit 1
        }
        return 0
    fi
    if [[ "$base" == "option_match" ]]; then
        for match_pattern in \
            '"match_patterns":["Some(v)"],"match_variant":"Some","match_bindings":["v"]' \
            '"match_patterns":["None"],"match_variant":"None","match_bindings":[]'; do
            grep -Fq "$match_pattern" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Option match fact was lost: $match_pattern" >&2
                exit 1
            }
        done
        missing_match_binding="$BUILD_DIR/${base}_${backend}.missing-match-binding.mir.json"
        sed 's/"match_bindings":\["v"\]/"match_bindings":[]/' \
            "$self_mir_json" >"$missing_match_binding"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_match_binding")" \
            >"$missing_match_binding.out" 2>"$missing_match_binding.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing Option match binding was accepted" >&2
            exit 1
        fi
        grep -Eq "(Some match case is missing subject or binding MIR fact|MIR expression graph facts are missing or inconsistent)" \
            "$missing_match_binding.err" "$missing_match_binding.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing Option binding diagnostic drifted" >&2
            cat "$missing_match_binding.out" "$missing_match_binding.err" >&2
            exit 1
        }
        return 0
    fi
    for match_pattern in \
        '"match_patterns":["1"]' \
        '"match_patterns":["2"]' \
        '"match_patterns":["3"]'; do
        grep -Fq "$match_pattern" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend match pattern fact was lost: $match_pattern" >&2
            exit 1
        }
    done
    if [[ "$base" == "match_case_assign" ]]; then
        grep -Fq '"kind":"phi","name":"value"' "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend match phi fact was lost" >&2
            exit 1
        }
        grep -Fq '"uses":["value.3","value.5","value.7","value.8"]' \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend match phi inputs drifted" >&2
            exit 1
        }
        missing_phi_input="$BUILD_DIR/${base}_${backend}.missing-phi-input.mir.json"
        sed 's/"uses":\["value.3","value.5","value.7","value.8"\]/"uses":["value.3","value.5","value.7"]/' \
            "$self_mir_json" >"$missing_phi_input"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_phi_input")" \
            >"$missing_phi_input.out" 2>"$missing_phi_input.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing match phi input was accepted" >&2
            exit 1
        fi
        grep -Fq "MIR phi facts are missing or inconsistent" \
            "$missing_phi_input.err" "$missing_phi_input.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing match phi diagnostic drifted" >&2
            cat "$missing_phi_input.out" "$missing_phi_input.err" >&2
            exit 1
        }
        unknown_phi_input="$BUILD_DIR/${base}_${backend}.unknown-phi-input.mir.json"
        sed 's/"uses":\["value.3","value.5","value.7","value.8"\]/"uses":["value.3","value.5","value.7","value.999"]/' \
            "$self_mir_json" >"$unknown_phi_input"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$unknown_phi_input")" \
            >"$unknown_phi_input.out" 2>"$unknown_phi_input.err"); then
            echo "[self-host-parity:driver-rung2] $backend unknown match phi input was accepted" >&2
            exit 1
        fi
        grep -Fq "MIR phi facts are missing or inconsistent" \
            "$unknown_phi_input.err" "$unknown_phi_input.out" || {
            echo "[self-host-parity:driver-rung2] $backend unknown match phi diagnostic drifted" >&2
            cat "$unknown_phi_input.out" "$unknown_phi_input.err" >&2
            exit 1
        }
    fi
    missing_match_pattern="$BUILD_DIR/${base}_${backend}.missing-match-pattern.mir.json"
    sed 's/"match_patterns":\["1"\]/"match_patterns":[]/' \
        "$self_mir_json" >"$missing_match_pattern"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_match_pattern")" \
        >"$missing_match_pattern.out" 2>"$missing_match_pattern.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing match pattern was accepted" >&2
        exit 1
    fi
    grep -Fq "match-case branch is missing MIR subject/pattern facts" \
        "$missing_match_pattern.err" "$missing_match_pattern.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing match pattern diagnostic drifted" >&2
        cat "$missing_match_pattern.out" "$missing_match_pattern.err" >&2
        exit 1
    }
}
