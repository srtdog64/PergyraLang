#!/usr/bin/env bash
# Exact fresh-result ownership is invariant under acyclic composition,
# declaration order, and unrelated routines. External MIR is re-admitted.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-owned-string-result-composition"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "$(pgy_path_for_bash_tool \
    "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")")"
export PGY_SELF_DRIVER_BIN="$(pgy_path_for_compiler "$PGY" "$DRIVER")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
STEP_TIMEOUT="${PGY_SELFHOST_PROBE_TIMEOUT:-45}s"
FIXTURES="tests/self_hosted/fixtures"
STEM="direct_mir_owned_string_result_composition"
CASES=(minimal unused_tail reordered long_chain local_alias shared_callee)
MUTATIONS=(wrong-source wrong-target missing-target recursive-target borrowed-branch)

fail() { echo "[$LABEL] $*; evidence: ${WORK_DIR:-not-created}" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
for tool in "$CC" "$CLANG" python timeout mktemp; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing tool: $tool"
done
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned_string_result_composition.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR"/}"
printf 'owned-string-result-composition-ready\n' >"$WORK_DIR/expected.run"
echo "[$LABEL] evidence: $WORK_DIR"

run_checked() {
    local stage="$1"
    shift
    if (cd "$ROOT_DIR" && timeout "$STEP_TIMEOUT" "$@") \
        >"$WORK_DIR/$stage.out" 2>"$WORK_DIR/$stage.err"; then
        return 0
    else
        local status="$?"
        cat "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err" >&2
        fail "$stage failed (exit=$status)"
    fi
}

expect_refusal() {
    local stage="$1" artifact="$2" diagnostic="$3" artifact_policy="$4"
    shift 4
    printf 'preserved:%s\n' "$stage" >"$artifact"
    printf 'preserved:%s\n' "$stage" >"$WORK_DIR/$stage.expected"
    if (cd "$ROOT_DIR" && timeout "$STEP_TIMEOUT" "$@") \
        >"$WORK_DIR/$stage.out" 2>"$WORK_DIR/$stage.err"; then
        fail "$stage accepted a rejected source/artifact"
    else
        local status="$?"
        [[ "$status" != 124 && "$status" != 137 ]] ||
            fail "$stage timed out rather than refusing"
    fi
    case "$artifact_policy" in
        preserve)
            cmp -s "$WORK_DIR/$stage.expected" "$artifact" ||
                fail "$stage replaced the prior MIR/projected artifact"
            ;;
        invalidate)
            [[ ! -e "$artifact" ]] ||
                fail "$stage left the previous public executable behind"
            ;;
        *) fail "$stage has an unknown artifact refusal policy" ;;
    esac
    grep -Fq "$diagnostic" "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err" || {
        cat "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err" >&2
        fail "$stage lost its refusal diagnostic"
    }
}

for case_name in "${CASES[@]}"; do
    run_checked "$case_name.producer" "$DRIVER" --emit-mir-json-verified \
        "$FIXTURES/${STEM}_${case_name}.pgy" \
        -o "$WORK_REL/$case_name.mir.json"
    [[ -s "$WORK_DIR/$case_name.mir.json" ]] || fail "$case_name emitted no MIR"
done

# Check the exact producer/receiver seam before generating falsifiers. This
# mechanical digest counterpart follows mir/expression_graph_digest_owner.pgy.
python - "$WORK_DIR" <<'PY'
import copy
import json
import pathlib
import sys

work = pathlib.Path(sys.argv[1])
cases = ("minimal", "unused_tail", "reordered", "long_chain", "local_alias", "shared_callee")
programs = {case: json.loads((work / (case + ".mir.json")).read_text(
    encoding="utf-8")) for case in cases}

def rows(program):
    return (instruction for routine in program["routines"]
            for block in routine["blocks"] for instruction in block["instructions"])

def push_rows(program):
    return [row for row in rows(program) if
            (row.get("collection_ownership_receipt") or {}).get("kind") ==
            "owned-string-push"]

def push_row(program):
    pushes = push_rows(program)
    if len(pushes) != 1:
        raise SystemExit("owned-result receipt count drifted")
    return pushes[0]

