#!/usr/bin/env bash
# Zero-parameter scalar signatures remain exact; their calls have a separate owner.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-zero-parameter-callable"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_zero_parameter_callable"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_zero_parameter_callable.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"

SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_signature_owner.pgy"
ENVELOPE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_route_envelope_owner.pgy"
INVENTORY_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_callable_inventory_owner.pgy"
C_SIGNATURE_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_c_signature_owner.pgy"
DIRECT_CALL_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_direct_call_readiness_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$SIGNATURE_OWNER" "$ENVELOPE_OWNER" "$INVENTORY_OWNER" \
        "$C_SIGNATURE_OWNER" "$DIRECT_CALL_OWNER"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'signature.param_count == 0' "$SIGNATURE_OWNER" ||
    fail "signature owner does not admit the exact zero-parameter case"
grep -Fq 'DirectMirScalarProgramZeroParameterReturnTypeSupportedWithFacts(' \
    "$SIGNATURE_OWNER" ||
    fail "zero-parameter return does not consume the bounded return-type owner"
grep -Fq 'signature.param_count == 0' "$ENVELOPE_OWNER" ||
    fail "route envelope does not preserve zero-parameter signatures"
grep -Fq 'inventory.parameter_counts[row] < 0' "$INVENTORY_OWNER" ||
    fail "callable inventory still rejects zero-parameter rows"
! grep -Fq 'inventory.parameter_counts[row] <= 0' "$INVENTORY_OWNER" ||
    fail "callable inventory restored the positive-count assumption"
grep -Fq 'output = Concat(output, "void")' "$C_SIGNATURE_OWNER" ||
    fail "C zero-parameter signature is not an exact void prototype"
! grep -Fq 'ArrayLength(arguments) < 1' "$DIRECT_CALL_OWNER" ||
    fail "direct-call readiness restored the positive-argument assumption"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
grep -Fq '"name":"Schema"' "$MIR" ||
    fail "producer omitted the zero-parameter callable"
printf 'ok\n' >"$WORK_DIR/expected.run"

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
        grep -Eq 'static const char ?\* pgy_scalar_routine_[0-9]+\(void\)' \
            "$artifact" || fail "C artifact omitted the exact zero-param signature"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "C artifact did not compile"
            }
    else
        grep -Eq 'define internal ptr @pgy\.scalar\.routine\.[0-9]+\(\)' \
            "$artifact" || fail "LLVM artifact omitted the zero-param signature"
        "$CLANG" -x ir "$artifact" -o "$bin" \
            >"$WORK_DIR/$backend.compile.out" \
            2>"$WORK_DIR/$backend.compile.err" || {
                cat "$WORK_DIR/$backend.compile.err" >&2
                fail "LLVM artifact did not compile"
            }
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"

done

echo "[$LABEL] zero-parameter scalar signature C/LLVM parity: PASS"
