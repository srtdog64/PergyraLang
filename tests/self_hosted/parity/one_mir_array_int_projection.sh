#!/usr/bin/env bash
# One admitted local Array<Int> graph drives runtime-free C and LLVM exactly
# once per target. This is a sealed literal/reassignment slice, not general
# runtime-bearing Array support.
set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 ||
    ! command -v tr >/dev/null 2>&1 ||
    ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-array-int"
DRIVER_BIN="${PGY_SELFHOST_ARRAY_INT_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER_BIN="$(pgy_select_optional_exe_binary "$DRIVER_BIN")"
WORK_DIR="${PGY_SELFHOST_ARRAY_INT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_array_int}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/array_literal_assignment.pgy"
MIR="$WORK_DIR/array_int.one.mir.json"
C_ARTIFACT="$WORK_DIR/array_int.one.c"
LLVM_ARTIFACT="$WORK_DIR/array_int.one.ll"
C_BIN="$WORK_DIR/array_int.c.exe"
LLVM_BIN="$WORK_DIR/array_int.llvm.exe"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
PYTHON_BIN="${PYTHON_BIN:-$(command -v python3 || command -v python || true)}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing file: ${1#"$ROOT_DIR/"}"; }
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    else
        fail "no SHA-256 tool is available"
    fi
}

assert_owner_ratchet() {
    local plan="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_plan_owner.pgy"
    local graph="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_graph_fact_owner.pgy"
    local abi="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_abi_projection_owner.pgy"
    local emission="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_emission_owner.pgy"
    local backend="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
    local owner cap lines term
    for owner in "$plan" "$graph" "$abi" "$emission" "$backend"; do
        require_file "$owner"
    done
    while IFS='|' read -r owner cap; do
        lines="$(wc -l < "$owner")"
        [[ "$lines" -le "$cap" ]] ||
            fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
    done <<EOF
$plan|450
$graph|220
$abi|160
$emission|320
$backend|200
EOF
    for term in BuildMirDocumentFactIndex JsonObjectFactTableFromBounds \
        CompileMirJsonToCVerified GenerateCFromVerifiedSemanticArtifact \
        llvm_codegen_ driver_run_pipeline; do
        ! grep -Fq -- "$term" "$plan" "$graph" "$abi" "$emission" ||
            fail "Array<Int> owner reopened a forbidden path: $term"
    done
    for term in 'admitted.routines.instruction_expressions' \
        'admitted.routines.instruction_kinds' \
        'BuildMirRoutineInstructionUseFacts' \
        'MirCapturedRequiredAbiLayoutRowAdmission' \
        'DirectMirArrayIntPlanMutationRejected'; do
        grep -Fq -- "$term" "$plan" ||
            fail "Array<Int> plan does not consume typed owner: $term"
    done
    grep -Fq 'DirectMirArrayIntPlanCandidate(admitted)' "$backend" ||
        fail "one-block dispatch does not classify Array<Int> before scalar"
    grep -Fq 'DirectMirArrayIntAbiProjectionFromPlan' "$backend" ||
        fail "backend does not bind the verified Array<Int> ABI"
    grep -Fq 'DirectMirArrayIntEmitC' "$backend" ||
        fail "backend does not consume the Array<Int> plan for C"
    grep -Fq 'DirectMirArrayIntEmitLlvm' "$backend" ||
        fail "backend does not consume the Array<Int> plan for LLVM"
    ! grep -Fq '@pgy_' "$emission" ||
        fail "runtime-free Array<Int> emission references Pergyra runtime"
}

project() {
    local target="$1" output="$2" stdout stderr
    stdout="$WORK_DIR/project-$target.out"
    stderr="$WORK_DIR/project-$target.err"
    rm -f "$output" "$stdout" "$stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$MIR")" "$(root_relative "$output")" \
        >"$stdout" 2>"$stderr") || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$target rejected admitted Array<Int> MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no Array<Int> artifact"
}

