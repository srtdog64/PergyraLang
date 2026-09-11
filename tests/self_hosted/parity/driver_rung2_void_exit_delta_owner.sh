#!/usr/bin/env bash
# A Void body that falls off its end records an exit instruction in the native
# MIR oracle and none in the Pergyra producer.  Neither spelling is wrong: the
# native side cannot tell an explicit `return;` from an implicit exit, so both
# arrive as one bare AST_RETURN_VOID, while the self producer terminates the
# block without recording a statement that the source never wrote.
#
# This owner admits only that named delta.  The oracle artifact with every
# trailing bare Void return removed must equal the self artifact byte for
# byte; any other difference stays a parity failure.  The waiver disappears
# automatically once the self producer records the same exit.

pgy_selfhost_driver_rung2_void_exit_delta_pattern() {
    # One bare Void return closing an instruction array.  Every payload field
    # is pinned null so a return carrying a value, an expression or a use can
    # never be removed, and the trailing `]` pins the tail position: only an
    # exit that falls off the end of a block is admitted.
    local head=',\{"id":[0-9]+,"kind":"return","name":"return","result":null'
    local operands=',"arg0":null,"arg1":null,"slot_anchor":null'
    local layout=',"abi_type_name":null,"abi_layout_id":0'
    local layout_tail=',"abi_layout_required":false,"abi_layout":null'
    local exprs=',"expr0":null,"expr0_graph":null,"expr1":null,"expr1_graph":null'
    local origin=',"source_type":"AST_RETURN_VOID"'
    printf '%s%s%s%s%s%s%s' "$head" "$operands" "$layout" "$layout_tail" \
        "$exprs" "$origin" ',[^{}]*,"uses":\[\]\}\]'
}

pgy_selfhost_driver_rung2_void_exit_delta() {
    local label="$1" build_dir="$2" oracle_canonical="$3" self_canonical="$4"
    local oracle_norm="$build_dir/void_exit_oracle_$$.txt"
    local self_norm="$build_dir/void_exit_self_$$.txt"
    local stripped="$build_dir/void_exit_stripped_$$.txt"
    local pattern

    pattern="$(pgy_selfhost_driver_rung2_void_exit_delta_pattern)" || return 1
    pgy_selfhost_normalize_text_artifact <"$oracle_canonical" \
        >"$oracle_norm" || return 1
    pgy_selfhost_normalize_text_artifact <"$self_canonical" \
        >"$self_norm" || return 1
    sed -E "s/$pattern/]/g" "$oracle_norm" >"$stripped" || return 1
    # Nothing removed means this is not the Void fall-off delta; leave the
    # verdict to the ordinary artifact comparison.
    if cmp -s -- "$oracle_norm" "$stripped"; then
        return 1
    fi
    cmp -s -- "$stripped" "$self_norm" || return 1
    echo "[$label] admits the bounded legacy-oracle Void fall-off exit delta"
    return 0
}
