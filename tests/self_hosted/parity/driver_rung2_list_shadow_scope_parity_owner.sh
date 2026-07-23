#!/usr/bin/env bash
# Owns lexical List binding identity, declaration ABI type carriage, and scope restoration.

pgy_selfhost_verify_driver_rung2_list_shadow_scope() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_type out err
    [[ "$base" == "list_shadow_scope_metadata" ]] || return 0

    for fact in \
        '"result":"items.1","arg0":"items","arg1":null,"slot_anchor":null,"abi_type_name":"List<Int>"' \
        '"result":"items.2","arg0":"items","arg1":null,"slot_anchor":null,"abi_type_name":"List<String>"' \
        '"expr0":"ListPush(items, \"shadow\")"' \
        '"uses":["items.2"]' \
        '"expr0":"ListPush(items, 20)"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend List shadow fact drifted: $fact" >&2
            exit 1
        }
    done
    [[ "$(grep -oF '"uses":["items.1"]' "$self_mir_json" | wc -l | tr -d ' ')" -eq 4 ]] || {
        echo "[self-host-parity:driver-rung2] $backend outer List identity was not restored" >&2
        exit 1
    }

    missing_type="$BUILD_DIR/${base}_${backend}.missing-inner-local-type.mir.json"
    out="$missing_type.out"
    err="$missing_type.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_type" \
        '"abi_type_name":"List<String>"' '"abi_type_name":null'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_type")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend missing inner List type was accepted" >&2
        exit 1
    fi
    grep -Fq 'local declaration is missing its MIR ABI type fact' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend missing inner List type diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_list_shadow_scope_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "list_shadow_scope_metadata" ]] || return 0
    for term in \
        'PgyList_Int items = pgy_list_new_int()' \
        'PgyList_String items = pgy_list_new_string()' \
        'pgy_list_push_string(&items, "shadow")' \
        'pgy_list_get_string(&items, 0)' \
        'pgy_list_push_int(&items, 20)' \
        'pgy_list_get_int(&items, 1)'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend scoped List C fact drifted: $term" >&2
            exit 1
        }
    done
}
