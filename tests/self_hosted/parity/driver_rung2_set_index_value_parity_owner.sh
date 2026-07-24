#!/usr/bin/env bash
# Owns graph-indexed scalar argument typing at the Set call boundary.
# Forbidden: Set-local source reparse, element guessing, and backend recovery.

pgy_selfhost_verify_driver_rung2_set_index_value() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local source bad_source out err
    [[ "$base" == "loop_collect_distinct_set" ]] || return 0

    for fact in \
        '"result":"words.1","arg0":"words","arg1":null,"slot_anchor":null,"abi_type_name":"Array<String>"' \
        '"result":"distinct.1","arg0":"distinct","arg1":null,"slot_anchor":null,"abi_type_name":"Set<String>"' \
        '"expr0":"SetAdd(distinct, words[i])"' \
        '"kind":"index","text":"words[i]"' \
        '"call_target_name":"SetAdd"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend Set index fact drifted: $fact" >&2
            exit 1
        }
    done

    source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
    bad_source="$BUILD_DIR/${base}_${backend}.bad-index-type.pgy"
    pgy_replace_first_literal "$source" "$bad_source" \
        'SetAdd(distinct, words[i]);' 'SetAdd(distinct, words["bad"]);'
    out="$bad_source.out"; err="$bad_source.err"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$bad_source")" \
        --emit-c-verified >"$out" 2>"$err"); then
        echo "[self-host-parity:driver-rung2] $backend String Set index was accepted" >&2
        exit 1
    fi
    grep -Fq 'Code: ast_artifact_invalid' "$out" "$err" &&
        grep -Fq 'owner: collection_value_type' "$out" "$err" || {
        echo "[self-host-parity:driver-rung2] $backend Set index diagnostic drifted" >&2
        cat "$out" "$err" >&2; exit 1
    }
}

pgy_selfhost_verify_driver_rung2_set_index_value_emitted_c() {
    local backend="$1" base="$2" emitted_c="$3"
    [[ "$base" == "loop_collect_distinct_set" ]] || return 0
    for term in 'PgySet_String distinct = pgy_set_new_string()' \
        'pgy_set_add_string(&distinct, pgy_as_get(words, i))' \
        'pgy_set_size_string(&distinct)' \
        'pgy_set_has_string(&distinct, "red")'; do
        grep -Fq "$term" "$emitted_c" || {
            echo "[self-host-parity:driver-rung2] $backend Set index C missing: $term" >&2
            exit 1
        }
    done
}
