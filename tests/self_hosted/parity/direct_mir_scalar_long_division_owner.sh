#!/usr/bin/env bash
# Dynamic Long division consumes one checked runtime ABI in C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-long-division"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_long_division"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_long_division.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_division_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_division_artifact_variants.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"divide","text":"left / right"' "$MIR" ||
    fail "producer omitted the Long division graph"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
    >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
    fail "runtime ABI object did not compile"

compile_artifact() {
    local backend="$1" artifact="$2" bin="$3" label="$4"
    if [[ "$backend" == c ]]; then
        local command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$label.compile.out" \
            2>"$WORK_DIR/$label.compile.err" || fail "$label C compile failed"
    else
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm \
            -o "$bin" >"$WORK_DIR/$label.compile.out" \
            2>"$WORK_DIR/$label.compile.err" || fail "$label LLVM compile failed"
    fi
}

printf '14\n' >"$WORK_DIR/expected-positive.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    grep -Fq 'pgy_checked_div_i64_export' "$artifact" ||
        fail "$backend omitted the canonical Long division ABI"
    if [[ "$backend" == c ]]; then
        grep -Fq 'static int64_t pgy_scalar_routine_1(int64_t pgy_param_0, int64_t pgy_param_1)' "$artifact" ||
            fail "C Long routine signature did not consume the canonical int64_t ABI"
        ! grep -Fq 'static int32_t pgy_scalar_routine_1' "$artifact" ||
            fail "C narrowed the Long routine signature to Int"
        ! grep -Eq 'return .* / .*;' "$artifact" ||
            fail "C reintroduced raw signed Long division"
    else
        ! grep -Fq 'sdiv i64' "$artifact" ||
            fail "LLVM reintroduced raw signed Long division"
        grep -Fq 'declare i64 @pgy_checked_div_i64_export(i64, i64)' "$artifact" ||
            fail "LLVM omitted the exact Long division declaration"
    fi
    for mode in positive minimum-minus-one zero-divisor; do
        variant="$WORK_DIR/$backend-$mode.$extension"
        python "$VARIANTS" "$artifact" "$backend" "$mode" "$variant"
        variant_bin="$WORK_DIR/$backend-$mode.exe"
        compile_artifact "$backend" "$variant" "$variant_bin" "$backend-$mode"
        if [[ "$mode" == positive ]]; then
            "$variant_bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
            cmp -s "$WORK_DIR/expected-positive.run" "$WORK_DIR/$backend-$mode.run" ||
                fail "$backend positive division drifted"
        elif "$variant_bin" >"$WORK_DIR/$backend-$mode.run" \
                2>"$WORK_DIR/$backend-$mode.err"; then
            fail "$backend accepted $mode Long division"
        elif [[ "$mode" == minimum-minus-one ]]; then
            grep -Fq 'class=arithmetic-overflow reason=signed division overflow' \
                "$WORK_DIR/$backend-$mode.err" || fail "$backend overflow panic drifted"
        else
            grep -Fq 'class=divide-by-zero reason=integer division or modulo by zero' \
                "$WORK_DIR/$backend-$mode.err" || fail "$backend zero panic drifted"
        fi
    done
done

for mutation in wrong-right-type wrong-expression-kind; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] checked Long division C/LLVM parity + MIN/-1/zero negatives: PASS"
