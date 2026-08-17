#!/usr/bin/env bash
# Long joins reuse the common PhiValue identity and predecessor receipt.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-long-phi-value"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_long_phi_value"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_long_phi_value.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_phi_value_mutations.py"
VARIANTS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_long_phi_value_artifact_variants.py"
OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_string_expression_owner.pgy"
TYPED="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_typed_readiness_owner.pgy"
OP="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_op_code_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for command in "$CC" "$CLANG" python; do command -v "$command" >/dev/null ||
    fail "missing command: $command"; done
phi_body="$(sed -n '/func DirectMirScalarCfgPhiValueTypeReady(/,/^}/p' "$OWNER")"
grep -Fq 'CompilerAbiLayoutLongTypeName()' <<<"$phi_body" ||
    fail "common PhiValue type owner omits Long"
grep -Fq 'DirectMirScalarCfgPhiValueTypeReady(' "$TYPED" ||
    fail "typed readiness reclassifies common PhiValue"
grep -Fq 'func DirectMirScalarCfgOpPhiValue() -> Int { return 29; }' "$OP" ||
    fail "common PhiValue operation identity drifted"
! grep -Rq 'DirectMirScalarCfgOpPhiLong' "$ROOT_DIR/src/self_hosted/compiler" ||
    fail "Long-only phi operation was introduced"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" ||
    fail "MIR production failed"
[[ "$(grep -o '"kind":"phi"' "$MIR" | wc -l)" -eq 1 ]] ||
    fail "producer omitted the single Long phi"

runtime_obj="$WORK_DIR/runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" ||
    fail "runtime ABI object did not compile"
printf '29\n' >"$WORK_DIR/expected-true.run"
printf '11\n' >"$WORK_DIR/expected-false.run"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" \
        -o "$artifact_rel") >"$WORK_DIR/$backend.out" \
        2>"$WORK_DIR/$backend.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    for mode in true false; do
        variant="$WORK_DIR/$backend-$mode.$extension"
        python "$VARIANTS" "$artifact" "$backend" "$mode" "$variant"
        bin="$WORK_DIR/$backend-$mode.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 -fwrapv "$variant")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$variant"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$bin")
            "${command[@]}" || fail "$backend-$mode compile failed"
        else
            "$CLANG" -x ir "$variant" -x none "$runtime_obj" -pthread -lm \
                -o "$bin" || fail "$backend-$mode compile failed"
        fi
        "$bin" | tr -d '\r' >"$WORK_DIR/$backend-$mode.run"
        cmp -s "$WORK_DIR/expected-$mode.run" "$WORK_DIR/$backend-$mode.run" ||
            fail "$backend $mode Long phi result drifted"
    done
done

for mutation in wrong-incoming-type non-dominating-incoming missing-incoming; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output="$ROOT_DIR/$WORK_REL/$mutation.$backend"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$WORK_REL/$mutation.$backend") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] common Long PhiValue C/LLVM parity + incoming negatives: PASS"
