#!/usr/bin/env bash
# Rebound scalar formals join the ordered parameter fact, never source locals.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-value-parameter-rebind"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_value_parameter_rebind"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_value_parameter_rebind.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_value_parameter_rebind_mutations.py"
PARAMETERS="$ROOT_DIR/src/self_hosted/compiler/direct_mir_routine_parameter_set_fact_owner.pgy"
INVENTORY="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_local_inventory_owner.pgy"
TYPES="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_value_type_owner.pgy"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'DirectMirRoutineLocalParameterTypeAtName(' "$INVENTORY" ||
    fail "local inventory does not consume the formal identity"
grep -Fq 'parameter_type != type_name' "$TYPES" ||
    fail "working-local type is not joined to the formal type"
grep -Fq 'fact.carriages[ordinal] != "value"' "$PARAMETERS" ||
    fail "parameter owner does not pin value carriage"
mkdir -p "$WORK_DIR"; rm -f "$WORK_DIR"/*
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"name":"Advance"' "$MIR" || fail "producer omitted Advance"
grep -Fq '"source_type":"AST_ASSIGNMENT"' "$MIR" ||
    fail "producer omitted the formal rebind"
printf '7\n4\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >/dev/null 2>"$WORK_DIR/c.compile.err" || fail "C artifact did not compile"
    else
        "$CLANG" -x ir "$artifact" -o "$bin" >/dev/null 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done
python "$MUTATIONS" "$MIR" "$WORK_DIR/orphan.mir.json"
for backend in c llvm; do
    output_rel="$WORK_REL/orphan.$backend"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/orphan.mir.json" -o "$output_rel") \
            >"$WORK_DIR/orphan.$backend.out" 2>"$WORK_DIR/orphan.$backend.err"; then
        fail "$backend accepted a working local without its formal owner"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published orphan output"
done
echo "[$LABEL] value-parameter rebind C/LLVM parity + identity negative: PASS"
