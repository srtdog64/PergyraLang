#!/usr/bin/env bash
# Focused hard-substitution gate: one Pergyra-produced MIR artifact must drive
# both backend projections.  The native compiler is runtime oracle evidence;
# it is never an input to either artifact consumer.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-dual-backend"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER_BUILD="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap}"
DRIVER_BIN="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-$DRIVER_BUILD/driver_seed.exe}"
WORK_DIR="${PGY_SELFHOST_ONE_MIR_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_dual_backend}"
SOURCE="$ROOT_DIR/examples/hello.pgy"
DIRECT_OWNER_REL="${PGY_SELFHOST_ONE_MIR_DIRECT_OWNER:-src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy}"
DIRECT_OWNER="$ROOT_DIR/$DIRECT_OWNER_REL"
CC="${PGY_SELFHOST_CC:-gcc}"

MIR_ARTIFACT="$WORK_DIR/hello.one.mir.json"
C_ARTIFACT="$WORK_DIR/hello.one.c"
LLVM_ARTIFACT="$WORK_DIR/hello.one.ll"
C_BIN="$WORK_DIR/hello.one.c.exe"
LLVM_BIN="$WORK_DIR/hello.one.llvm.exe"
ORACLE_BIN="$WORK_DIR/hello.oracle.exe"

fail() {
    echo "[$LABEL] $*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "missing required file: ${1#"$ROOT_DIR/"}"
}

hash_file() {
    local file="$1"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$file" | awk '{print $1}'
        return
    fi
    if command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$file" | awk '{print $1}'
        return
    fi
    if command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$file" | awk '{print $NF}'
        return
    fi
    fail "no SHA-256 tool is available"
}

assert_mir_identity() {
    local expected="$1"
    local observed
    observed="$(hash_file "$MIR_ARTIFACT")"
    [[ "$observed" == "$expected" ]] ||
        fail "backend projection mutated the admitted MIR artifact"
}

assert_direct_owner_ratchet() {
    local term
    require_file "$DIRECT_OWNER"
    grep -Fq -- '--mir-json-backend=c' \
        "$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy" ||
        fail "bootstrap CLI is missing the direct C MIR projection mode"
    grep -Fq -- '--mir-json-backend=llvm' \
        "$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy" ||
        fail "bootstrap CLI is missing the direct LLVM MIR projection mode"

    # This owner is the only backend-neutral artifact admission/projection
    # seam.  Reopening the current MIR->tree->semantic bridge here would make
    # byte-equal outputs two semantic authorities rather than one MIR SoT.
    for term in \
        '../parser/' \
        '../semantic/' \
        'EmitMirProgramTree' \
        'AstTreeArtifactFromText' \
        'SemanticAstArtifactAnalyze' \
        'CompileSourceTo' \
        'CompileMirJsonToCVerified' \
        'CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved' \
        'GenerateCFromVerifiedSemanticArtifact' \
        '--canonicalize-mir-json' \
        '--canonicalize-oracle-mir-json'; do
        if grep -Fq -- "$term" "$DIRECT_OWNER"; then
            fail "direct MIR backend owner reopened forbidden bridge: $term"
        fi
    done
}

run_projection() {
    local target="$1"
    local output="$2"
    local stdout="$WORK_DIR/project-${target}.out"
    local stderr="$WORK_DIR/project-${target}.err"
    local mir_rel output_rel
    mir_rel="$(pgy_selfhost_path_relative_to_root "$MIR_ARTIFACT")"
    output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
    rm -f "$output"
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" \
        "--mir-json-backend=$target" "$mir_rel" "$output_rel" \
        >"$stdout" 2>"$stderr"); then
        cat "$stdout" "$stderr" >&2 || true
        fail "$target projection rejected the admitted MIR artifact"
    fi
    [[ -s "$output" ]] || fail "$target projection emitted no artifact"
}

expect_rejected_without_artifact() {
    local label="$1"
    local mode="$2"
    local input="$3"
    local output="$4"
    local diagnostic_pattern="$5"
    local stdout="$WORK_DIR/negative-${label}.out"
    local stderr="$WORK_DIR/negative-${label}.err"
    local input_rel output_rel
    input_rel="$(pgy_selfhost_path_relative_to_root "$input")"
    output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
    rm -f "$output" "$stdout" "$stderr"
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" "$mode" "$input_rel" "$output_rel" \
        >"$stdout" 2>"$stderr"); then
        fail "$label mutation was accepted"
    fi
    [[ ! -e "$output" ]] ||
        fail "$label mutation produced an artifact before rejection"
    grep -Eiq -- "$diagnostic_pattern" "$stdout" "$stderr" || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$label rejection did not name its missing/invalid fact"
    }
}

compile_c_artifact() {
    local -a command=("$CC" -x c -std=c11 "$C_ARTIFACT")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=(-lm -o "$C_BIN")
    "${command[@]}" >"$WORK_DIR/c.compile.log" 2>&1 || {
        cat "$WORK_DIR/c.compile.log" >&2
        fail "C projection did not compile"
    }
}

