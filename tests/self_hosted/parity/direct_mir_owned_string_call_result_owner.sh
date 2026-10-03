#!/usr/bin/env bash
# A String result is an owned collection element only when its exact callable
# body and exact actual allocator prove a HeapOrNull result domain.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-owned-string-call-result"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_owned_string_call_result"
WORK_DIR="$ROOT_DIR/$WORK_REL"
OWNED_SOURCE="tests/self_hosted/fixtures/direct_mir_owned_string_call_result_push.pgy"
WRAPPER_SOURCE="tests/self_hosted/fixtures/direct_mir_owned_string_wrapper_result_push.pgy"
BORROWED_SOURCE="tests/self_hosted/fixtures/direct_mir_borrowed_string_call_result_push.pgy"
CONDITIONAL_TRANSFER_SOURCE="tests/self_hosted/fixtures/direct_mir_owned_array_string_conditional_return_transfer.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/collection_ownership_receipt_mutations.py"
FACT_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_owned_string_result_fact_owner.pgy"
EXPRESSION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_owned_string_expression_domain_owner.pgy"
TRANSITION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_collection_ownership_statement_transition_owner.pgy"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'SemanticAstOwnedStringResultFactsFromResolvedFacts(' "$FACT_OWNER" ||
    fail "owned String result fact owner is missing"
grep -Fq 'SemanticAstScopedLocalBindingIdentityForGraphLeaf(' "$EXPRESSION_OWNER" ||
    fail "owned String result proof joins locals by spelling"
grep -Fq 'SemanticAstOwnedStringCallTarget(' "$TRANSITION_OWNER" ||
    fail "ArrayPush transition does not consume the owned-result fact"
! grep -Fq 'Slot<' "$FACT_OWNER" "$EXPRESSION_OWNER" "$TRANSITION_OWNER" ||
    fail "ordinary String ownership imported Slot semantics"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$OWNED_SOURCE" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || {
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2
        fail "self-host producer rejected an owned String result"
    }
[[ -s "$MIR" ]] || fail "self-host producer emitted no MIR"

python - "$MIR" <<'PY'
import json
import pathlib
import sys

program = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
main = next(r for r in program["routines"] if r["name"] == "Main")
rows = [i for b in main["blocks"] for i in b["instructions"]]
pushes = [i for i in rows if
          (i.get("collection_ownership_receipt") or {}).get("kind") ==
          "owned-string-push"]
if len(pushes) != 1:
    raise SystemExit("owned String result receipt count drifted")
push = pushes[0]
receipt = push["collection_ownership_receipt"]
source = receipt.get("source_binding_syntax_id", 0)
graph = push.get("expr0_graph") or {}
targets = [n.get("call_target_syntax_id", 0) for n in graph.get("nodes", [])
           if n.get("call_target_name") == "MakeOwnedString"]
if source <= 0 or targets != [source]:
    raise SystemExit("receipt source is not the exact owned callable target")
PY

printf 'owned-string-call-result-ready\n' >"$WORK_DIR/expected.run"
for backend in c llvm; do
    extension="$backend"; [[ "$backend" == llvm ]] && extension="ll"
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend projection emitted no artifact"
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$binary")
        "${command[@]}" >"$WORK_DIR/c.compile.out" 2>"$WORK_DIR/c.compile.err" ||
            fail "C projection did not compile"
    else
        runtime="$WORK_DIR/runtime.o"
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
            -I"$ROOT_DIR/src/runtime" -c \
            "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime" \
            >"$WORK_DIR/runtime.compile.out" 2>"$WORK_DIR/runtime.compile.err" ||
            fail "LLVM runtime ABI did not compile"
        "$CLANG" -x ir "$artifact" -x none "$runtime" -pthread -lm \
            -o "$binary" >"$WORK_DIR/llvm.compile.out" \
            2>"$WORK_DIR/llvm.compile.err" || fail "LLVM projection did not compile"
    fi
    "$binary" | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

