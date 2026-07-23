#!/usr/bin/env bash
# Owns graph-derived foreach iterable type parity and negative mutations.

pgy_selfhost_iteration_expression_reject_mutation() {
    local backend="$1" driver_bin="$2" base="$3" source="$4"
    local from="$5" to="$6" actual="$7"
    local mutated="$BUILD_DIR/${base}_${backend}.bad-iterable.pgy"
    local self_out="$mutated.self.out" self_err="$mutated.self.err"
    local oracle_bin="$mutated.oracle.exe" oracle_log="$mutated.oracle.log"

    pgy_replace_first_literal "$source" "$mutated" "$from" "$to"
    if (cd "$ROOT_DIR" && "$driver_bin" \
        "$(pgy_selfhost_path_relative_to_root "$mutated")" \
        --emit-c-verified >"$self_out" 2>"$self_err"); then
        echo "[self-host-parity:driver-rung2] $backend invalid foreach iterable was accepted: $base" >&2
        exit 1
    fi
    for fact in "Code: statement_type_unresolved" "- actual: $actual"; do
        grep -Fq -- "$fact" "$self_out" "$self_err" || {
            echo "[self-host-parity:driver-rung2] $backend foreach diagnostic drifted: $base/$fact" >&2
            cat "$self_out" "$self_err" >&2
            exit 1
        }
    done
    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$mutated")" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$oracle_bin")" \
        >"$oracle_log" 2>&1); then
        echo "[self-host-parity:driver-rung2] native oracle accepted invalid foreach iterable: $base" >&2
        exit 1
    fi
}

pgy_selfhost_verify_driver_rung2_iteration_expression() {
    local backend="$1" base="$2" self_mir_json="$3" driver_bin="$4"
    local source
    case "$base" in
        for_in_array_literal_iterable)
            grep -Fq '"kind":"array_literal","text":"[]"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend foreach array graph was lost" >&2
                exit 1
            }
            source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
            pgy_selfhost_iteration_expression_reject_mutation \
                "$backend" "$driver_bin" "$base" "$source" \
                '[10, 20, 30]' '[10, "bad", 30]' 'Unknown'
            ;;
        for_in_member_iterable)
            grep -Fq '"kind":"member_access","text":"b.items"' "$self_mir_json" || {
                echo "[self-host-parity:driver-rung2] $backend foreach member graph was lost" >&2
                exit 1
            }
            source="$ROOT_DIR/tests/cases/backend_compare/$base/main.pgy"
            pgy_selfhost_iteration_expression_reject_mutation \
                "$backend" "$driver_bin" "$base" "$source" \
                'for x in b.items' 'for x in b' 'Bag'
            ;;
        *) return 0 ;;
    esac
    for fact in \
        '"binding_type":"Int","iterable_type":"Array<Int>","collection_hoisted":true' \
        '"name":"__pgy_forin_0","type":"Array<Int>"'; do
        grep -Fq "$fact" "$self_mir_json" || {
            echo "[self-host-parity:driver-rung2] $backend foreach owner fact drifted: $base/$fact" >&2
            exit 1
        }
    done
}