compile_llvm_artifact() {
    local clang_bin="${PGY_SELFHOST_CLANG:-}"
    local llc_bin="${PGY_SELFHOST_LLC:-}"
    local object="$WORK_DIR/hello.one.llvm.o"
    if [[ -z "$clang_bin" ]] && command -v clang >/dev/null 2>&1; then
        clang_bin="$(command -v clang)"
    fi
    if [[ -n "$clang_bin" ]]; then
        "$clang_bin" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
            >"$WORK_DIR/llvm.compile.log" 2>&1 || {
            cat "$WORK_DIR/llvm.compile.log" >&2
            fail "LLVM projection did not compile with clang"
        }
        return
    fi
    if [[ -z "$llc_bin" ]] && command -v llc >/dev/null 2>&1; then
        llc_bin="$(command -v llc)"
    fi
    [[ -n "$llc_bin" ]] || fail "LLVM projection requires clang or llc"
    "$llc_bin" -filetype=obj "$LLVM_ARTIFACT" -o "$object" \
        >"$WORK_DIR/llvm-llc.compile.log" 2>&1 || {
        cat "$WORK_DIR/llvm-llc.compile.log" >&2
        fail "LLVM projection did not compile with llc"
    }
    "$CC" "$object" -o "$LLVM_BIN" \
        >"$WORK_DIR/llvm-link.compile.log" 2>&1 || {
        cat "$WORK_DIR/llvm-link.compile.log" >&2
        fail "LLVM projection runtime link failed"
    }
}

require_file "$SOURCE"
require_file "$PGY"
require_file "$DRIVER_BIN"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
assert_direct_owner_ratchet
mkdir -p "$WORK_DIR"
rm -f "$MIR_ARTIFACT" "$C_ARTIFACT" "$LLVM_ARTIFACT"

# Producer runs exactly once.  Both positive consumers below receive this
# exact path, and its digest is checked after every projection.
source_rel="$(pgy_selfhost_path_relative_to_root "$SOURCE")"
mir_rel="$(pgy_selfhost_path_relative_to_root "$MIR_ARTIFACT")"
if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$source_rel" "$mir_rel" >"$WORK_DIR/mir-producer.out" \
    2>"$WORK_DIR/mir-producer.err"); then
    cat "$WORK_DIR/mir-producer.out" "$WORK_DIR/mir-producer.err" >&2 || true
    fail "Pergyra seed failed to produce MIR"
fi
grep -Fq '"schema":"pgy.mir.v1"' "$MIR_ARTIFACT" ||
    fail "Pergyra seed output is not pgy.mir.v1"
grep -Fq '"expr0_graph":{' "$MIR_ARTIFACT" ||
    fail "hello MIR is missing the expression graph falsifier"
grep -Fq '"kind":"stmt"' "$MIR_ARTIFACT" ||
    fail "hello MIR is missing the instruction-kind falsifier"
mir_digest="$(hash_file "$MIR_ARTIFACT")"

run_projection c "$C_ARTIFACT"
assert_mir_identity "$mir_digest"
run_projection llvm "$LLVM_ARTIFACT"
assert_mir_identity "$mir_digest"

MISSING_GRAPH="$WORK_DIR/hello.missing-expr0-graph.json"
INVALID_KIND="$WORK_DIR/hello.invalid-instruction-kind.json"
sed 's/"expr0_graph"/"expr0_graph_removed"/g' \
    "$MIR_ARTIFACT" >"$MISSING_GRAPH"
sed 's/"kind":"stmt"/"kind":"invalid-one-mir-gate"/g' \
    "$MIR_ARTIFACT" >"$INVALID_KIND"
for target in c llvm; do
    expect_rejected_without_artifact \
        "${target}-missing-expr0-graph" "--mir-json-backend=$target" \
        "$MISSING_GRAPH" "$WORK_DIR/negative-${target}-graph.artifact" \
        'expr0_graph|expression graph'
    expect_rejected_without_artifact \
        "${target}-invalid-instruction-kind" "--mir-json-backend=$target" \
        "$INVALID_KIND" "$WORK_DIR/negative-${target}-kind.artifact" \
        'instruction[^[:alnum:]]+kind|kind[^[:alnum:]]+instruction|invalid instruction'
done
expect_rejected_without_artifact \
    "invalid-target" "--mir-json-backend=invalid" \
    "$MIR_ARTIFACT" "$WORK_DIR/negative-target.artifact" \
    'target|backend'
assert_mir_identity "$mir_digest"

compile_c_artifact
compile_llvm_artifact

source_arg="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
oracle_arg="$(pgy_path_for_compiler "$PGY" "$ORACLE_BIN")"
if ! (cd "$ROOT_DIR" && "$PGY" "$source_arg" --backend=c -o "$oracle_arg" \
    >"$WORK_DIR/oracle.compile.log" 2>&1); then
    cat "$WORK_DIR/oracle.compile.log" >&2
    fail "native C oracle compile failed"
fi

(cd "$ROOT_DIR" && "$ORACLE_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/oracle.run"
(cd "$ROOT_DIR" && "$C_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/c.run"
(cd "$ROOT_DIR" && "$LLVM_BIN") | pgy_selfhost_normalize_text_artifact \
    >"$WORK_DIR/llvm.run"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "$LABEL:c-runtime" "$WORK_DIR" \
    "$WORK_DIR/oracle.run" "$WORK_DIR/c.run" "run_output"
pgy_selfhost_compare_expected_text_artifact_file_with_owner \
    "$LABEL:llvm-runtime" "$WORK_DIR" \
    "$WORK_DIR/oracle.run" "$WORK_DIR/llvm.run" "run_output"

echo "[$LABEL] one Pergyra MIR -> C/LLVM projection parity ok (sha256=$mir_digest)"
