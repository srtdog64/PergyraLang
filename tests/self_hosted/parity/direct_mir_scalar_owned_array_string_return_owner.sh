#!/usr/bin/env bash
# Exact owned Array<String> return reaches C and LLVM and rejects forged ABI.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-owned-array-string-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_owned_array_string_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_owned_array_string_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
ABI_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_owner.pgy"
POLICY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_parameter_policy_owner.pgy"
SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
ROUTE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_route_envelope_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$ABI_OWNER" "$POLICY_OWNER" "$SIGNATURE_OWNER" \
    "$ROUTE_OWNER" "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'owned_return_present' "$ABI_OWNER" ||
    fail "Array<String> ABI omits owned-return receipt"
grep -Fq 'instruction_kind != "def" && instruction_kind != "return"' \
    "$ABI_OWNER" || fail "Array<String> ABI does not distinguish return rows"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringReturnSignatureReady' \
    "$POLICY_OWNER" || fail "owned-return signature owner is missing"
owned_return_policy_body="$(awk '
    /func DirectMirScalarProgramOwnedArrayStringReturnSignatureReady\(/ { capture = 1 }
    capture { print }
    capture && /^}/ { exit }
' "$POLICY_OWNER")"
if grep -Fq 'signature.param_count < 1' <<<"$owned_return_policy_body"; then
    fail "owned-return signature restored the accidental nonempty parameter boundary"
fi
grep -Fq 'DirectMirScalarCfgScalarTypeSupported(' <<<"$owned_return_policy_body" ||
    fail "owned-return signature does not consume the scalar type owner"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringReturnSignatureReady(' \
    "$SIGNATURE_OWNER" || fail "zero-parameter signature bypasses the owned-return owner"
grep -Fq 'DirectMirScalarProgramOwnedArrayStringReturnSignatureReady(' \
    "$ROUTE_OWNER" || fail "zero-parameter route bypasses the owned-return owner"
if grep -Fq 'signature.param_count != 2' "$POLICY_OWNER"; then
    fail "owned-return signature restored the exact two-parameter fallback"
fi

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"return":"Array<String>"' "$MIR" ||
    fail "producer emitted no owned Array<String> signature"
grep -Fq '"kind":"return"' "$MIR" || fail "producer emitted no return row"
grep -Fq '"abi_type_name":"Array<String>"' "$MIR" ||
    fail "producer emitted no Array<String> return ABI"
grep -Fq '"name":"BuildBoundedValues"' "$MIR" ||
    fail "producer omitted the four-parameter owned-return callable"
grep -Fq '"name":"BuildZeroParameterValues"' "$MIR" ||
    fail "producer omitted the zero-parameter owned-return callable"

printf '1\n2\n0\n1\n2\n0\n1\n2\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'static pgy_as pgy_scalar_routine_' "$artifact" ||
            fail "C callable omitted the owned Array<String> return type"
        grep -Fq '= pgy_scalar_routine_' "$artifact" ||
            fail "C caller did not store the returned carrier"
        grep -Fq 'return pgy_local_' "$artifact" ||
            fail "C callable did not return its local carrier"
        [[ "$(grep -Fc 'pgy_as_drop_owned(&pgy_local_' "$artifact")" -ge 5 ]] ||
            fail "C caller omitted returned-carrier cleanup"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'define internal %pgy.array.string @pgy.scalar.routine.' \
            "$artifact" || fail "LLVM callable omitted the owned return type"
        grep -Fq 'call %pgy.array.string @pgy.scalar.routine.' "$artifact" ||
            fail "LLVM caller did not receive the returned carrier"
        grep -Fq 'ret %pgy.array.string' "$artifact" ||
            fail "LLVM callable did not return the carrier"
        [[ "$(grep -Fc 'call void @pgy_as_drop_owned(ptr %pgy.local.' "$artifact")" -ge 5 ]] ||
            fail "LLVM caller omitted returned-carrier cleanup"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" 2>"$WORK_DIR/$backend.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-string-owned-return-abi-layout \
    array-string-owned-return-kind; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] owned Array<String> return lifecycle and negatives: PASS"
