#!/usr/bin/env bash
# Value-result logical-record Array<Int> assignment consumes the exact prior SSA local.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-value-result-logical-record-array-int-indexed-assignment"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_value_result_logical_record_array_int_indexed_assignment"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_value_result_logical_record_array_int_indexed_assignment.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_value_result_logical_record_array_int_indexed_assignment_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -f "$MUTATIONS" ]] || fail "missing mutation owner"
mkdir -p "$WORK_DIR"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
python - "$MIR" <<'PY' || fail "producer target fact drifted"
import json, sys
d=json.load(open(sys.argv[1], encoding="utf-8"))
r=next(x for x in d["routines"] if x["name"]=="ResolveTarget")
rows=[x for b in r["blocks"] for x in b["instructions"] if x.get("source_type")=="AST_ASSIGNMENT"]
assert len(rows)==2 and r["params"][0]["carriage"]=="value-result"
first, second=rows
assert first["result"]=="analysis.2" and first["uses"]==["first_id.1"]
assert second["result"]=="analysis.5"
assert second["uses"]==["analysis.2","node_id.1","target.1"]
n=second["expr1_graph"]["nodes"]
assert second["expr1_graph"]["root"]==8 and n[0]["binding_kind"]=="formal_parameter"
assert n[0]["binding_ordinal"]==0 and n[5]["text"]=="call_target_kinds"
assert n[8]["left"]==6 and n[8]["right"]==7
PY
printf '8\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"; [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Ec 'pgy_ai_set\(&\(\(\(pgy_local_[0-9]+\)\.field_0\)\.field_0\)\.field_0,' "$artifact")" -eq 2 ]] ||
            fail "C artifact did not write both nested Array<Int> slots"
        grep -Eq '\*pgy_param_0_mutref = pgy_local_[0-9]+;' "$artifact" ||
            fail "C artifact omitted value-result copy-out"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$bin")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C artifact did not compile"
    else
        [[ "$(grep -Ec '\.address = getelementptr inbounds %pgy\.scalar\.logical\.record\.value\.' "$artifact")" -eq 6 ]] ||
            fail "LLVM artifact did not consume both three-member paths"
        [[ "$(grep -Ec 'call void @pgy_ai_set\(ptr %pgy\.expr\.[0-9]+\.[0-9]+\.address, i64 ' "$artifact")" -eq 2 ]] ||
            fail "LLVM artifact did not write both nested Array<Int> slots"
        grep -Eq 'store %pgy\.scalar\.logical\.record\.value\.[0-9]+ .*ptr %pgy\.param\.0\.mutref' "$artifact" ||
            fail "LLVM artifact omitted value-result copy-out"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
            >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
            fail "runtime ABI object did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" \
            >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" ||
            fail "LLVM artifact did not compile"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in formal-binding formal-ordinal carriage missing-predecessor \
        wrong-predecessor member-identity rhs-type result-owner; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$mutated_rel" -o "$output_rel") \
                >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] typed value-result predecessor + C/LLVM parity/negatives: PASS"
