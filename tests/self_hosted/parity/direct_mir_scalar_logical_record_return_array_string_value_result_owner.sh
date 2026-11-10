#!/usr/bin/env bash
# Same logical-record return plus ArrayString copyout C/LLVM boundary.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-record-return-array-string-copyout"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_record_return_array_string_copyout"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_logical_record_return_array_string_value_result.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_logical_record_return_array_string_value_result_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
    cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
    fail "MIR production failed"
}
grep -Fq '"name":"AdvancePresealRows"' "$MIR" || fail "producer omitted callable"
grep -Fq '"type":"Array<String>","carriage":"value-result"' "$MIR" || fail "producer omitted ArrayString copyout"
printf 'record-return-array-copyout-ready\n0\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || {
        cat "$WORK_DIR/$backend.project.out" "$WORK_DIR/$backend.project.err" >&2
        fail "$backend projection failed"
    }
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        signature="$(grep -E '^static pgy_scalar_logical_record_value_[0-9]+ pgy_scalar_routine_[0-9]+\(' "$artifact" | grep -F 'pgy_param_0_mutref' | head -n 1)"
        [[ "$signature" =~ pgy_as[[:space:]]+\*pgy_param_0_mutref ]] || fail "C omitted ArrayString copyout ordinal 0"
        [[ "$signature" =~ pgy_scalar_logical_record_value_[0-9]+[[:space:]]+pgy_param_1 ]] || fail "C omitted record value ordinal 1"
        [[ "$signature" == *'const char* pgy_param_2'* ]] || fail "C omitted String ordinal 2"
        grep -Fq 'pgy_as pgy_param_0 = *pgy_param_0_mutref;' "$artifact" || fail "C omitted copy-in"
        [[ "$(grep -Fc '*pgy_param_0_mutref = pgy_param_0;' "$artifact")" -ge 2 ]] || fail "C omitted early/final copyout"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        grep -Eq 'define internal %pgy\.scalar\.logical\.record\.value\.[0-9]+ @pgy\.scalar\.routine\.[0-9]+\(ptr %pgy\.param\.0\.mutref, %pgy\.scalar\.logical\.record\.value\.[0-9]+ %pgy\.param\.1, ptr %pgy\.param\.2\)' "$artifact" || fail "LLVM omitted exact signature"
        python - "$artifact" <<'PY'
import pathlib, re, sys
text = pathlib.Path(sys.argv[1]).read_text(encoding="utf-8")
aliases = dict(re.findall(r"^(%[^ ]+) = type (.+)$", text, re.M))
def layout(value):
    return aliases.get(value.strip(), value.strip())
allocation = re.search(r"%pgy\.param\.0\.local = alloca (.+?)(?:, align \d+)?$", text, re.M)
copyin = re.search(r"%pgy\.param\.0\.copyin = load (.+?), ptr %pgy\.param\.0\.mutref", text)
store = re.search(r"store (.+?) %pgy\.param\.0\.copyin, ptr %pgy\.param\.0\.local", text)
if not allocation or not copyin or not store or any(
    layout(match.group(1)) != "{ ptr, i64, i64, ptr }"
    for match in (allocation, copyin, store)
):
    raise SystemExit("LLVM ArrayString copy-in lost its admitted storage layout or local transfer")
PY
        [[ "$(grep -Fc '%pgy.param.0.copyout.' "$artifact")" -ge 4 ]] || fail "LLVM omitted early/final copyout"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

# Owner-handle/direct is admitted by the shared logical-record parameter
# owner. Preserve it as a positive control while rejecting inconsistent loans.
owned_rel="$WORK_REL/record-owner.mir.json"
python "$MUTATIONS" "$MIR" record-owner "$ROOT_DIR/$owned_rel"
for backend in c llvm; do
    artifact_rel="$WORK_REL/record-owner.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/record-owner-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$owned_rel" -o "$artifact_rel") \
        >"$WORK_DIR/record-owner.$backend.out" 2>"$WORK_DIR/record-owner.$backend.err" ||
        fail "$backend refused admitted owner-handle record carriage"
    if [[ "$backend" == c ]]; then
        "$CC" -x c -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
            "$artifact" -lm -o "$bin" >"$WORK_DIR/record-owner.c.compile" 2>&1 ||
            fail "C owner-handle artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o" -pthread -lm \
            -o "$bin" >"$WORK_DIR/record-owner.llvm.compile" 2>&1 ||
            fail "LLVM owner-handle artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/record-owner.$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/record-owner.$backend.run" ||
        fail "$backend owner-handle runtime output drifted"
done

for mutation in copyout-type copyout-pass copyout-abi copyout-layout \
    record-type record-carriage string-type return-type parameter-count; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then fail "$backend accepted $mutation"; fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] record return + ArrayString copyout C/LLVM parity + negatives: PASS"
