#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/portable_text_mutation_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:generic-specialization-identity-epoch"
fail() { echo "[$LABEL] $*" >&2; exit 1; }

DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC_BIN="${CC:-cc}"
pgy_require_runnable_binary_here "$LABEL:driver" "$DRIVER" \
    || fail "self driver is not runnable"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

SOURCE="tests/self_hosted/parity/fixture/intent_typed_outcome_execution.pgy"
WORK_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/generic_specialization_identity_epoch}"
mkdir -p "$WORK_DIR"
BASE="$WORK_DIR/base.mir.json"
BAD="$WORK_DIR/bad-ordinal.mir.json"
MISSING="$WORK_DIR/missing-callee-binding.mir.json"; CROSSED="$WORK_DIR/crossed-callee-binding.mir.json"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE") \
    | tr -d '\r' >"$BASE"
grep -Fq '"callable":"Identity"' "$BASE" \
    || fail "mixed intent/generic specialization row is missing"
BASE_ARG="$(pgy_selfhost_path_relative_to_root "$BASE")"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$BASE_ARG") >"$WORK_DIR/program.c"
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$WORK_DIR/program.c" -o "$WORK_DIR/program.exe"
"$WORK_DIR/program.exe" | tr -d '\r' >"$WORK_DIR/program.run"
printf '%s\n' 'accepted=true' 'calls=1' 'rejected=false' 'calls=2' \
    >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/program.run" \
    || fail "mixed intent/generic raw MIR execution drifted"

python - "$BASE" "$MISSING" "$CROSSED" <<'PY'
import copy, json, pathlib, sys
base = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
found = []
for routine_i, routine in enumerate(base.get("routines", [])):
    for block_i, block in enumerate(routine.get("blocks", [])):
        for instruction_i, instruction in enumerate(block.get("instructions", [])):
            graph = instruction.get("expr0_graph")
            if not isinstance(graph, dict):
                continue
            nodes = graph.get("nodes", [])
            for call_i, call in enumerate(nodes):
                if call.get("kind") != "call" or call.get("call_target_name") != "IntentRunAccepted":
                    continue
                callee_i = call.get("left"); callee = nodes[callee_i]; target_id = call.get("call_target_syntax_id")
                assert target_id > 0 and callee.get("binding_syntax_id") == target_id
                assert callee.get("binding_kind") == "declared_callable"
                found.append((routine_i, block_i, instruction_i, callee_i, target_id))
assert len(found) == 1
for path, mode in ((sys.argv[2], "missing"), (sys.argv[3], "crossed")):
    document = copy.deepcopy(base)
    ri, bi, ii, ni, target_id = found[0]
    callee = document["routines"][ri]["blocks"][bi]["instructions"][ii]["expr0_graph"]["nodes"][ni]
    callee["binding_syntax_id"] = 0 if mode == "missing" else target_id + 1
    callee["binding_kind"] = "none" if mode == "missing" else "declared_callable"
    pathlib.Path(path).write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
PY

pgy_replace_first_literal "$BASE" "$BAD" \
    '"source_call_ordinal":0' '"source_call_ordinal":999'
BAD_ARG="$(pgy_selfhost_path_relative_to_root "$BAD")"
set +e
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "$BAD_ARG") \
    >"$WORK_DIR/bad.out" 2>"$WORK_DIR/bad.err"
bad_rc=$?
set -e
[[ "$bad_rc" -ne 0 ]] || fail "invalid generic call ordinal was accepted"
grep -Fq 'generic specialization identity is unknown' \
    "$WORK_DIR/bad.out" "$WORK_DIR/bad.err" \
    || fail "invalid generic call ordinal diagnostic drifted"
if grep -Fq '#include' "$WORK_DIR/bad.out" "$WORK_DIR/bad.err"; then
    fail "invalid generic call ordinal emitted a partial C artifact"
fi

for mutation in missing-callee-binding crossed-callee-binding; do
    input="$WORK_DIR/$mutation.mir.json"
    set +e
    (cd "$ROOT_DIR" && "$DRIVER" --mir-json "$(pgy_selfhost_path_relative_to_root "$input")") \
        >"$WORK_DIR/$mutation.out" 2>"$WORK_DIR/$mutation.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 ]] || fail "$mutation was accepted"
    grep -Eq 'callee_binding=0|MIR instruction expression graph is missing or invalid' \
        "$WORK_DIR/$mutation.out" "$WORK_DIR/$mutation.err" \
        || fail "$mutation lost its owner diagnostic"
    ! grep -Fq '#include' "$WORK_DIR/$mutation.out" "$WORK_DIR/$mutation.err" \
        || fail "$mutation emitted a partial C artifact"
done

echo "[$LABEL] PASS"
