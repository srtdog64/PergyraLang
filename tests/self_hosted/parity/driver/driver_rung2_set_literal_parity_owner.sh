#!/usr/bin/env bash
# Owns the Set-literal graph/type/runtime boundary.
# Forbidden: array/struct fallback, source reparse, inferred Set element ABI,
# and accepting an untyped empty literal or a missing Set ABI row.
# Set literal graph/runtime ABI parity and fail-closed empty/type/missing-row negatives.
# Forbidden names: set_literal_as_array_or_struct_fallback,
# set_element_type_guess, source_set_spelling_as_abi,
# untyped_empty_set_success, missing_set_runtime_fact_success.

pgy_selfhost_verify_driver_rung2_set_literal() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_type bad_source untyped_source out err
    [[ "$base" == "set_literal_basic" ]] || return 0

    for fact in \
        '"kind":"set_literal"' \
        '"kind":"set_element"' \
        '"result":"seen.1","arg0":"seen","arg1":null,"slot_anchor":null,"abi_type_name":"Set<Int>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Set literal fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_type="$BUILD_DIR/${base}_${backend}.missing-set-literal-abi-type.mir.json"
    out="$missing_type.out"
    err="$missing_type.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_type" \
        '"abi_type_name":"Set<Int>"' '"abi_type_name":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing Set literal ABI type was accepted" >&2
        exit 1
    fi
    grep -Fq 'local declaration is missing its MIR ABI type fact' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend Set literal missing ABI diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    bad_source="$BUILD_DIR/${base}_${backend}.bad-set-literal-element.pgy"
    pgy_replace_first_literal \
        "$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy" "$bad_source" \
        '{1, 2, 2, 3}' '{1, "bad", 2, 3}'
    out="$bad_source.out"
    err="$bad_source.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend mismatched Set literal element was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: initializer_type_unresolved' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend Set literal element diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    untyped_source="$ROOT_DIR/tests/cases/backend_compare/$base/bad_untyped_empty.pgy"
    out="$BUILD_DIR/${base}_${backend}.untyped-empty-set-literal.out"
    err="$BUILD_DIR/${base}_${backend}.untyped-empty-set-literal.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$untyped_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend untyped empty Set literal was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: initializer_type_unresolved' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend empty Set literal diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_set_literal_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "set_literal_basic" ]] || return 0
    for term in \
        'PgySet_int seen = ({ PgySet_int _pgy_set_value = pgy_set_new_int()' \
        'pgy_set_add_int(&_pgy_set_value, 1)' \
        'pgy_set_add_int(&_pgy_set_value, 2)' \
        'pgy_set_add_int(&_pgy_set_value, 3)' \
        'PgySet_String tags = ({ PgySet_String _pgy_set_value = pgy_set_new_string()' \
        'pgy_set_add_string(&_pgy_set_value, "alpha")' \
        'pgy_set_add_string(&_pgy_set_value, "beta")'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Set literal C missing: $term" >&2
            exit 1
        }
    done
}
