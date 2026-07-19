#!/usr/bin/env bash
# Owns DRV-2 generic member specialization carriage and C emission checks.

pgy_selfhost_verify_driver_rung2_generic_member_specialization() {
    local backend="$1"
    local base="$2"
    local self_mir_json="$3"
    local native_mir_json="$4"
    local driver_bin="$5"
    local missing_rows
    local bad_identity
    local bad_symbol
    local unresolved_actual
    local owner
    local receiver
    local specialized
    local inner_specialized=""
    local constructed=0
    local array_constructed=0
    local record_array_constructed=0
    local inner_actual="Int"

    if [[ "$base" == "generic_member_inferred_flow" ]]; then
        owner="Box"
        receiver="box"
    elif [[ "$base" == "generic_vessel_member_inferred_flow" ]]; then
        owner="Cell"
        receiver="cell"
    elif [[ "$base" == "generic_member_constructed_return_flow" ]]; then
        owner="Wrapper"
        receiver="wrapper"
        constructed=1
    elif [[ "$base" == "generic_member_array_return_flow" ]]; then
        owner="ArrayWrapper"
        receiver="wrapper"
        array_constructed=1
    elif [[ "$base" == "generic_member_record_array_return_flow" ]]; then
        owner="RecordArrayWrapper"
        receiver="wrapper"
        record_array_constructed=1
        inner_actual="Point"
    else
        return 0
    fi
    if [[ "$record_array_constructed" -eq 1 ]]; then
        specialized="RecordArrayWrapper_Echo_Array_Point_"
        inner_specialized="RecordArrayWrapper_Wrap_Point"
    elif [[ "$array_constructed" -eq 1 ]]; then
        specialized="ArrayWrapper_Echo_Array_Int_"
        inner_specialized="ArrayWrapper_Wrap_Int"
    elif [[ "$constructed" -eq 1 ]]; then
        specialized="Wrapper_Echo_Option_Int_"
        inner_specialized="Wrapper_Wrap_Int"
    else
        specialized="${owner}_Echo_Int"
    fi

    if grep -Fq '"kind":"generic_type_actual"' "$self_mir_json"; then
        echo "[self-host-parity:driver-rung2] $backend inferred member call gained explicit actuals" >&2
        exit 1
    fi
    grep -Fq "\"kind\":\"call\",\"text\":\"$receiver.Echo()\",\"call_target_kind\":\"member\",\"call_target_name\":\"${owner}_Echo\"" \
        "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend generic member call spine drifted" >&2
        exit 1
    }
    if [[ "$constructed" -eq 1 || "$array_constructed" -eq 1 ||
        "$record_array_constructed" -eq 1 ]]; then
        grep -Fq "\"kind\":\"call\",\"text\":\"$receiver.Wrap()\",\"call_target_kind\":\"member\",\"call_target_name\":\"${owner}_Wrap\"" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend constructed inner member call spine drifted" >&2
            exit 1
        }
        local outer_actual="Option<Int>"
        if [[ "$array_constructed" -eq 1 ]]; then
            outer_actual="Array<Int>"
        elif [[ "$record_array_constructed" -eq 1 ]]; then
            outer_actual="Array<Point>"
        fi
        grep -Fq "\"owner\":\"$owner\",\"callable\":\"Echo\",\"specialized_symbol\":\"$specialized\",\"generic_params\":[\"T\"],\"generic_actuals\":[\"$outer_actual\"]" \
            "$self_mir_json" &&
            grep -Fq "\"owner\":\"$owner\",\"callable\":\"Wrap\",\"specialized_symbol\":\"$inner_specialized\",\"generic_params\":[\"T\"],\"generic_actuals\":[\"$inner_actual\"]" \
                "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend constructed MIR specialization rows drifted" >&2
            exit 1
        }
        [[ "$(grep -oF "\"specialized_symbol\":\"$specialized\"" "$self_mir_json" | wc -l | tr -d ' ')" == "1" &&
            "$(grep -oF "\"specialized_symbol\":\"$inner_specialized\"" "$self_mir_json" | wc -l | tr -d ' ')" == "1" ]] || {
            echo "[self-host-parity:driver-rung2] $backend constructed MIR row count drifted" >&2
            exit 1
        }
    else
        grep -Fq "\"owner\":\"$owner\",\"callable\":\"Echo\",\"specialized_symbol\":\"$specialized\",\"generic_params\":[\"T\"],\"generic_actuals\":[\"Int\"]" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend MIR generic method specialization row drifted" >&2
            exit 1
        }
        [[ "$(grep -oF "\"specialized_symbol\":\"$specialized\"" "$self_mir_json" | wc -l | tr -d ' ')" == "2" ]] || {
            echo "[self-host-parity:driver-rung2] $backend MIR generic method row count drifted" >&2
            exit 1
        }
    fi
    grep -Fq '"source_call_ordinal":0' "$self_mir_json" &&
        grep -Fq '"source_call_ordinal":1' "$self_mir_json" || {
        echo "[self-host-parity:driver-rung2] $backend MIR generic method ordinal drifted" >&2
        exit 1
    }
    if [[ "$constructed" -eq 1 || "$array_constructed" -eq 1 ||
        "$record_array_constructed" -eq 1 ]]; then
        local native_outer_actual="Option<Int>"
        if [[ "$array_constructed" -eq 1 ]]; then
            native_outer_actual="Array<Int>"
        elif [[ "$record_array_constructed" -eq 1 ]]; then
            native_outer_actual="Array<Point>"
        fi
        grep -Fq "\"owner\":\"$owner\",\"method\":\"Echo\",\"symbol\":\"$specialized\",\"generic_params\":[\"T\"],\"actual_types\":[\"$native_outer_actual\"]" \
            "$native_mir_json" &&
            grep -Fq "\"owner\":\"$owner\",\"method\":\"Wrap\",\"symbol\":\"$inner_specialized\",\"generic_params\":[\"T\"],\"actual_types\":[\"$inner_actual\"]" \
                "$native_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend native constructed specialization rows drifted" >&2
            exit 1
        }
    else
        grep -Fq "\"owner\":\"$owner\",\"method\":\"Echo\",\"symbol\":\"$specialized\",\"generic_params\":[\"T\"],\"actual_types\":[\"Int\"]" \
            "$native_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend native/self generic method semantics drifted" >&2
            exit 1
        }
        [[ "$(grep -oF "\"symbol\":\"$specialized\"" "$native_mir_json" | wc -l | tr -d ' ')" == "2" ]] || {
            echo "[self-host-parity:driver-rung2] $backend native generic method row count drifted" >&2
            exit 1
        }
    fi

    missing_rows="${self_mir_json%.json}.missing-generic-method-rows.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$missing_rows" \
        '"generic_method_specializations":' \
        '"generic_method_specializations_missing":'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_rows")" \
        >"$missing_rows.out" 2>"$missing_rows.err"); then
        echo "[self-host-parity:driver-rung2] $backend missing MIR generic method row was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR generic specialization facts are incomplete" \
        "$missing_rows.err" "$missing_rows.out" || {
        echo "[self-host-parity:driver-rung2] $backend missing MIR generic method row diagnostic drifted" >&2
        cat "$missing_rows.out" "$missing_rows.err" >&2
        exit 1
    }

    bad_identity="${self_mir_json%.json}.bad-generic-method-identity.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$bad_identity" \
        '"source_call_ordinal":0' '"source_call_ordinal":999'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$bad_identity")" \
        >"$bad_identity.out" 2>"$bad_identity.err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid MIR generic method identity was accepted" >&2
        exit 1
    fi
    grep -Fq "generic specialization identity is unknown" \
        "$bad_identity.err" "$bad_identity.out" || {
        echo "[self-host-parity:driver-rung2] $backend generic method identity diagnostic drifted" >&2
        cat "$bad_identity.out" "$bad_identity.err" >&2
        exit 1
    }

    bad_symbol="${self_mir_json%.json}.bad-generic-method-symbol.mir.json"
    pgy_replace_first_literal "$self_mir_json" "$bad_symbol" \
        "\"specialized_symbol\":\"$specialized\"" \
        "\"specialized_symbol\":\"${owner}_Echo_Drift\""
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$bad_symbol")" \
        >"$bad_symbol.out" 2>"$bad_symbol.err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid MIR generic method symbol was accepted" >&2
        exit 1
    fi
    grep -Fq "generic specialization symbol drifted" \
        "$bad_symbol.err" "$bad_symbol.out" || {
        echo "[self-host-parity:driver-rung2] $backend generic method symbol diagnostic drifted" >&2
        cat "$bad_symbol.out" "$bad_symbol.err" >&2
        exit 1
    }

    if [[ "$constructed" -eq 1 || "$array_constructed" -eq 1 ||
        "$record_array_constructed" -eq 1 ]]; then
        unresolved_actual="${self_mir_json%.json}.unresolved-generic-method-actual.mir.json"
        local concrete_outer="Option<Int>"
        local unresolved_outer="Option<T>"
        if [[ "$array_constructed" -eq 1 ]]; then
            concrete_outer="Array<Int>"
            unresolved_outer="Array<T>"
        elif [[ "$record_array_constructed" -eq 1 ]]; then
            concrete_outer="Array<Point>"
            unresolved_outer="Array<T>"
        fi
        pgy_replace_first_literal "$self_mir_json" "$unresolved_actual" \
            "\"generic_actuals\":[\"$concrete_outer\"]" \
            "\"generic_actuals\":[\"$unresolved_outer\"]"
        if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
            "$(pgy_selfhost_path_relative_to_root "$unresolved_actual")" \
            >"$unresolved_actual.out" 2>"$unresolved_actual.err"); then
            echo "[self-host-parity:driver-rung2] $backend unresolved constructed generic actual was accepted" >&2
            exit 1
        fi
        grep -Fq "MIR generic specialization actuals drifted" \
            "$unresolved_actual.err" "$unresolved_actual.out" || {
            echo "[self-host-parity:driver-rung2] $backend unresolved constructed actual diagnostic drifted" >&2
            cat "$unresolved_actual.out" "$unresolved_actual.err" >&2
            exit 1
        }
    fi
}

