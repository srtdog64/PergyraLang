#!/usr/bin/env bash
# Direct scalar String(String, Int) callable C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-direct-scalar-callable"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_direct_scalar_callable"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_direct_scalar_string_return.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_direct_scalar_mutations.py"
LITERAL_MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_string_literal_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"EchoAt"' "$MIR" || fail "producer omitted callable"
grep -Fq '"name":"CodeAt"' "$MIR" || fail "producer omitted Int callable"
grep -Fq '"return":"String"' "$MIR" || fail "producer omitted String return"
grep -Fq '"expr0":"EchoAt(\"discarded\", 0)"' "$MIR" ||
    fail "producer omitted the discarded String-return call"
grep -Fq '"expr0":"Print(EchoAt(\"nested\", 0))"' "$MIR" ||
    fail "producer omitted nested Print"
printf 'print:nesteddirect-scalar-ready\nline\nquote"slash\\x\n7\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'static void pgy_print(const char *s)' "$artifact" ||
            fail "C Print helper was not materialized"
        grep -Eq '^static const char\* pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, int32_t pgy_param_1\)' "$artifact" || fail "C direct scalar signature drifted"
        grep -Eq '^static int32_t pgy_scalar_routine_[0-9]+\(const char\* pgy_param_0, int32_t pgy_param_1, int32_t pgy_param_2\)' "$artifact" || fail "C direct scalar Int signature drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Fq 'define internal void @pgy_print(ptr %value)' "$artifact" ||
            fail "LLVM Print helper was not materialized"
        grep -Eq '^define internal ptr @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, i64 %pgy\.param\.1\)' "$artifact" || fail "LLVM direct scalar signature drifted"
        grep -Eq '^define internal i64 @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0, i64 %pgy\.param\.1, i64 %pgy\.param\.2\)' "$artifact" || fail "LLVM direct scalar Int signature drifted"
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for mutation in carriage pass resource abi-required discarded-extra-use \
        discarded-noncall print-wrong-type print-wrong-arity \
        print-forged-target print-extra-use; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

invalid_rel="$WORK_REL/invalid-string-escape.mir.json"
python "$LITERAL_MUTATIONS" "$MIR" "$ROOT_DIR/$invalid_rel"
for backend in c llvm; do
    output_rel="$WORK_REL/invalid-string-escape.$backend"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$invalid_rel" -o "$output_rel") >"$WORK_DIR/invalid-string-escape.$backend.out" \
        2>"$WORK_DIR/invalid-string-escape.$backend.err"; then
        fail "$backend accepted an unsupported string escape"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published an invalid string literal"
done

echo "[$LABEL] direct scalar String(String, Int) C/LLVM parity + negatives: PASS"
