#!/usr/bin/env bash
# One typed Int comparison and wrap-semantics fact family consumed by C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-int-comparison"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_int_comparison"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_int_comparison.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_int_comparison_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"kind":"inequality"' "$MIR" || fail "producer omitted inequality"
grep -Fq '"name":"BoolComparisonCode"' "$MIR" || fail "producer omitted Bool comparison routine"
grep -Fq '"kind":"greater_equal"' "$MIR" || fail "producer omitted greater-equal"
grep -Fq '"kind":"greater"' "$MIR" || fail "producer omitted greater"
grep -Fq '"kind":"less_equal"' "$MIR" || fail "producer omitted less-equal"
printf '1\n1\n-2147483648\n2147483647\n-2147483648\n-2147483648\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'printf("%d\n"' "$artifact" || fail "C artifact did not select the Int32 format"
        grep -Fq ' != ' "$artifact" || fail "C artifact omitted inequality"
        grep -Fq ' >= ' "$artifact" || fail "C artifact omitted greater-equal"
        grep -Fq ' > ' "$artifact" || fail "C artifact omitted greater"
        grep -Fq ' <= ' "$artifact" || fail "C artifact omitted less-equal"
        command=("$CC" -x c -std=c11 -fwrapv "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'c"%lld\0A\00"' "$artifact" || fail "LLVM widened carrier format is missing"
        grep -Fq 'icmp ne i64' "$artifact" || fail "LLVM artifact omitted inequality"
        grep -Fq 'icmp ne i1' "$artifact" || fail "LLVM artifact omitted Bool inequality"
        grep -Fq 'icmp sge i64' "$artifact" || fail "LLVM artifact omitted greater-equal"
        grep -Fq 'icmp sgt i64' "$artifact" || fail "LLVM artifact omitted greater"
        grep -Fq 'icmp sle i64' "$artifact" || fail "LLVM artifact omitted less-equal"
        grep -Fq '.wide = add i64' "$artifact" || fail "LLVM artifact omitted widened Int addition"
        grep -Fq '.narrow = trunc i64' "$artifact" || fail "LLVM artifact omitted Int narrowing"
        grep -Fq ' = sext i32 ' "$artifact" || fail "LLVM artifact omitted Int sign normalization"
        ! grep -Fq 'add nsw i64' "$artifact" || fail "LLVM expression marked wrap addition nsw"
        ! grep -Fq 'sub nsw i32' "$artifact" || fail "LLVM Abs marked wrap subtraction nsw"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for mutation in mixed-type bool-mixed-type out-of-range-literal; do
    invalid_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$invalid_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$invalid_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation comparison/counter facts"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation facts"
    done
done

echo "[$LABEL] typed comparison/wrap C/LLVM parity + mixed-type negatives: PASS"
