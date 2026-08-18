#!/usr/bin/env bash
# Owns graph-scalar ListPush value typing and fail-closed edge/type mutations.

pgy_selfhost_verify_driver_rung2_list_push_scalar_value() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_edge bad_source source out err
    [[ "$base" == "list_push_get_loop" ]] || return 0

    for fact in \
        '"call_target_kind":"direct","call_target_name":"ListPush"' \
        '"kind":"multiply","text":"i * i"' \
        '"name":"i","type":"Int"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend ListPush scalar fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_edge="$BUILD_DIR/${base}_${backend}.missing-multiply-edge.mir.json"
    out="$missing_edge.out"
    err="$missing_edge.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_edge" \
        '"kind":"multiply","text":"i * i","call_target_kind":"none","call_target_name":"","call_target_syntax_id":0,"runtime_call_abi_id":0,"binding_kind":"none","binding_ordinal":null,"left":4,"right":5' \
        '"kind":"multiply","text":"i * i","call_target_kind":"none","call_target_name":"","call_target_syntax_id":0,"runtime_call_abi_id":0,"binding_kind":"none","binding_ordinal":null,"left":4,"right":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_edge")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing ListPush scalar edge was accepted" >&2
        exit 1
    fi
    grep -Eq 'MIR instruction expression graph is missing or invalid|owner: collection_value_type' \
        "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing scalar edge diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    bad_source="$BUILD_DIR/${base}_${backend}.bad-scalar-value.pgy"
    pgy_replace_first_literal "$source" "$bad_source" \
        'ListPush(xs, i * i);' 'ListPush(xs, i * "bad");'
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$bad_source.out" 2>"$bad_source.err"); then
        echo "[self-host-parity:driver-rung2] $backend mixed ListPush scalar type was accepted" >&2
        exit 1
    fi
    grep -Fq 'owner: collection_value_type' \
        "$bad_source.out" "$bad_source.err" || {
        echo "[self-host-parity:driver-rung2] $backend mixed scalar diagnostic drifted" >&2
        cat "$bad_source.out" "$bad_source.err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_list_push_scalar_value_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "list_push_get_loop" ]] || return 0
    for term in \
        'pgy_list_push_int(&xs, (i * i))' \
        'pgy_list_size_int(&xs)' \
        'pgy_list_get_int(&xs, j)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend List scalar ABI missing: $term" >&2
            exit 1
        }
    done
}
