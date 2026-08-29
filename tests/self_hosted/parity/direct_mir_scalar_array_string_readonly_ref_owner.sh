#!/usr/bin/env bash
# Exact read-only Array<String> C/LLVM pointer projection and negatives.
# Read-only ArrayString parameter loads consume the carried LLVM projection and reject layout drift.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-string-readonly-ref"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_array_string_readonly_ref"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_array_string_readonly_ref.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
POLICY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_readonly_ref_policy_owner.pgy"
TARGET="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_readonly_ref_target_owner.pgy"
C_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_array_string_readonly_ref_owner.pgy"
LLVM_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_array_string_readonly_ref_owner.pgy"
PROJECTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_array_string_abi_projection_owner.pgy"
EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"
EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_expression_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$POLICY" "$TARGET" "$C_OWNER" "$LLVM_OWNER" \
        "$PROJECTION_OWNER" "$EMISSION_OWNER" "$EXPRESSION_OWNER" \
        "$MUTATIONS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'DirectMirScalarProgramArrayStringReadonlyRefParameterReady(' "$POLICY" ||
    fail "read-only policy owner is missing"
grep -Fq 'DirectMirScalarProgramArrayStringReadonlyRefAt(' "$TARGET" ||
    fail "read-only target owner is missing"
grep -Fq 'DirectMirScalarProgramCArrayStringReadonlyRefCallArgument(' "$C_OWNER" ||
    fail "C read-only call owner is missing"
grep -Fq 'DirectMirScalarProgramLlvmArrayStringReadonlyRefCallArgument(' "$LLVM_OWNER" ||
    fail "LLVM read-only call owner is missing"
grep -Fq 'DirectMirScalarProgramArrayStringAbiProjectionReadyForFact(' "$LLVM_OWNER" ||
    fail "LLVM read-only load does not cross-seal the carried projection"
grep -Fq 'projection.storage.align' "$LLVM_OWNER" ||
    fail "LLVM read-only load does not consume projected storage alignment"
if grep -Fq 'load %pgy.array.string, ptr ' "$LLVM_OWNER" &&
        grep -Fq ', align 8' "$LLVM_OWNER"; then
    fail "LLVM read-only load retained its storage-alignment literal"
fi
if grep -Fq 'DirectMirScalarProgramArrayStringAbiProjectionFromFact(' \
        "$LLVM_OWNER" "$EXPRESSION_OWNER"; then
    fail "LLVM read-only path derives a second target projection"
fi
grep -Fq 'array_string_projection: Option<DirectMirArrayStringAbiProjection>' \
    "$EMISSION_OWNER" || fail "LLVM routine path omits projection carriage"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"type":"Array<String>","carriage":"readonly-ref","resource":"none","pass":"direct","abi_type_name":"Array<String>","abi_layout_id":703020034,"abi_layout_required":true' "$MIR" ||
    fail "producer omitted the read-only Array<String> ABI receipt"
for target_name in ContainsString ForwardContainsString; do
    grep -Fq "\"call_target_name\":\"$target_name\"" "$MIR" ||
        fail "producer omitted call target $target_name"
done
printf 'array-string-readonly-ref-ready\n' >"$WORK_DIR/expected.run"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -std=c11 -DPGY_LLVM_ENABLED \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "canonical runtime object did not compile"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'static bool pgy_scalar_routine_[0-9]+\(const pgy_as \*pgy_param_0, const char\* pgy_param_1\)' "$artifact" ||
            fail "C signature omitted the const Array<String> pointer"
        grep -Fq 'pgy_as_len((*pgy_param_0))' "$artifact" ||
            fail "C parameter read omitted the read-only array value"
        grep -Eq 'pgy_scalar_routine_[0-9]+\(pgy_param_0, pgy_param_1\)' "$artifact" ||
            fail "C forwarding call rebuilt the read-only array"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        grep -Eq 'define internal i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM signature omitted the read-only Array<String> pointer"
        grep -Fq '= load %pgy.array.string, ptr %pgy.param.0, align 8' "$artifact" ||
            fail "LLVM parameter read omitted the array load"
        grep -Eq 'call i1 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, ptr %pgy\.param\.1\)' "$artifact" ||
            fail "LLVM forwarding call rebuilt the read-only array"
        command=("$CLANG" -x ir "$artifact" -x none "$runtime_obj" \
            -pthread -lm -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend artifact did not compile"
        }
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in array-string-readonly-ref-carriage \
        array-string-readonly-ref-pass-shape \
        array-string-readonly-ref-resource array-string-readonly-ref-type \
        array-string-readonly-ref-abi-required \
        array-string-readonly-ref-abi-layout; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
            fail "$backend published an artifact for $mutation"
    done
done

echo "[$LABEL] read-only Array<String> C/LLVM parity + negatives: PASS"
