#!/usr/bin/env bash
# Readonly Int/String array identity, projected ABI, value reborrow and no copy-out.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-readonly-array-int"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_array_int_readonly_ref.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
for binary in "$DRIVER" "$PGY"; do pgy_require_runnable_binary_here "$LABEL" "$binary" || exit 1; done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/readonly-array-int.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $WORK_REL"
printf '3\n7\n2147483647\n1\n4\n4\n2\n0\n2\n2\n한글🙂\n' >"$WORK_DIR/expected.run"
(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE" --backend=c --opt=dev \
    -o "$WORK_REL/native.exe") >"$WORK_DIR/native.log" 2>&1 || fail "native source compilation failed"
"$WORK_DIR/native.exe" | tr -d '\r' >"$WORK_DIR/native.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" || fail "native runtime differed"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" -o "$WORK_REL/program.mir.json") \
    >"$WORK_DIR/producer.log" 2>&1 || { cat "$WORK_DIR/producer.log" >&2; fail "MIR producer rejected source"; }
"$CLANG" -std=c11 -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.log" 2>&1 || fail "runtime compilation failed"
for backend in c llvm; do
    artifact="$WORK_DIR/program.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/program.mir.json" \
        -o "$WORK_REL/program.$backend") >"$WORK_DIR/$backend.project.log" 2>&1 || {
        cat "$WORK_DIR/$backend.project.log" >&2; fail "$backend projection failed";
    }
    if [[ "$backend" == c ]]; then
        grep -Eq 'const pgy_ai \*pgy_param_[01]' "$artifact" || fail "C omitted readonly pointer"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK_DIR/direct-$backend.exe")
    else
        grep -Fq '= load %pgy.array.int, ptr %pgy.param.1, align 8' "$artifact" || fail "LLVM omitted ordinal/ABI load"
        grep -Fq 'getelementptr inbounds i32' "$artifact" || fail "LLVM widened stored Int elements"
        grep -Fq 'sext i32' "$artifact" || fail "LLVM lost signed Int reads"
        command=("$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$WORK_DIR/direct-$backend.exe")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.log" 2>&1 || {
        cat "$WORK_DIR/$backend.compile.log" >&2; fail "$backend artifact compilation failed";
    }
    "$WORK_DIR/direct-$backend.exe" | tr -d '\r' >"$WORK_DIR/direct-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/direct-$backend.run" || fail "$backend direct runtime differed"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 "$PGY" "$SOURCE" "--backend=$backend" \
        --opt=dev -o "$WORK_REL/public-$backend.exe") >"$WORK_DIR/$backend.public.log" 2>&1 || {
        cat "$WORK_DIR/$backend.public.log" >&2; fail "$backend public compilation failed";
    }
    if grep -Fq '[pipeline timing]' "$WORK_DIR/$backend.public.log"; then fail "native fallback"; fi
    "$WORK_DIR/public-$backend.exe" | tr -d '\r' >"$WORK_DIR/public-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/public-$backend.run" || fail "$backend public runtime differed"
done
python "$ROOT_DIR/tests/self_hosted/parity/direct_mir_array_int_readonly_ref_mutations.py" \
    "$WORK_DIR/program.mir.json" "$WORK_DIR"
for mutation in missing-abi layout-id abi-required storage-align element-width cross-family \
        pass-shape resource owner-carriage binding-ordinal writer-readonly \
        negate-child negate-kind negate-binding; do
    for backend in c llvm; do
        output="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$WORK_REL/$mutation.mir.json" \
            -o "$output") >"$WORK_DIR/$mutation.$backend.log" 2>&1; then fail "$backend accepted $mutation"; fi
        [[ ! -e "$ROOT_DIR/$output" ]] || fail "$backend published $mutation"
        grep -Eq '(CODEGEN ERROR|MIR-LOWER ERROR):' "$WORK_DIR/$mutation.$backend.log" || {
            cat "$WORK_DIR/$mutation.$backend.log" >&2; fail "$backend did not diagnose $mutation";
        }
    done
done
source_failures=0
for negative in push set return copyout; do
    for producer in native public-c public-llvm; do
        arguments=(); backend=c
        if [[ "$producer" == native ]]; then arguments+=(--native-pipeline); fi
        if [[ "$producer" == public-llvm ]]; then backend=llvm; fi
        arguments+=(--error-format=json)
        expected_code=PGY_SEM_BORROW_ESCAPE
        if [[ "$negative" == push || "$negative" == set ]]; then expected_code=PGY_SEM_BUILTIN_ARGS_INVALID; fi
        if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE "$PGY" "${arguments[@]}" \
            "$WORK_REL/$negative.pgy" "--backend=$backend" --opt=dev -o "$WORK_REL/$negative.$producer.exe") \
            >"$WORK_DIR/$negative.$producer.log" 2>&1; then
            echo "[$LABEL] $producer accepted readonly $negative" >&2
            source_failures=$((source_failures + 1))
        elif [[ -e "$WORK_DIR/$negative.$producer.exe" || ! -s "$WORK_DIR/$negative.$producer.log" ]]; then
            echo "[$LABEL] $producer failed without a clean diagnostic refusal: $negative" >&2
            source_failures=$((source_failures + 1))
        elif ! grep -Fq "$expected_code" "$WORK_DIR/$negative.$producer.log" ||
            { [[ "$expected_code" == PGY_SEM_BORROW_ESCAPE ]] &&
              ! grep -Fq '"layer":"resource"' "$WORK_DIR/$negative.$producer.log"; }; then
            echo "[$LABEL] $producer rejected $negative for the wrong reason" >&2
            source_failures=$((source_failures + 1))
        fi
    done
done
[[ "$source_failures" == 0 ]] || fail "$source_failures source-boundary controls failed (all results collected)"
echo "[$LABEL] native/C/LLVM/public 11 values, 28 MIR refusals and 12 source refusals: PASS"
