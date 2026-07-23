#!/usr/bin/env bash
# Owns ListGet return-type carriage into compound-expression codegen.

pgy_selfhost_verify_driver_rung2_list_int_loop() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local missing_return out err
    [[ "$base" == "list_int_loop" ]] || return 0

    for fact in \
        '"name":"xs","type":"List<Int>"' \
        '"call_target_kind":"direct","call_target_name":"ListGet"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend ListGet loop fact drifted: $fact" >&2
            exit 1
        }
    done

    missing_return="$BUILD_DIR/${base}_${backend}.missing-list-get-return.mir.json"
    out="$missing_return.out"
    err="$missing_return.err"
    pgy_replace_first_literal "$self_mir_json" "$missing_return" \
        '"call_target_kind":"direct","call_target_name":"ListGet"' \
        '"call_target_kind":"direct","call_target_name":"ListPush"'
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$missing_return")" \
        >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend mutated ListGet return fact was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: ast_artifact_invalid' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend ListGet return mutation diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    }
}

pgy_selfhost_verify_driver_rung2_list_int_loop_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "list_int_loop" ]] || return 0
    for symbol in pgy_list_size_int pgy_list_get_int; do
        grep -Fq "$symbol" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend ListGet loop runtime symbol missing: $symbol" >&2
            exit 1
        }
    done
    grep -Fq 'total + pgy_list_get_int(&xs, i)' "$emitted_c" || {
        echo "[self-host-parity:driver-rung2] $backend ListGet return was not carried into addition" >&2
        exit 1
    }
}
