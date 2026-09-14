#!/usr/bin/env bash
# A local Array<Int> indexed set consumes its exact predecessor and typed RHS.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-local-array-int-indexed-direct-call"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"; CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_local_array_int_indexed_direct_call"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_local_array_int_indexed_direct_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"; MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_local_array_int_indexed_direct_call_mutations.py"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
[[ -f "$MUTATIONS" ]] || fail "missing mutation owner"
mkdir -p "$WORK_DIR"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE_REL" -o "$MIR_REL") \
    >"$WORK_DIR/producer.out" 2>"$WORK_DIR/producer.err" || fail "MIR production failed"
python - "$MIR" <<'PY' || fail "producer local indexed-assignment fact drifted"
import json, sys
d=json.load(open(sys.argv[1], encoding="utf-8"))
r=next(x for x in d["routines"] if x["name"]=="ApplyEffectMask")
rows=[x for b in r["blocks"] for x in b["instructions"] if x.get("source_type")=="AST_ASSIGNMENT"]
assert len(rows)==2 and r["params"]==[]
x,b=rows
assert x["result"].startswith("known_effects.") and x["arg0"]=="known_effects" and x["arg1"]=="local"
assert x["abi_type_name"]=="Array<Int>"
assert x["uses"]==["known_effects.1","callable.1","instances.1","context.1"]
t=x["expr1_graph"]; assert t["root"]==2 and [n["kind"] for n in t["nodes"]]==["leaf","leaf","index"]
assert t["nodes"][0]["text"]=="known_effects" and t["nodes"][0]["binding_kind"]=="none"
v=x["expr0_graph"]["nodes"]
assert x["expr0_graph"]["root"]==11 and v[1]["call_target_name"]=="JoinEffectMask"
assert v[4]["text"]=="known_effects[callable]" and v[10]["text"]=="instances.used_effects[context]"
assert b["arg0"]=="unknown_effects" and b["abi_type_name"]=="Array<Bool>"
assert b["uses"]==["unknown_effects.1","callable.1"] and b["expr0"]=="true"
PY
printf '6\n' >"$WORK_DIR/expected.run"

for backend in c llvm; do
    extension="$backend"; [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"; artifact="$ROOT_DIR/$artifact_rel"
    bin="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$MIR_REL" -o "$artifact_rel") \
        >"$WORK_DIR/$backend.project.out" 2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        [[ "$(grep -Ec 'pgy_ai_set\(&pgy_local_[0-9]+, pgy_set_[0-9]+_index, pgy_set_[0-9]+_value\);' "$artifact")" -eq 1 ]] || fail "C local set drifted"
        [[ "$(grep -Ec 'pgy_ab_set\(&pgy_local_[0-9]+, pgy_set_[0-9]+_index, pgy_set_[0-9]+_value\);' "$artifact")" -eq 1 ]] || fail "C local Bool set drifted"
        grep -Eq 'pgy_set_[0-9]+_value = .*pgy_scalar_routine_[0-9]+\(pgy_ai_get' "$artifact" || fail "C typed RHS call drifted"
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread); fi
        command+=(-lm -o "$bin"); "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" || fail "C compile failed"
    else
        [[ "$(grep -Ec 'call void @pgy_ai_set\(ptr %pgy\.local\.[0-9]+, i64 .*i64 ' "$artifact")" -eq 1 ]] || fail "LLVM local set drifted"
        [[ "$(grep -Ec 'call void @pgy_ab_set\(ptr %pgy\.local\.[0-9]+, i64 .*i1 ' "$artifact")" -eq 1 ]] || fail "LLVM local Bool set drifted"
        grep -Eq 'call i64 @pgy\.scalar\.routine\.[0-9]+\(i64 .*i64 ' "$artifact" || fail "LLVM typed RHS call drifted"
        runtime_obj="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" || fail "runtime ABI compile failed"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin" >"$WORK_DIR/llvm.compile.out" 2>"$WORK_DIR/llvm.compile.err" || fail "LLVM compile failed"
    fi
    "$bin" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" || fail "$backend runtime output drifted"
done

for mutation in missing-predecessor wrong-predecessor repeated-predecessor target-owner \
        target-binding result-owner array-type rhs-call-target \
        bool-missing-predecessor bool-rhs-type; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"; rm -f "$ROOT_DIR/$output_rel"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" "$mutated_rel" -o "$output_rel") >"$WORK_DIR/$mutation.$backend.out" 2>"$WORK_DIR/$mutation.$backend.err"; then fail "$backend accepted $mutation"; fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] local indexed predecessor + C/LLVM parity/negatives: PASS"
