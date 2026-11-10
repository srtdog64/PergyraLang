#!/usr/bin/env bash
# One scalar Result ABI receipt drives Bool return projection on C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-scalar-result-bool"
DRIVER="$(pgy_select_optional_exe_binary \
    "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_scalar_result_bool.pgy"
BAD_OK="tests/self_hosted/fixtures/direct_mir_scalar_result_bool_bad_ok.pgy"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_result_bool_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for binary in "$DRIVER" "$PGY"; do
    pgy_require_runnable_binary_here "$LABEL" "$binary" || exit 1
done
for command in "$CC" "$CLANG" python3; do
    command -v "$command" >/dev/null 2>&1 || fail "missing tool: $command"
done
[[ -f "$MUTATOR" ]] || fail "missing mutation owner"

mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/result-bool.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
printf 'true\nfalse\ntrue\n' >"$WORK_DIR/expected.run"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE" --backend=c \
    --opt=dev -o "$WORK_REL/native.exe") >"$WORK_DIR/native.log" 2>&1 ||
    { cat "$WORK_DIR/native.log" >&2; fail "native source compile failed"; }
"$WORK_DIR/native.exe" | tr -d '\r' >"$WORK_DIR/native.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" ||
    fail "native runtime output drifted"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" ||
    { cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2;
      fail "MIR producer rejected Result<Bool>"; }
grep -Fq '"abi_type_name":"Result<Bool>"' "$MIR" ||
    fail "MIR omitted Result<Bool> type receipt"
grep -Fq '"runtime_fn":"pgy_result_ok_Bool"' "$MIR" ||
    fail "MIR omitted Result<Bool> runtime receipt"
grep -Fq '"inner_c_type":"bool"' "$MIR" ||
    fail "MIR omitted Result<Bool> payload receipt"

"$CLANG" -std=c11 -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
    -I"$ROOT_DIR/src/runtime" -pthread -c \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" ||
    fail "runtime object compile failed"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    executable="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || {
            cat "$WORK_DIR/$backend.project.out" \
                "$WORK_DIR/$backend.project.err" >&2
            fail "$backend projection failed"
        }
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Fq 'pgy_scalar_result_bool' "$artifact" ||
            fail "C omitted Result<Bool> representation"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$executable")
    else
        grep -Fq '%pgy.scalar.result.bool = type { i32, i1, ptr }' \
            "$artifact" || fail "LLVM omitted Result<Bool> representation"
        command=("$CLANG" -x ir "$artifact" -x none \
            "$WORK_DIR/runtime.o" -pthread -lm -o "$executable")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || {
            cat "$WORK_DIR/$backend.compile.err" >&2
            fail "$backend artifact compile failed"
        }
    "$executable" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in missing-layout layout-id payload-width runtime-symbol \
        return-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    mutated="$ROOT_DIR/$mutated_rel"
    python3 "$MUTATOR" "$MIR" "$mutation" "$mutated"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        output="$ROOT_DIR/$output_rel"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
        grep -Eq 'CODEGEN ERROR:|MIR-LOWER ERROR:' \
            "$WORK_DIR/$mutation.$backend.out" \
            "$WORK_DIR/$mutation.$backend.err" ||
            fail "$backend did not diagnose $mutation"
    done
done

bad_rel="$WORK_REL/bad-ok.mir.json"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$BAD_OK" \
        -o "$bad_rel") >"$WORK_DIR/bad-ok.out" 2>"$WORK_DIR/bad-ok.err"; then
    fail "MIR producer accepted Result<Bool> with Int payload"
fi
[[ ! -e "$ROOT_DIR/$bad_rel" ]] || fail "bad Ok payload published MIR"
grep -Fq 'Code: return_type_mismatch' "$WORK_DIR/bad-ok.out" ||
    fail "bad Ok payload lost its semantic diagnostic"

echo "[$LABEL] native/C/LLVM 3 values, 10 ABI refusals and bad payload refusal: PASS"
