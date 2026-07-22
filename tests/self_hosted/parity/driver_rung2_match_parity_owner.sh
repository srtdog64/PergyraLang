#!/usr/bin/env bash
# Owns DRV-2 scalar-match MIR facts and missing-pattern rejection.

pgy_selfhost_verify_driver_rung2_match() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local driver_bin="$4"
    local match_pattern missing_match_pattern missing_phi_input unknown_phi_input
    local missing_match_binding missing_match_binding_type missing_enum_variant
    local result_ok_pattern result_err_pattern result_ok_type

    if [[ "$base" != "match_case_int" &&
        "$base" != "match_case_assign" &&
        "$base" != "option_match" &&
        "$base" != "enum_match" &&
        "$base" != "class_holds_enum_field" &&
        "$base" != "dish_result_collect" &&
        "$base" != "class_factory_result_wrap" &&
        "$base" != "class_result_chain_loop" &&
        "$base" != "class_method_result_loop" ]]; then
        return 0
    fi
    if [[ "$base" == "dish_result_collect" ||
        "$base" == "class_factory_result_wrap" ||
        "$base" == "class_result_chain_loop" ||
        "$base" == "class_method_result_loop" ]]; then
        result_ok_pattern='"match_patterns":["Ok(d)"],"match_variant":"Ok","match_bindings":["d"],"match_binding_types":["Dish"]'
        result_err_pattern='"match_patterns":["Err(e)"],"match_variant":"Err","match_bindings":["e"],"match_binding_types":["CookErr"]'
        result_ok_type="Dish"
        if [[ "$base" == "class_factory_result_wrap" ]]; then
            result_ok_pattern='"match_patterns":["Ok(t)"],"match_variant":"Ok","match_bindings":["t"],"match_binding_types":["Tax"]'
            result_err_pattern='"match_patterns":["Err(e)"],"match_variant":"Err","match_bindings":["e"],"match_binding_types":["TaxErr"]'
            result_ok_type="Tax"
        elif [[ "$base" == "class_result_chain_loop" ]]; then
            result_ok_pattern='"match_patterns":["Ok(after)"],"match_variant":"Ok","match_bindings":["after"],"match_binding_types":["Wizard"]'
            result_err_pattern='"match_patterns":["Err(e)"],"match_variant":"Err","match_bindings":["e"],"match_binding_types":["DraftErr"]'
            result_ok_type="Wizard"
        elif [[ "$base" == "class_method_result_loop" ]]; then
            result_ok_pattern='"match_patterns":["Ok(v)"],"match_variant":"Ok","match_bindings":["v"],"match_binding_types":["Int"]'
            result_err_pattern='"match_patterns":["Err(e)"],"match_variant":"Err","match_bindings":["e"],"match_binding_types":["DivErr"]'
            result_ok_type="Int"
        fi
        for match_pattern in "$result_ok_pattern" "$result_err_pattern"; do
            grep -Fq "$match_pattern" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Result match binding type fact was lost: $match_pattern" >&2
                exit 1
            }
        done
        missing_match_binding_type="$BUILD_DIR/${base}_${backend}.missing-match-binding-type.mir.json"
        pgy_replace_first_literal "$self_mir_json" "$missing_match_binding_type" \
            "\"match_binding_types\":[\"$result_ok_type\"]" \
            '"match_binding_types":[]'
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$missing_match_binding_type")" \
            >"$missing_match_binding_type.out" 2>"$missing_match_binding_type.err"); then
            echo "[self-host-parity:driver-rung2] $backend missing Result match binding type was accepted" >&2
            exit 1
        fi
        grep -Fq "MIR instruction expression graph is missing or invalid" \
            "$missing_match_binding_type.err" "$missing_match_binding_type.out" || {
            echo "[self-host-parity:driver-rung2] $backend missing Result binding type diagnostic drifted" >&2
            cat "$missing_match_binding_type.out" "$missing_match_binding_type.err" >&2
            exit 1
        }
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
            '"match_patterns":["Some(v)"],"match_variant":"Some","match_bindings":["v"],"match_binding_types":["Int"]' \
            '"match_patterns":["None"],"match_variant":"None","match_bindings":[],"match_binding_types":[]'; do
            grep -Fq "$match_pattern" "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend Option match fact was lost: $match_pattern" >&2
                exit 1
            }
        done
        missing_match_binding="$BUILD_DIR/${base}_${backend}.missing-match-binding.mir.json"
        sed 's/"match_bindings":\["v"\],"match_binding_types":\["Int"\]/"match_bindings":[],"match_binding_types":[]/' \
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