pgy_selfhost_verify_driver_rung2_generic_member_specialization_emitted_c() {
    local backend="$1"
    local base="$2"
    local emitted_c="$3"
    local owner
    local receiver
    local expected
    local specialized
    local inner_specialized=""
    local constructed=0
    local array_constructed=0
    local record_array_constructed=0

    if [[ "$base" == "generic_member_inferred_flow" ]]; then
        owner="Box"
        receiver="box"
        expected="41"
    elif [[ "$base" == "generic_vessel_member_inferred_flow" ]]; then
        owner="Cell"
        receiver="cell"
        expected="42"
    elif [[ "$base" == "generic_member_constructed_return_flow" ]]; then
        owner="Wrapper"
        receiver="wrapper"
        expected="43"
        constructed=1
    elif [[ "$base" == "generic_member_array_return_flow" ]]; then
        owner="ArrayWrapper"
        receiver="wrapper"
        expected="44"
        array_constructed=1
    elif [[ "$base" == "generic_member_record_array_return_flow" ]]; then
        owner="RecordArrayWrapper"
        receiver="wrapper"
        expected="point"
        record_array_constructed=1
    else
        return 0
    fi
    if [[ "$record_array_constructed" -eq 1 ]]; then
        specialized="RecordArrayWrapper_Echo_Array_Point_"
        inner_specialized="RecordArrayWrapper_Wrap_Point"
    elif [[ "$array_constructed" -eq 1 ]]; then
        specialized="ArrayWrapper_Echo_Array_Int_"
        inner_specialized="ArrayWrapper_Wrap_Int"
    elif [[ "$constructed" -eq 1 ]]; then
        specialized="Wrapper_Echo_Option_Int_"
        inner_specialized="Wrapper_Wrap_Int"
    else
        specialized="${owner}_Echo_Int"
    fi

    if [[ "$record_array_constructed" -eq 1 ]]; then
        grep -Eq "pgy_Point_array ${specialized}\\(RecordArrayWrapper [A-Za-z_][A-Za-z0-9_]*, pgy_Point_array [A-Za-z_][A-Za-z0-9_]*\\)" \
            "$emitted_c" &&
            grep -Eq "pgy_Point_array ${inner_specialized}\\(RecordArrayWrapper [A-Za-z_][A-Za-z0-9_]*, Point [A-Za-z_][A-Za-z0-9_]*\\)" \
                "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend record-Array generic member bodies were not specialized" >&2
            exit 1
        }
        grep -Fq "$specialized($receiver, $inner_specialized($receiver, $expected))" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend record-Array generic member call was not specialized" >&2
            exit 1
        }
        grep -Fq 'typedef struct { Point *data; long long len; long long cap; } pgy_Point_array;' \
            "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend concrete record-Array runtime row was lost" >&2
            exit 1
        }
        if grep -Eq "RecordArrayWrapper_(Echo|Wrap)\\(RecordArrayWrapper " "$emitted_c"; then
            echo "[self-host-parity:driver-rung2] $backend raw record-Array generic member leaked into C" >&2
            exit 1
        fi
        return 0
    fi

    if [[ "$array_constructed" -eq 1 ]]; then
        grep -Eq "pgy_ai ${specialized}\\(ArrayWrapper [A-Za-z_][A-Za-z0-9_]*, pgy_ai [A-Za-z_][A-Za-z0-9_]*\\)" \
            "$emitted_c" &&
            grep -Eq "pgy_ai ${inner_specialized}\\(ArrayWrapper [A-Za-z_][A-Za-z0-9_]*, long long [A-Za-z_][A-Za-z0-9_]*\\)" \
                "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Array generic member bodies were not specialized" >&2
            exit 1
        }
        grep -Fq "$specialized($receiver, $inner_specialized($receiver, $expected))" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Array generic member call was not specialized" >&2
            exit 1
        }
        if grep -Eq "ArrayWrapper_(Echo|Wrap)\\(ArrayWrapper " "$emitted_c"; then
            echo "[self-host-parity:driver-rung2] $backend raw Array generic member leaked into C" >&2
            exit 1
        fi
        return 0
    fi

    if [[ "$constructed" -eq 1 ]]; then
        grep -Eq "pgy_option_int ${specialized}\\(Wrapper [A-Za-z_][A-Za-z0-9_]*, pgy_option_int [A-Za-z_][A-Za-z0-9_]*\\)" \
            "$emitted_c" &&
            grep -Eq "pgy_option_int ${inner_specialized}\\(Wrapper [A-Za-z_][A-Za-z0-9_]*, long long [A-Za-z_][A-Za-z0-9_]*\\)" \
                "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend constructed generic member bodies were not specialized" >&2
            exit 1
        }
        grep -Fq "$specialized($receiver, $inner_specialized($receiver, $expected))" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend constructed generic member call was not specialized" >&2
            exit 1
        }
        if grep -Eq "Wrapper_(Echo|Wrap)\\(Wrapper " "$emitted_c"; then
            echo "[self-host-parity:driver-rung2] $backend raw constructed generic member leaked into C" >&2
            exit 1
        fi
        return 0
    fi

    grep -Eq "long long ${specialized}\\(${owner} [A-Za-z_][A-Za-z0-9_]*, long long [A-Za-z_][A-Za-z0-9_]*\\)" \
        "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend generic member body was not specialized" >&2
        exit 1
    }
    grep -Fq "$specialized($receiver, $specialized($receiver, $expected))" "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend generic member call was not specialized" >&2
        exit 1
    }
    if grep -Eq "long long ${owner}_Echo\\(${owner} " "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend raw generic member leaked into C" >&2
        exit 1
    fi
}
