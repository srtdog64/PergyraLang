#!/usr/bin/env bash
# Checked native MIR -> production String caller source gate -> C/LLVM.
# Native MIR is the declared oracle here, never a source-verifier fallback.
# This does not claim source admission, exclusive lifetime, CLI or fixed point.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-string-argument-source"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
ISSUER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
MANIFEST="${PGY_SELFHOST_MACHINE_LAYER_DECLARATION:-$(pgy_self_driver_machine_manifest_path "$ISSUER")}"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
BUILD_TIMEOUT="${PGY_SELFHOST_PROBE_BUILD_TIMEOUT:-300}s"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
PROBE="tests/self_hosted/fixtures/direct_mir_owned_string_scalar_projection_probe.pgy"
FIXTURES="tests/self_hosted/parity/fixture/owned_string_argument_sources"
OWNER="src/self_hosted/compiler/direct_mir_scalar_program_owned_string_argument_source_owner.pgy"
CALLER="src/self_hosted/compiler/direct_mir_scalar_cfg_program_direct_call_carriage_owner.pgy"
LITERAL_OWNER="src/self_hosted/compiler/direct_mir_scalar_program_array_string_literal_operand_admission_owner.pgy"
IDENTITY_MUTATOR="tests/self_hosted/parity/direct_mir_owned_string_literal_identity_mutations.py"
POSITIVES=(existing binary_field_positive nary_field_positive)
NEGATIVES=(borrowed_local_negative borrowed_member_negative default_formal_negative
    own_formal_unproved_negative local_alias_unproved_negative
    record_alias_unproved_negative nary_first_borrowed_negative nary_last_borrowed_negative
    reassigned_local_negative reassigned_member_negative)
