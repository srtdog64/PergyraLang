#!/usr/bin/env bash
# Entrypoint AST_RETURN_VOID retires owned locals and returns process status zero.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-entrypoint-early-return"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_entrypoint_early_return"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_scalar_entrypoint_early_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"
ADMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_routine_admission_owner.pgy"
C_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_emission_owner.pgy"
LLVM_EMISSION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_llvm_emission_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$MUTATIONS" "$ADMISSION" "$C_EMISSION" "$LLVM_EMISSION"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

if grep -Fq 'routine_ordinal == 0 || instruction_row + 1' "$ADMISSION"; then
    fail "routine admission still rejects every explicit entrypoint return"
fi
grep -Fq 'DirectMirScalarProgramCStringArrayCleanup(plan, routine)' "$C_EMISSION" ||
    fail "C entrypoint return bypasses owned-array cleanup"
grep -Fq 'DirectMirScalarProgramLlvmStringArrayCleanup(' "$LLVM_EMISSION" ||
    fail "LLVM entrypoint return bypasses owned-array cleanup"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"source_type":"AST_RETURN_VOID"' "$MIR" ||
    fail "producer omitted the explicit entrypoint return receipt"
printf '2\nentrypoint-early-return-ready\n' >"$WORK_DIR/expected.run"

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
        [[ "$(grep -Fc 'pgy_as_drop_owned(&pgy_local_' "$artifact")" -ge 1 ]] ||
            fail "C did not retire the owned array on the entrypoint exit"
        [[ "$(grep -Fc 'return 0;' "$artifact")" -ge 1 ]] ||
            fail "C did not return status zero on the entrypoint exit"
        if grep -Eq '^    return;$' "$artifact"; then
            fail "C emitted a void return from int main"
        fi
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        [[ "$(grep -Fc 'call void @pgy_as_drop_owned' "$artifact")" -ge 1 ]] ||
            fail "LLVM did not retire the owned array on the entrypoint exit"
        [[ "$(grep -Fc 'ret i32 0' "$artifact")" -ge 1 ]] ||
            fail "LLVM did not return status zero on the entrypoint exit"
        main_ir="$(awk '/^define i32 @main/{inside=1} inside{print} inside && /^}/{exit}' "$artifact")"
        if grep -Fq 'ret void' <<<"$main_ir"; then
            fail "LLVM emitted a void return from i32 main"
        fi
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

mutation="entrypoint-void-return-source"
mutated_rel="$WORK_REL/$mutation.mir.json"
python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
for backend in c llvm; do
    output_rel="$WORK_REL/$mutation.$backend"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$mutated_rel" -o "$output_rel") \
        >"$WORK_DIR/$mutation.$backend.out" \
        2>"$WORK_DIR/$mutation.$backend.err"; then
        fail "$backend accepted the forged entrypoint return source"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
        fail "$backend published an artifact for the forged return"
done

echo "[$LABEL] explicit entrypoint return C/LLVM cleanup parity + negative: PASS"
