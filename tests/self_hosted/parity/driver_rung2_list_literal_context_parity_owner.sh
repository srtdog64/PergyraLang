#!/usr/bin/env bash
# Owns declared List<T> sequence-literal contextualization and its ABI proof.

pgy_selfhost_verify_driver_rung2_list_literal_context() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_type bad_source out err source
    [[ "$base" == "list_literal_context" ]] || return 0

    for fact in \
        '"result":"nums.1","arg0":"nums","arg1":null,"slot_anchor":null,"abi_type_name":"List<Int>"' \
        '"result":"names.1","arg0":"names","arg1":null,"slot_anchor":null,"abi_type_name":"List<String>"' \
        '"result":"empty.1","arg0":"empty","arg1":null,"slot_anchor":null,"abi_type_name":"List<String>"' \
        '"expr0":"[3, 4, 5]"' \
        '"expr0":"[\"alpha\", \"beta\"]"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend List literal fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_type="$BUILD_DIR/${base}_${backend}.missing-list-abi-type.mir.json"
    out="$missing_type.out"
    err="$missing_type.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_type" \
        '"abi_type_name":"List<Int>"' '"abi_type_name":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing List ABI type was accepted" >&2
        exit 1
    fi
    grep -Fq 'local declaration is missing its MIR ABI type fact' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing List ABI diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    source="$ROOT_DIR/tests/cases/backend_compare/list_literal_context/main.pgy"
    bad_source="$BUILD_DIR/${base}_${backend}.bad-element.pgy"
    pgy_replace_first_literal "$source" "$bad_source" \
        'let nums: List<Int> = [3, 4, 5];' \
        'let nums: List<Int> = ["bad"];'
    out="$bad_source.out"
    err="$bad_source.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend mismatched List literal was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: let_type_mismatch' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend List literal mismatch diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_list_literal_context_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "list_literal_context" ]] || return 0
    for term in \
        'PgyList_Int nums = ({' \
        'pgy_list_push_int(&_pgy_list_value, 3)' \
        'PgyList_String names = ({' \
        'pgy_list_push_string(&_pgy_list_value, "beta")' \
        'PgyList_String empty = ({' \
        'pgy_list_size_string(&empty)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend List literal C fact drifted: $term" >&2
            exit 1
        }
    done
}
