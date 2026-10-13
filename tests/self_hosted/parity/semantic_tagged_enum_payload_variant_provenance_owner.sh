#!/usr/bin/env bash
# Payload reads require source-semantic proof of the receiver's active enum
# variant.  The proof is consumed before MIR or backend artifact publication.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-semantic-enum-payload-variant-provenance"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/semantic_enum_payload_variant_provenance"
WORK_DIR="$ROOT_DIR/$WORK_REL"
VARIANT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_enum_payload_variant_provenance_verdict_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_local_binding_identity_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }

pgy_require_runnable_binary_here "$LABEL:pgy" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL:self-driver" "$SELF_DRIVER" || exit 1
mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*

variant_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$VARIANT_OWNER")" || fail "source-size measurement failed: $VARIANT_OWNER"
[[ "$variant_lines" -le 1450 ]] ||
    fail "variant provenance owner exceeded its cohesive flow cap"
identity_lines="$(python3 "$ROOT_DIR/scripts/source_size_count.py" "$IDENTITY_OWNER")" || fail "source-size measurement failed: $IDENTITY_OWNER"
[[ "$identity_lines" -le 160 ]] ||
    fail "local identity owner exceeded its component cap"

positives=(
    enum_payload_variant_same
    enum_payload_variant_reassignment_latest
    enum_payload_variant_branch_consensus
    enum_payload_variant_match_refined
    enum_payload_variant_direct_constructor
)
negatives=(
    enum_payload_variant_wrong_same_payload
    enum_payload_variant_wrong_distinct_payload
    enum_payload_variant_reassignment_stale
    enum_payload_variant_branch_conflict
    enum_payload_variant_match_cross_arm
    enum_payload_variant_function_return_unproven
    enum_payload_variant_member_unproven
    enum_payload_variant_inout_stale
    enum_payload_variant_loop_continue_stale
    enum_payload_variant_nested_match_binding_unproven
    enum_payload_variant_loop_inout_stale
    enum_payload_variant_assignment_rhs_inout_stale
    enum_payload_variant_if_condition_inout_stale
    enum_payload_variant_if_header_inout_stale
    enum_payload_variant_member_inout_stale
    enum_payload_variant_ref_forward_inout_stale
    enum_payload_variant_generic_receiver_unproven
    enum_payload_variant_match_binding_shadow_unproven
    enum_payload_variant_while_condition_backedge
    enum_payload_variant_same_surface_inout_stale
    enum_payload_variant_for_binding_unproven
)

expected_runtime() {
    case "$1" in
        enum_payload_variant_same) printf '%s\n' 11 ;;
        enum_payload_variant_reassignment_latest) printf '%s\n' 22 ;;
        enum_payload_variant_branch_consensus) printf '%s\n' 31 ;;
        enum_payload_variant_match_refined) printf '%s\n' 41 ;;
        enum_payload_variant_direct_constructor) printf '%s\n' 58 ;;
        *) return 1 ;;
    esac
}

for stem in "${positives[@]}"; do
    source_rel="tests/self_hosted/parity/fixture/$stem.pgy"
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-verified "$source_rel") \
        >"$WORK_DIR/$stem.mir.json" 2>"$WORK_DIR/$stem.direct.err" ||
        fail "$stem did not reach verified MIR"
    [[ -s "$WORK_DIR/$stem.mir.json" && ! -s "$WORK_DIR/$stem.direct.err" ]] ||
        fail "$stem changed the verified-MIR output channels"
    grep -Fq '"schema":"pgy.mir.v1"' "$WORK_DIR/$stem.mir.json" ||
        fail "$stem did not publish canonical MIR"

    suffix=""
    [[ "$PGY" == *.exe ]] && suffix=".exe"
    output_rel="$WORK_REL/$stem$suffix"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$source_rel" --backend=c -o "$output_rel") \
        >"$WORK_DIR/$stem.compile.out" 2>"$WORK_DIR/$stem.compile.err" ||
        fail "$stem public C compilation failed"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.compile.out" \
        "$WORK_DIR/$stem.compile.err" || fail "$stem retried native"
    "$WORK_DIR/$stem$suffix" | tr -d '\r' >"$WORK_DIR/$stem.run"
    expected_runtime "$stem" >"$WORK_DIR/$stem.expected"
    cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem.run" ||
        fail "$stem runtime output drifted"
done

for stem in "${negatives[@]}"; do
    source_rel="tests/self_hosted/parity/fixture/$stem.pgy"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
        "$source_rel") >"$WORK_DIR/$stem.direct.out" \
        2>"$WORK_DIR/$stem.direct.err"
    direct_rc=$?
    set -e
    [[ "$direct_rc" -ne 0 && ! -s "$WORK_DIR/$stem.direct.err" ]] ||
        fail "$stem direct diagnostic did not fail on stdout only"
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' "$WORK_DIR/$stem.direct.out" ||
        fail "$stem lost the public diagnostic marker"
    tail -n +2 "$WORK_DIR/$stem.direct.out" >"$WORK_DIR/$stem.expected.json"
    for fact in \
        '"severity":"error"' \
        '"stage":"semantic"' \
        '"layer":"type"' \
        '"code":"PGY_SEM_ENUM_VARIANT_UNPROVEN"' \
        '"cause_ir":"semantic:enum_payload:active_variant_unproven"' \
        '"fix_source":"narrow-enum-variant-before-projection"'; do
        require_text "$WORK_DIR/$stem.expected.json" "$fact"
    done

    for mode in mir c llvm; do
        artifact=""
        args=("$source_rel" --error-format=json)
        if [[ "$mode" == mir ]]; then
            args+=(--mir)
        else
            artifact="$WORK_REL/$stem-$mode.bin"
            args+=("--backend=$mode" -o "$artifact")
            rm -f "$ROOT_DIR/$artifact"
        fi
        set +e
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
            PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
            "$PGY" "${args[@]}") >"$WORK_DIR/$stem.$mode.out" \
            2>"$WORK_DIR/$stem.$mode.err"
        rc=$?
        set -e
        [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$stem.$mode.out" ]] ||
            fail "$stem $mode published output for an unproven variant"
        cmp -s "$WORK_DIR/$stem.expected.json" "$WORK_DIR/$stem.$mode.err" ||
            fail "$stem $mode did not relay the exact owned receipt"
        [[ -z "$artifact" || ! -e "$ROOT_DIR/$artifact" ]] ||
            fail "$stem $mode published an invalid artifact"
        ! grep -Fq '[pipeline timing]' "$WORK_DIR/$stem.$mode.err" ||
            fail "$stem $mode retried native"
    done
done

echo "[$LABEL] 5 valid flows and 21 counterexamples, exact MIR/C/LLVM failure: PASS"
