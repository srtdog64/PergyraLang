#!/usr/bin/env bash
# Test protocol only: exact observed refusal reasons from existing owners.
owned_collection_refusal_reason() {
    case "$1" in
        flip-final|flip-final-live|delete-drop|prior-live-final-drop|missing-receipt|wrong-receipt-op|wrong-receipt-root|wrong-receipt-source)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program routine admission stage is invalid: stage=collection_ownership_transition ordinal=0 block=-1 row=-1 source=' ;;
        duplicate-drop|duplicate-definition)
            printf '%s' 'MIR-LOWER ERROR: MIR instruction identities are missing or duplicated' ;;
        missing-declaration)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG instruction LocalRef coverage is invalid' ;;
        wrong-declaration-role)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG LocalRef declaration role or owner is invalid' ;;
        wrong-declaration|wrong-operand-type)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program routine admission stage is invalid: stage=local_inventory ordinal=0 block=-1 row=-1 source=' ;;
        wrong-producer)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program expression admission is invalid: stage=leaf-operand node=0 row=2 source=AST_LET_DECL' ;;
        coherent-wrong-key-type|wrong-key-argument)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program expression admission is invalid: stage=builtin-call node=5 row=1 source=AST_CALL' ;;
        coherent-wrong-value-type|wrong-value-argument)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program expression admission is invalid: stage=builtin-call node=7 row=1 source=AST_CALL' ;;
        wrong-call-arity)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program expression admission is invalid: stage=graph-root-mapping node=5 row=1 source=AST_CALL' ;;
        wrong-call-identity)
            printf '%s' 'CODEGEN ERROR: direct MIR scalar CFG program expression admission is invalid: stage=direct-call node=3 row=2 source=AST_LET_DECL' ;;
        *) return 1 ;;
    esac
}

owned_collection_refusal_ready() {
    local mode="$1" status="$2" output="$3" errors="$4" expected
    [[ -f "$output" && -f "$errors" && ! -s "$errors" ]] || return 1
    case "$mode" in
        malformed-type-angle|malformed-type-comma|unknown-source-type|unknown-map-value-type)
            [[ "$status" == 2 && ! -s "$output" ]] ;;
        *)
            [[ "$status" == 1 ]] || return 1
            expected="$(owned_collection_refusal_reason "$mode")" || return 1
            [[ "$(tr -d '\r' <"$output")" == "$expected" ]] ;;
    esac
}

owned_collection_refusal_selftest() {
    local work="$1" mode output="$1/protocol.out" errors="$1/protocol.err"
    : >"$errors"
    for mode in flip-final flip-final-live delete-drop prior-live-final-drop missing-receipt wrong-receipt-op wrong-receipt-root wrong-receipt-source duplicate-drop duplicate-definition missing-declaration wrong-declaration-role wrong-declaration wrong-operand-type wrong-producer coherent-wrong-key-type wrong-key-argument coherent-wrong-value-type wrong-value-argument wrong-call-arity wrong-call-identity; do
        owned_collection_refusal_reason "$mode" >"$output"
        owned_collection_refusal_ready "$mode" 1 "$output" "$errors" || return 1
    done
    for mode in malformed-type-angle malformed-type-comma unknown-source-type unknown-map-value-type; do
        : >"$output"
        owned_collection_refusal_ready "$mode" 2 "$output" "$errors" || return 1
        if owned_collection_refusal_ready "$mode" 1 "$output" "$errors"; then return 1; fi
    done
    owned_collection_refusal_reason delete-drop >"$output"
    for status in 0 2 124 139; do
        if owned_collection_refusal_ready delete-drop "$status" "$output" "$errors"; then return 1; fi
    done
    if owned_collection_refusal_ready missing-declaration 1 "$output" "$errors"; then return 1; fi
    if owned_collection_refusal_ready unknown-mode 1 "$output" "$errors"; then return 1; fi
    if owned_collection_refusal_ready delete-drop 1 "$work/absent" "$errors"; then return 1; fi
    printf '%s\n' 'CODEGEN ERROR: unrelated failure' >"$output"
    if owned_collection_refusal_ready delete-drop 1 "$output" "$errors"; then return 1; fi
    : >"$output"
    if owned_collection_refusal_ready delete-drop 1 "$output" "$errors"; then return 1; fi
    printf '%s\n' false >"$output"
    if owned_collection_refusal_ready delete-drop 0 "$output" "$errors"; then return 1; fi
    owned_collection_refusal_reason delete-drop >"$output"
    printf '%s\n' 'unexpected stderr' >"$errors"
    if owned_collection_refusal_ready delete-drop 1 "$output" "$errors"; then return 1; fi
    : >"$errors"
    printf '\nextra output\n' >>"$output"
    if owned_collection_refusal_ready delete-drop 1 "$output" "$errors"; then return 1; fi
}