reject_mutation() {
    local name="$1" diagnostic="$2" target output stdout stderr
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"
        stdout="$WORK_DIR/$name.$target.out"
        stderr="$WORK_DIR/$name.$target.err"
        rm -f "$output" "$stdout" "$stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
            "$(root_relative "$WORK_DIR/$name.json")" \
            "$(root_relative "$output")" >"$stdout" 2>"$stderr"); then
            fail "$target accepted Array<Int> mutation: $name"
        fi
        [[ ! -e "$output" ]] || fail "$target emitted before rejecting $name"
        grep -Fq "$diagnostic" "$stdout" "$stderr" || {
            cat "$stdout" "$stderr" >&2 || true
            fail "$target rejection did not distinguish $name"
        }
    done
}

require_file "$SOURCE"
require_file "$DRIVER_BIN"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM IR compiler: $CLANG"
[[ -n "$PYTHON_BIN" ]] || fail "python3/python is required for typed mutations"
assert_owner_ratchet
mkdir -p "$WORK_DIR"

rm -f "$MIR" "$C_ARTIFACT" "$LLVM_ARTIFACT"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected array_literal_assignment.pgy"
mir_digest="$(hash_file "$MIR")"
project c "$C_ARTIFACT"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "C projection mutated MIR"
project llvm "$LLVM_ARTIFACT"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "LLVM projection mutated MIR"
! grep -Fq '@pgy_' "$LLVM_ARTIFACT" || fail "direct LLVM reopened Pergyra runtime"

c_command=("$CC" -x c -std=c11 "$C_ARTIFACT")
if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
    c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
fi
c_command+=(-o "$C_BIN")
"${c_command[@]}" >"$WORK_DIR/c.compile.log" 2>&1 || fail "C artifact did not compile"
"$CLANG" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
    >"$WORK_DIR/llvm.compile.log" 2>&1 || fail "LLVM artifact did not compile"
(cd "$ROOT_DIR" && "$C_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/c.run"
(cd "$ROOT_DIR" && "$LLVM_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/llvm.run"
printf '3\n10\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" || fail "C output drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" || fail "LLVM output drifted"

"$PYTHON_BIN" - "$MIR" "$WORK_DIR" <<'PY'
import copy, json, pathlib, sys
source, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))
def emit(name, mutate):
    doc = copy.deepcopy(base)
    instructions = doc["routines"][0]["blocks"][0]["instructions"]
    mutate(instructions)
    (out / f"{name}.json").write_text(
        json.dumps(doc, separators=(",", ":")), encoding="utf-8")
emit("element-kind", lambda i: i[1]["expr0_graph"]["nodes"][5].__setitem__("kind", "leaf"))
emit("index-kind", lambda i: i[3]["expr0_graph"]["nodes"][6].__setitem__("kind", "leaf"))
emit("length-target", lambda i: i[2]["expr0_graph"]["nodes"][3].__setitem__("call_target_name", "StringLength"))
emit("stale-use", lambda i: i[2].__setitem__("uses", ["nums.1"]))
emit("abi-offset", lambda i: i[0]["abi_layout"]["fields"][1].__setitem__("offset", 0))
emit("source-type", lambda i: i[0].__setitem__("source_type", "AST_ASSIGNMENT"))
emit("unsupported-index", lambda i: i[3]["expr0_graph"]["nodes"][6].__setitem__("text", "3"))
PY

reject_mutation element-kind "direct MIR Array<Int> element or spine fact is invalid"
reject_mutation index-kind "direct MIR Array<Int> index graph shape is invalid"
reject_mutation length-target "direct MIR Array<Int> length/index owner facts are invalid"
reject_mutation stale-use "direct MIR Array<Int> use identity is invalid"
reject_mutation abi-offset "direct MIR Array<Int> ABI admission is invalid"
reject_mutation source-type "direct MIR Array<Int> initial definition identity is invalid"
reject_mutation unsupported-index "direct MIR Array<Int> plan identity is invalid"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "negative gates mutated MIR"
echo "[$LABEL] runtime-free Array<Int> C/LLVM parity and seven negatives are fail-closed (sha256=$mir_digest)"