IDENTITY_NEGATIVES=(literal_foreign_formal literal_foreign_routine literal_receiver_binding)
fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
[[ -s "$MANIFEST" ]] || fail "missing issued machine declaration"
for tool in "$CC" "$CLANG" python3 timeout mktemp sha256sum cmp cp find sort xargs; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
cd "$ROOT_DIR"
mkdir -p .tmp/self_hosted
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned_string_argument_source.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
echo "[$LABEL] evidence: $WORK_DIR"
run_checked() {
    local stage="$1" budget="$2"; shift 2
    if ! timeout "$budget" "$@" >"$WORK_DIR/$stage.out" 2>"$WORK_DIR/$stage.err"; then
        cat "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err" >&2
        fail "$stage failed"
    fi
}
cp -- "$MANIFEST" "$WORK_DIR/machine-layer.json"
cmp -s "$MANIFEST" "$WORK_DIR/machine-layer.json" || fail "issued declaration copy changed"
sha256sum "$PGY" "$MANIFEST" "$PROBE" "$OWNER" "$CALLER" "$LITERAL_OWNER" "$IDENTITY_MUTATOR" \
    src/self_hosted/compiler/direct_mir_scalar_program_owned_string_result_fact_owner.pgy \
    tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy \
    "$FIXTURES"/*.pgy >"$WORK_DIR/inputs.sha256"
find src/self_hosted -type f -name '*.pgy' -print0 | sort -z | \
    xargs -0 sha256sum >"$WORK_DIR/owners.sha256"
run_checked probe.emit "$BUILD_TIMEOUT" "$PGY" "$PROBE" --native-pipeline --emit-c \
    -o "$WORK_REL/probe.c"
export PGY_SELFHOST_CC_PROFILE=test
pgy_selfhost_select_emitted_c_compile_profile
run_checked probe.compile "$BUILD_TIMEOUT" "$CC" -x c -std=c11 \
    "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" -Isrc -Isrc/runtime -pthread \
    "$WORK_DIR/probe.c" -lm -o "$WORK_DIR/probe.exe"
sha256sum "$WORK_DIR/probe.c" "$WORK_DIR/probe.exe" >"$WORK_DIR/probe.sha256"
run_checked runtime.compile "$STEP_TIMEOUT" "$CLANG" -DPGY_LLVM_ENABLED -Isrc \
    -Isrc/runtime -c src/runtime/pgy_runtime_lib.c -o "$WORK_DIR/runtime.o"
for case_name in "${POSITIVES[@]}" "${NEGATIVES[@]}"; do
    source_path="$FIXTURES/$case_name.pgy"
    [[ "$case_name" != existing ]] || source_path="tests/self_hosted/fixtures/direct_mir_owned_string_parameter.pgy"
    run_checked "$case_name.produce" "$STEP_TIMEOUT" "$PGY" \
        --native-pipeline --mir-json "$source_path"
    cp -- "$WORK_DIR/$case_name.produce.out" "$WORK_DIR/$case_name.mir.json"
done
for case_name in "${IDENTITY_NEGATIVES[@]}"; do
    run_checked "$case_name.mutate" "$STEP_TIMEOUT" python3 "$IDENTITY_MUTATOR" \
        "$WORK_DIR/existing.mir.json" "$case_name" "$WORK_DIR/$case_name.mir.json"
done
for case_name in "${POSITIVES[@]}"; do
    expected="owned-string-source-ready"
    [[ "$case_name" != existing ]] || expected="released"
    printf '%s\n' "$expected" >"$WORK_DIR/$case_name.expected"
    for backend in c llvm; do
        extension=c; [[ "$backend" != llvm ]] || extension=ll
        artifact="$WORK_DIR/$case_name.$extension"
        run_checked "$case_name.$backend.project" "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" \
            "$WORK_REL/$case_name.mir.json" "$WORK_REL/machine-layer.json" \
            "$backend" "$WORK_REL/$case_name.$extension"
        [[ -s "$artifact" ]] || fail "$case_name/$backend published no artifact"
        if [[ "$backend" == c ]]; then
            run_checked "$case_name.c.compile" "$STEP_TIMEOUT" "$CC" -x c -std=c11 \
                -Isrc -Isrc/runtime -pthread "$artifact" -lm -o "$WORK_DIR/$case_name-c.exe"
        else
            run_checked "$case_name.llvm.compile" "$STEP_TIMEOUT" "$CLANG" -x ir "$artifact" \
                -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$WORK_DIR/$case_name-llvm.exe"
        fi
        run_checked "$case_name.$backend.run" "$STEP_TIMEOUT" "$WORK_DIR/$case_name-$backend.exe"
        tr -d '\r' <"$WORK_DIR/$case_name.$backend.run.out" >"$WORK_DIR/$case_name.$backend.normalized"
        cmp -s "$WORK_DIR/$case_name.expected" "$WORK_DIR/$case_name.$backend.normalized" ||
            fail "$case_name/$backend runtime output drifted"
    done
done
for case_name in "${NEGATIVES[@]}" "${IDENTITY_NEGATIVES[@]}"; do
    for backend in c llvm; do
        artifact="$WORK_DIR/$case_name.$backend.sentinel"
        printf 'not-published\n' >"$artifact"
        if timeout "$STEP_TIMEOUT" "$WORK_DIR/probe.exe" \
            "$WORK_REL/$case_name.mir.json" "$WORK_REL/machine-layer.json" \
            "$backend" "$WORK_REL/$case_name.$backend.sentinel" \
            >"$WORK_DIR/$case_name.$backend.out" 2>"$WORK_DIR/$case_name.$backend.err"; then
            fail "$case_name/$backend admitted an unproved String source"
        else
            status="$?"
            [[ "$status" != 124 && "$status" != 137 ]] || fail "$case_name/$backend timed out"
        fi
        diagnostic='target_carriage=owner-handle target_type=String'
        if [[ "$case_name" == reassigned_local_negative ||
              "$case_name" == reassigned_member_negative ]]; then
            # This lane is still unsupported by the earlier assignment owner;
            # do not attribute its refusal to the newly reached source guard.
            diagnostic='stage=identity_cell_store_target ordinal=0 block=0 row=6 source=AST_ASSIGNMENT'
        elif [[ "$case_name" == literal_* ]]; then
            diagnostic="$(cat "$WORK_DIR/$case_name.mutate.out")"
        fi
        grep -Fq "$diagnostic" \
            "$WORK_DIR/$case_name.$backend.out" "$WORK_DIR/$case_name.$backend.err" ||
            fail "$case_name/$backend missed its exact refusal owner"
        [[ "$(cat "$artifact")" == not-published ]] || fail "$case_name/$backend overwrote the sentinel"
    done
done
sha256sum -c "$WORK_DIR/inputs.sha256" >"$WORK_DIR/inputs-verified.out" || fail "inputs changed"
sha256sum -c "$WORK_DIR/owners.sha256" >"$WORK_DIR/owners-verified.out" || fail "owners changed"
sha256sum -c "$WORK_DIR/probe.sha256" >"$WORK_DIR/probe-verified.out" || fail "issued probe changed"
echo "[$LABEL] three C/LLVM programs; eight caller, two earlier assignment and three exact literal-ID refusal pairs: PASS"
