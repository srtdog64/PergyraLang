#!/usr/bin/env bash
# Owns List<T> collection-call lowering and fail-closed call-target evidence.
# missing List call target fails closed
# List operation ABI calls

pgy_selfhost_verify_driver_rung2_list_ops() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_target source bad_value bad_index out err
    [[ "$base" == "list_ops" ]] || return 0

    for operation in ListPush ListGet ListSet ListRemove ListSize; do
        grep -Fq "\"call_target_kind\":\"direct\",\"call_target_name\":\"$operation\"" \
            "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend List call target drifted: $operation" >&2
            exit 1
        }
    done

    missing_target="$BUILD_DIR/${base}_${backend}.missing-list-call-target.mir.json"
    out="$missing_target.out"
    err="$missing_target.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_target" \
        '"call_target_kind":"direct","call_target_name":"ListPush"' \
        '"call_target_kind":"none","call_target_name":""'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_target")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing List call target was accepted" >&2
        exit 1
    fi
    grep -Fq "MIR instruction expression graph is missing or invalid" \
        "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend List call-target diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    bad_value="$BUILD_DIR/${base}_${backend}.bad-list-value.pgy"
    pgy_replace_first_literal "$source" "$bad_value" \
        'ListPush(items, 10);' 'ListPush(items, "bad");'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_value")" \
        --emit-c-verified >"$bad_value.out" 2>"$bad_value.err"); then
        echo "[self-host-parity:driver-rung2] $backend wrong List value type was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: call_arg_type_mismatch' \
        "$bad_value.out" "$bad_value.err" || {
        echo "[self-host-parity:driver-rung2] $backend List value diagnostic drifted" >&2
        cat "$bad_value.out" "$bad_value.err" >&2
        exit 1
    }

    bad_index="$BUILD_DIR/${base}_${backend}.bad-list-index.pgy"
    pgy_replace_first_literal "$source" "$bad_index" \
        'ListGet(items, 0)' 'ListGet(items, "bad")'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_index")" \
        --emit-c-verified >"$bad_index.out" 2>"$bad_index.err"); then
        echo "[self-host-parity:driver-rung2] $backend wrong List index type was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: builtin_arg_type_mismatch' \
        "$bad_index.out" "$bad_index.err" || {
        echo "[self-host-parity:driver-rung2] $backend List index diagnostic drifted" >&2
        cat "$bad_index.out" "$bad_index.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_list_ops_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "list_ops" ]] || return 0

    for term in \
        'pgy_list_push_int(&items, 10)' \
        'pgy_list_get_int(&items, 0)' \
        'pgy_list_set_int(&items, 1, 99)' \
        'pgy_list_remove_int(&items, 0)' \
        'pgy_list_size_int(&items)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend List ABI call missing: $term" >&2
            exit 1
        }
    done
    if grep -Eq '(^|[^A-Za-z0-9_])List(Push|Get|Set|Remove|Size)\(' "$emitted_c"; then
        echo "[self-host-parity:driver-rung2] $backend source List call escaped lowering" >&2
        exit 1
    fi
}