printf 'owned-string-wrapper-result-ready\n' >"$WORK_DIR/wrapper.expected.run"
for backend in c llvm; do
    output_rel="$WORK_REL/wrapper-$backend.exe"
    (cd "$ROOT_DIR" && "$PGY" "$WRAPPER_SOURCE" "--backend=$backend" \
        --run -o "$output_rel") >"$WORK_DIR/wrapper-$backend.out" \
        2>"$WORK_DIR/wrapper-$backend.err" || {
            cat "$WORK_DIR/wrapper-$backend.out" \
                "$WORK_DIR/wrapper-$backend.err" >&2
            fail "public $backend rejected a composed owned String result"
        }
    tr -d '\r' <"$WORK_DIR/wrapper-$backend.out" | sed '/^pgy:/d' \
        >"$WORK_DIR/wrapper-$backend.run"
    cmp -s "$WORK_DIR/wrapper.expected.run" \
        "$WORK_DIR/wrapper-$backend.run" ||
        fail "public $backend wrapper runtime output drifted"
done

printf 'preserved:borrowed\n' >"$WORK_DIR/borrowed.mir.json"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$BORROWED_SOURCE" -o "$WORK_REL/borrowed.mir.json") \
    >"$WORK_DIR/borrowed.out" 2>"$WORK_DIR/borrowed.err"; then
    fail "self-host accepted a borrowed String result as owned"
fi
[[ "$(cat "$WORK_DIR/borrowed.mir.json")" == "preserved:borrowed" ]] ||
    fail "semantic refusal replaced the prior MIR artifact"
grep -Fq 'borrow_boundary_escape' "$WORK_DIR/borrowed.out" \
    "$WORK_DIR/borrowed.err" || fail "borrowed result lost its diagnostic"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$CONDITIONAL_TRANSFER_SOURCE" \
    -o "$WORK_REL/conditional-return-transfer.mir.json") \
    >"$WORK_DIR/conditional-return-transfer.out" \
    2>"$WORK_DIR/conditional-return-transfer.err" || {
        cat "$WORK_DIR/conditional-return-transfer.out" \
            "$WORK_DIR/conditional-return-transfer.err" >&2
        fail "a terminating branch leaked its move into the sibling path"
    }

for backend in c llvm; do
    for source in "$OWNED_SOURCE" "$BORROWED_SOURCE"; do
        stem="$(basename "$source" .pgy)-$backend"
        output_rel="$WORK_REL/$stem.exe"
        if [[ "$source" == "$OWNED_SOURCE" ]]; then
            (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$source" \
                "--backend=$backend" --run -o "$output_rel") \
                >"$WORK_DIR/$stem.out" 2>"$WORK_DIR/$stem.err" ||
                fail "native $backend rejected the owned result"
            tr -d '\r' <"$WORK_DIR/$stem.out" | sed '/^pgy:/d' \
                >"$WORK_DIR/$stem.run"
            cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$stem.run" ||
                fail "native $backend runtime output drifted"
        else
            if (cd "$ROOT_DIR" && "$PGY" --native-pipeline "$source" \
                "--backend=$backend" -o "$output_rel") \
                >"$WORK_DIR/$stem.out" 2>"$WORK_DIR/$stem.err"; then
                fail "native $backend accepted the borrowed result"
            fi
            [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
                fail "native $backend published a rejected artifact"
        fi
    done
done

MUTATED="$WORK_DIR/mutations"
python "$MUTATIONS" "$MIR" "$MUTATED"
for backend in c llvm; do
    output_rel="$WORK_REL/wrong-source-binding.$backend"
    if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MUTATED/wrong-source-binding.mir.json" -o "$output_rel") \
        >"$WORK_DIR/wrong-source-$backend.out" \
        2>"$WORK_DIR/wrong-source-$backend.err"; then
        fail "$backend accepted a forged owned-result source"
    fi
    [[ ! -e "$ROOT_DIR/$output_rel" ]] ||
        fail "$backend published a forged-source artifact"
done

echo "[$LABEL] body proof + exact receipt source + C/LLVM parity/negatives: PASS"
