#!/usr/bin/env bash
# One enum declaration fact drives readonly-ref ABI on C and LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-scalar-enum-readonly-ref"
DRIVER="$(pgy_select_optional_exe_binary \
    "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE="tests/self_hosted/fixtures/direct_mir_scalar_enum_readonly_ref.pgy"
ESCAPE="tests/self_hosted/fixtures/direct_mir_scalar_payload_enum_readonly_ref_escape.pgy"
MUTATOR="$ROOT_DIR/tests/self_hosted/parity/direct_mir/direct_mir_scalar_enum_readonly_ref_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing ${1#"$ROOT_DIR/"}: $2"; }
for binary in "$DRIVER" "$PGY"; do
    pgy_require_runnable_binary_here "$LABEL" "$binary" || exit 1
done
for command in "$CC" "$CLANG" python3; do
    command -v "$command" >/dev/null 2>&1 || fail "missing tool: $command"
done
[[ -f "$MUTATOR" ]] || fail "missing mutation owner"

mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/enum-readonly-ref.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
printf 'true\nfalse\n' >"$WORK_DIR/expected.run"

(cd "$ROOT_DIR" && "$PGY" --native-pipeline "$SOURCE" --backend=c \
    --opt=dev -o "$WORK_REL/native.exe") >"$WORK_DIR/native.out" \
    2>"$WORK_DIR/native.err" || {
        cat "$WORK_DIR/native.out" "$WORK_DIR/native.err" >&2
        fail "native source compile failed"
    }
"$WORK_DIR/native.exe" | tr -d '\r' >"$WORK_DIR/native.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/native.run" ||
    fail "native runtime output drifted"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR producer rejected enum readonly refs"
    }
for fact in \
        '"type":"EnumRefObservation","carriage":"readonly-ref","resource":"none","pass":"direct"' \
        '"type":"EnumRefMachine","carriage":"readonly-ref","resource":"none","pass":"direct"'; do
    require_text "$MIR" "$fact"
done

"$CLANG" -std=c11 -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
    -I"$ROOT_DIR/src/runtime" -pthread -c \
    "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" ||
    fail "runtime object compile failed"

for backend in c llvm; do
    suffix=c; [[ "$backend" == llvm ]] && suffix=ll
    artifact_rel="$WORK_REL/program.$suffix"
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
        require_text "$artifact" \
            'static bool pgy_scalar_routine_1(int32_t pgy_param_0)'
        require_text "$artifact" \
            'static bool pgy_scalar_routine_2(const PgyScalarPayloadEnum1 *pgy_param_0)'
        require_text "$artifact" \
            'static bool pgy_scalar_routine_3(int32_t pgy_param_0, const PgyScalarPayloadEnum1 *pgy_param_1)'
        require_text "$artifact" 'pgy_scalar_routine_2(pgy_param_1)'
        require_text "$artifact" 'pgy_scalar_routine_3(pgy_local_0, &(pgy_local_2))'
        "$CC" -x c -std=c11 -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" -pthread "$artifact" -lm \
            -o "$executable" >"$WORK_DIR/c.compile.out" \
            2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        require_text "$artifact" \
            'define internal i1 @pgy.scalar.routine.1(i64 %pgy.param.0)'
        require_text "$artifact" \
            'define internal i1 @pgy.scalar.routine.2(ptr %pgy.param.0)'
        require_text "$artifact" \
            'define internal i1 @pgy.scalar.routine.3(i64 %pgy.param.0, ptr %pgy.param.1)'
        require_text "$artifact" \
            'call i1 @pgy.scalar.routine.2(ptr %pgy.param.1)'
        "$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o" \
            -pthread -lm -o "$executable" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$executable" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in payload-free-pass-indirect payload-free-resource \
        payload-free-abi-required payload-free-layout-id \
        payload-enum-pass-indirect payload-enum-resource \
        payload-enum-abi-required payload-enum-layout-id; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python3 "$MUTATOR" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
        grep -Fq 'direct MIR scalar program route rejected' \
            "$WORK_DIR/$mutation.$backend.out" "$WORK_DIR/$mutation.$backend.err" ||
            fail "$backend did not diagnose $mutation"
    done
done

ESCAPE_MIR_REL="$WORK_REL/escape.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$ESCAPE" \
    -o "$ESCAPE_MIR_REL") >"$WORK_DIR/escape.producer.out" \
    2>"$WORK_DIR/escape.producer.err" || fail "escape fixture lost its exact MIR"
for backend in c llvm; do
    output_rel="$WORK_REL/escape.$backend"
    rm -f "$ROOT_DIR/$output_rel"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$ESCAPE_MIR_REL" -o "$output_rel") >"$WORK_DIR/escape.$backend.out" \
        2>"$WORK_DIR/escape.$backend.err"; then
        fail "$backend accepted borrowed payload-enum escape"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published escape"
    grep -Fq 'direct MIR scalar operation expression is invalid' \
        "$WORK_DIR/escape.$backend.out" "$WORK_DIR/escape.$backend.err" ||
        fail "$backend lost borrowed payload-enum escape diagnostic"
done

echo "[$LABEL] native/C/LLVM enum refs, 16 ABI refusals and escape refusal: PASS"