def digest(graph):
    def hash_int(value, field):
        return (value * 131 + field + 2) % 268435456
    def hash_string(value, field):
        value = (value * 131 + len(field)) % 268435456
        for byte in field.encode("utf-8"):
            value = (value * 131 + byte) % 268435456
        return value
    value = hash_int(hash_int(71, graph["root"]), len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = hash_string(value, node[key])
        for key in ("runtime_call_abi_id", "left", "right"):
            field = node[key]
            value = hash_int(value, -1 if field is None else field)
    return 1073741824 + value

for name, program in programs.items():
    pushes = push_rows(program)
    if len(pushes) != (2 if name == "shared_callee" else 1):
        raise SystemExit(name + ": owned-result receipt count drifted")
    owned = next(r for r in program["routines"] if r["name"] == "Owned0")
    target = owned["source_syntax_id"]
    for push in pushes:
        receipt = push["collection_ownership_receipt"]
        calls = [node for node in push["expr0_graph"]["nodes"] if
                 node["call_target_name"] == "Owned0"]
        if target <= 0 or receipt["source_binding_syntax_id"] != target or \
                len(calls) != 1 or calls[0]["call_target_syntax_id"] != target:
            raise SystemExit(name + ": receipt is not the exact callable identity")

base, padded = programs["minimal"], programs["unused_tail"]
for name in ("Owned0", "Owned1", "Main"):
    left = next(r for r in base["routines"] if r["name"] == name)
    right = next(r for r in padded["routines"] if r["name"] == name)
    if left["source_syntax_id"] != right["source_syntax_id"]:
        raise SystemExit("unused tail changed original callable identity")
if push_row(base)["collection_ownership_receipt"] != \
        push_row(padded)["collection_ownership_receipt"]:
    raise SystemExit("unused tail changed original ownership receipt")

for mutation in ("wrong-source", "wrong-target", "missing-target", "recursive-target"):
    program = copy.deepcopy(base)
    push = push_row(program)
    call = next(n for n in push["expr0_graph"]["nodes"] if
                n["call_target_name"] == "Owned0")
    if mutation == "wrong-source":
        push["collection_ownership_receipt"]["source_binding_syntax_id"] = \
            push["collection_ownership_receipt"]["receiver_binding_syntax_id"]
    elif mutation == "wrong-target":
        call["call_target_syntax_id"] = next(r["source_syntax_id"] for r in
                                            program["routines"] if r["name"] == "Owned1")
    elif mutation == "missing-target":
        call["call_target_syntax_id"] = 0
    else:
        routine = next(r for r in program["routines"] if r["name"] == "Owned0")
        returned = next(i for b in routine["blocks"] for i in b["instructions"]
                        if i["kind"] == "return")
        graph = returned["expr0_graph"]
        if digest(graph) != graph["digest"]:
            raise SystemExit("fixture graph digest counterpart drifted")
        returned["expr0"] = "Owned0()"
        for node in graph["nodes"]:
            if node["kind"] == "leaf":
                node["text"] = "Owned0"
                node["binding_syntax_id"] = routine["source_syntax_id"]
            elif node["kind"] == "call":
                node["text"] = "Owned0()"
                node["call_target_name"] = "Owned0"
                node["call_target_syntax_id"] = routine["source_syntax_id"]
        graph["digest"] = digest(graph)
    (work / (mutation + ".mir.json")).write_text(
        json.dumps(program, separators=(",", ":")), encoding="utf-8")

program = copy.deepcopy(programs["shared_callee"])
routine = next(r for r in program["routines"] if r["name"] == "Owned0")
returns = [i for b in routine["blocks"] for i in b["instructions"]
           if i["kind"] == "return" and i.get("expr0_graph")]
if len(returns) != 2:
    raise SystemExit("shared-callee fixture no longer has two terminal returns")
returned = returns[-1]
returned["expr0"] = '"borrowed"'
returned["source_type"] = "AST_STRING"
graph = {"root": 0, "nodes": [{
    "kind": "string_literal", "text": '"borrowed"',
    "call_target_kind": "none", "call_target_name": "",
    "call_target_syntax_id": 0, "runtime_call_abi_id": 0,
    "binding_syntax_id": 0, "binding_kind": "none", "binding_ordinal": None,
    "left": None, "right": None}]}
graph["digest"] = digest(graph)
returned["expr0_graph"] = graph
(work / "borrowed-branch.mir.json").write_text(
    json.dumps(program, separators=(",", ":")), encoding="utf-8")
PY

BORROWED_SOURCE="$FIXTURES/direct_mir_borrowed_string_call_result_push.pgy"
CYCLE_SOURCE="$FIXTURES/${STEM}_cycle.pgy"
for negative in borrowed cycle; do
    source_path="$BORROWED_SOURCE"
    [[ "$negative" == borrowed ]] || source_path="$CYCLE_SOURCE"
    expect_refusal "$negative.producer" "$WORK_DIR/$negative.mir.json" \
        borrow_boundary_escape preserve "$DRIVER" --emit-mir-json-verified \
        "$source_path" -o "$WORK_REL/$negative.mir.json"
    # Public executable refusal instead invalidates the old binary to prevent
    # stale execution: driver_binary_output_owner.h and binary_output_refusal_owner.sh.
    for backend in c llvm; do
        expect_refusal "$negative.public-$backend" \
            "$WORK_DIR/$negative.public-$backend.exe" borrow_boundary_escape invalidate \
            "$PGY" "$source_path" "--backend=$backend" \
            -o "$WORK_REL/$negative.public-$backend.exe"
    done
done

# A single runtime object is reused by all LLVM projections in this target.
run_checked runtime.compile "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" \
    -I"$ROOT_DIR/src/runtime" -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" \
    -o "$WORK_DIR/runtime.o"

for case_name in "${CASES[@]}"; do
    for backend in c llvm; do
        extension="$backend"
        [[ "$backend" == c ]] || extension=ll
        artifact="$WORK_DIR/$case_name.$extension"
        run_checked "$case_name.$backend.project" "$DRIVER" \
            "--mir-json-backend=$backend" "$WORK_REL/$case_name.mir.json" \
            -o "$WORK_REL/$case_name.$extension"
        [[ -s "$artifact" ]] || fail "$case_name/$backend emitted no artifact"
        binary="$WORK_DIR/$case_name.direct-$backend.exe"
        if [[ "$backend" == c ]]; then
            command=("$CC" -x c -std=c11 "$artifact")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$binary")
            run_checked "$case_name.c.compile" "${command[@]}"
        else
            run_checked "$case_name.llvm.compile" "$CLANG" -x ir "$artifact" \
                -x none "$WORK_DIR/runtime.o" -pthread -lm -o "$binary"
        fi
        run_checked "$case_name.$backend.run" "$binary"
        tr -d '\r' <"$WORK_DIR/$case_name.$backend.run.out" \
            >"$WORK_DIR/$case_name.$backend.run"
        cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$case_name.$backend.run" ||
            fail "$case_name/$backend direct runtime output drifted"

        run_checked "$case_name.$backend.public" "$PGY" \
            "$FIXTURES/${STEM}_${case_name}.pgy" "--backend=$backend" --run \
            -o "$WORK_REL/$case_name.public-$backend.exe"
        tr -d '\r' <"$WORK_DIR/$case_name.$backend.public.out" | sed '/^pgy:/d' \
            >"$WORK_DIR/$case_name.$backend.public.run"
        cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$case_name.$backend.public.run" ||
            fail "$case_name/$backend public runtime output drifted"
    done
done

for mutation in "${MUTATIONS[@]}"; do
    for backend in c llvm; do
        expect_refusal "$mutation.$backend" "$WORK_DIR/$mutation.$backend" \
            'CODEGEN ERROR:' preserve "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$mutation.mir.json" -o "$WORK_REL/$mutation.$backend"
        if [[ "$mutation" == recursive-target || "$mutation" == borrowed-branch ]]; then
            grep -Fq 'program_readiness=26' "$WORK_DIR/$mutation.$backend.out" \
                "$WORK_DIR/$mutation.$backend.err" ||
                fail "$mutation/$backend did not reach the ownership admission refusal"
        fi
    done
done

# The native oracle is not a production fallback or an acceptance prerequisite
# for this Pergyra slice. Keep its independent declaration-order defect visible.
for case_name in minimal reordered; do
    for backend in c llvm; do
        stage="$case_name.$backend.native-observation"
        if (cd "$ROOT_DIR" && timeout "$STEP_TIMEOUT" "$PGY" --native-pipeline \
            "$FIXTURES/${STEM}_${case_name}.pgy" "--backend=$backend" --run \
            -o "$WORK_REL/$stage.exe") >"$WORK_DIR/$stage.out" \
            2>"$WORK_DIR/$stage.err"; then
            tr -d '\r' <"$WORK_DIR/$stage.out" | sed '/^pgy:/d' \
                >"$WORK_DIR/$stage.run"
            if cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$stage.run"; then
                observation="EXECUTED"
            else
                observation="OUTPUT-DRIFT"
            fi
        else
            status="$?"
            observation="REFUSED(exit=$status)"
            if grep -Fq 'stage=invalid-state-transition' \
                "$WORK_DIR/$stage.out" "$WORK_DIR/$stage.err"; then
                observation="KNOWN-DECLARATION-ORDER-DEFECT(exit=$status)"
            fi
        fi
        printf '[%s] native oracle %s/%s: %s (not slice acceptance)\n' \
            "$LABEL" "$case_name" "$backend" "$observation"
    done
done

echo "[$LABEL] exact identity + 6 compositions/direct-public C/LLVM + 12 preserved/4 invalidated refusal artifacts: PASS"
