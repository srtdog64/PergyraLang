#!/usr/bin/env bash
# Owns List<T> contextual construction and fail-closed ABI negatives.
# missing contextual List type fails closed
# nested List ABI owner and negative mutations
# Forbidden regressions: missing_contextual_list_type_success,
# unsupported_nested_list_element_success, and list_constructor_symbol_drift.

pgy_selfhost_verify_driver_rung2_nested_generic_containers() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local source missing_context unsupported_element out err
    [[ "$base" == "nested_generic_containers" ]] || return 0

    for fact in \
        '"expr1":"List<HashMap<String, Int>>"' \
        '"source_locals":[{"name":"xs","type":"List<HashMap<String, Int>>"}]'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend nested List MIR fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    missing_context="$BUILD_DIR/${base}_${backend}.missing-context.pgy"
    out="$missing_context.out"
    err="$missing_context.err"
    pgy_replace_first_literal "$source" "$missing_context" \
        'let xs: List<HashMap<String, Int>> =' \
        'let xs ='
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$missing_context")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing List context was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: initializer_type_unresolved' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing List context diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    unsupported_element="$BUILD_DIR/${base}_${backend}.unsupported-element.pgy"
    out="$unsupported_element.out"
    err="$unsupported_element.err"
    pgy_replace_first_literal "$source" "$unsupported_element" \
        'List<HashMap<String, Int>>' 'List<HashMap<String, Float>>'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$unsupported_element")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend unsupported nested List element was accepted" >&2
        exit 1
    fi
    grep -Fq 'CODEGEN ERROR: List<T> runtime ABI fact is missing' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend unsupported nested List ABI diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_nested_generic_containers_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "nested_generic_containers" ]] || return 0

    for fact in \
        '/* PGY_COLLECTION_LIST_HashMap_String_Int */' \
        'PGY_LIST_DEFINE(HashMap_String_Int, PgyHashMap_Int)' \
        'PgyList_HashMap_String_Int xs = pgy_list_new_HashMap_String_Int();'; do
        grep -Fq "$fact" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend nested List C ABI fact drifted: $fact" >&2
            exit 1
        }
    done
}
