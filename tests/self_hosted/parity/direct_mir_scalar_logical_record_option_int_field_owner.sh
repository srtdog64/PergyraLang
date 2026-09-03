#!/usr/bin/env bash
# ABI-free logical records join the canonical Option<Int> receipt before C/LLVM.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-logical-record-option-int-field"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_logical_record_option_int_field"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_five_routine_unsupported_nominal.pgy"
MIR_REL="$WORK_REL/program.mir.json"

MATERIALIZATION="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_logical_record_option_int_abi_owner.pgy"
GRAPH="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_cfg_program_graph_admission_owner.pgy"
C_RECORD="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_logical_record_owner.pgy"
LLVM_RECORD="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_logical_record_owner.pgy"
ABI_ROWS="$ROOT_DIR/src/self_hosted/compiler/abi_layout_row_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for owner in "$MATERIALIZATION" "$GRAPH" "$C_RECORD" "$LLVM_RECORD" \
    "$ABI_ROWS"; do
    [[ -f "$owner" ]] || fail "missing owner: ${owner#"$ROOT_DIR/"}"
done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'SelfMirOptionAbiValueOffset(' "$MATERIALIZATION" ||
    fail "logical-record Option materialization bypasses the canonical contract"
grep -Fq 'DirectMirOptionMatchAbiFactFromCapture(capture)' "$MATERIALIZATION" ||
    fail "logical-record Option materialization does not issue the shared receipt"
! grep -Eq 'source_json|MirProgramRoutineIndex|fixture|routine_count' \
    "$MATERIALIZATION" || fail "logical-record Option owner rescans a program root"
grep -Fq 'DirectMirScalarProgramLogicalRecordOptionIntAbiFromFact(' "$GRAPH" ||
    fail "GraphPlan does not carry the logical-record Option receipt"
grep -Fq 'DirectMirScalarProgramCOptionIntTypeName()' "$C_RECORD" ||
    fail "C logical record invents its Option field representation"
grep -Fq 'DirectMirScalarProgramLlvmOptionIntTypeName()' "$LLVM_RECORD" ||
    fail "LLVM logical record invents its Option field representation"
grep -Fq 'CompilerAbiLayoutRowMaterializationFor(CompilerAbiLayoutOptionIntTypeName()) == CompilerAbiLayoutFieldAllowedMaterialization()' "$ABI_ROWS" ||
    fail "source-C Option<Int> field admission is missing"

mkdir -p "$WORK_DIR"
rm -f "$WORK_DIR"/*
printf '11\n' >"$WORK_DIR/expected.run"

source_c="$WORK_DIR/source.c"
(cd "$ROOT_DIR" && "$DRIVER" "$SOURCE_REL" --emit-c-verified) \
    >"$source_c" 2>"$WORK_DIR/source.err" || {
        cat "$source_c" "$WORK_DIR/source.err" >&2
        fail "installed source-C entrypoint rejected the Option<Int> field"
    }
grep -Fq 'pgy_option_int optional_count;' "$source_c" ||
    fail "source-C artifact omitted the Option<Int> field"
source_bin="$WORK_DIR/source.exe"
source_command=("$CC" -x c -std=c11 "$source_c")
if pgy_selfhost_emitted_c_uses_runtime_headers "$source_c"; then
    source_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
source_command+=(-lm -o "$source_bin")
"${source_command[@]}" >"$WORK_DIR/source.compile.out" \
    2>"$WORK_DIR/source.compile.err" || fail "source-C artifact did not compile"
"$source_bin" | tr -d '\r' >"$WORK_DIR/source.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/source.run" ||
    fail "source-C runtime output drifted"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" \
    -o "$MIR_REL") >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "MIR production failed"
    }
python - "$ROOT_DIR/$MIR_REL" <<'PY'
import json
import pathlib
import sys
document = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
metadata = next(row for row in document["decls"] if row["name"] == "Metadata")
assert metadata["abi_layout_id"] == 0
assert metadata["abi_layout_required"] is False
assert metadata["abi_layout"] is None
assert [field["type"] for field in metadata["fields"]] == ["Bool", "Option<Int>"]
PY

for backend in c llvm; do
    artifact_rel="$WORK_REL/program.$backend"
    artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.out" \
        2>"$WORK_DIR/$backend.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        grep -Eq 'typedef struct \{ bool field_0; pgy_scalar_option_int field_1; \} pgy_scalar_logical_record_value_[0-9]+;' "$artifact" ||
            fail "C artifact omitted the owner-projected Option field"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
    else
        grep -Eq '%pgy\.scalar\.logical\.record\.value\.[0-9]+ = type \{ i1, %pgy\.scalar\.option\.int \}' "$artifact" ||
            fail "LLVM artifact omitted the owner-projected Option field"
        command=("$CLANG" -x ir "$artifact" -o "$bin")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.out" \
        2>"$WORK_DIR/$backend.compile.err" || fail "$backend artifact did not compile"
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

sed 's/Option<Int>/Option<Long>/' "$ROOT_DIR/$SOURCE_REL" \
    >"$WORK_DIR/option-long.pgy"
negative_rel="$WORK_REL/option-long.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$WORK_REL/option-long.pgy" -o "$negative_rel") \
    >"$WORK_DIR/negative.producer.out" 2>"$WORK_DIR/negative.producer.err" ||
    fail "Option<Long> negative MIR production failed"
for backend in c llvm; do
    output_rel="$WORK_REL/option-long.$backend"
    output="$ROOT_DIR/$output_rel"
    rm -f "$output"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$negative_rel" -o "$output_rel") >"$WORK_DIR/negative.$backend.out" \
        2>"$WORK_DIR/negative.$backend.err"; then
        fail "$backend accepted an unowned Option<Long> record field"
    fi
    [[ ! -e "$output" ]] || fail "$backend published the negative artifact"
    grep -Fq 'owner=callable-route-envelope stage=return-type' \
        "$WORK_DIR/negative.$backend.out" "$WORK_DIR/negative.$backend.err" ||
        fail "$backend discarded the exact rejection boundary"
done

echo "[$LABEL] source-C + direct C/LLVM exact 11; Option<Long> negative: PASS"
