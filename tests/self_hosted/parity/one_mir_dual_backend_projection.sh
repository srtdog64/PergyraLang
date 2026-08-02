#!/usr/bin/env bash
# One admitted graph directly drives C and LLVM; expected output is pinned
# because the public C path is itself installed self-host substitution.
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
source "$ROOT_DIR/tests/self_hosted/parity/driver_rung2_execution_action_gate.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-one-mir-dual-backend"
DRIVER_BUILD="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap}"
DRIVER_BIN="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-$DRIVER_BUILD/driver_seed.exe}"
WORK_DIR="${PGY_SELFHOST_ONE_MIR_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_dual_backend}"
DIRECT_OWNER_REL="${PGY_SELFHOST_ONE_MIR_DIRECT_OWNER:-src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy}"
DIRECT_OWNER="$ROOT_DIR/$DIRECT_OWNER_REL"
DIRECT_ADMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_graph_admission_owner.pgy"; DIRECT_EMISSION_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_emission_owner.pgy"
RUNTIME_ABI_FACT_OWNER="$ROOT_DIR/src/self_hosted/compiler/runtime_call_abi_structured_fact_owner.pgy"
CC="${PGY_SELFHOST_CC:-gcc}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() {
    [[ -f "$1" ]] || fail "missing required file: ${1#"$ROOT_DIR/"}"
}
hash_file() {
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum "$1" | awk '{print $1}'
    elif command -v shasum >/dev/null 2>&1; then
        shasum -a 256 "$1" | awk '{print $1}'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha256 "$1" | awk '{print $NF}'
    else
        fail "no SHA-256 tool is available"
    fi
}
assert_mir_identity() {
    [[ "$(hash_file "$MIR_ARTIFACT")" == "$1" ]] ||
        fail "$CASE backend projection mutated its admitted MIR artifact"
}

assert_direct_owner_ratchet() {
    local term
    require_file "$DIRECT_OWNER"; require_file "$DIRECT_ADMISSION_OWNER"; require_file "$DIRECT_EMISSION_OWNER"; require_file "$RUNTIME_ABI_FACT_OWNER"
    pgy_selfhost_assert_driver_rung2_execution_action "$ROOT_DIR" ||
        fail "direct execution action gate failed"
    for term in '--mir-json-backend=c' '--mir-json-backend=llvm'; do
        grep -Fq -- "$term" \
            "$ROOT_DIR/src/self_hosted/compiler/driver_bootstrap_main.pgy" ||
            fail "bootstrap CLI is missing direct mode: $term"
    done
    for term in \
        '../parser/' '../semantic/' 'EmitMirProgramTree' \
        'AstTreeArtifactFromText' 'SemanticAstArtifactAnalyze' \
        'CompileSourceTo' 'CompileMirJsonToCVerified' \
        'CompileMachineAdmittedMirJsonToCForTargetVerifiedObserved' \
        'GenerateCFromVerifiedSemanticArtifact' \
        '--canonicalize-mir-json' '--canonicalize-oracle-mir-json'; do
        if grep -Fq -- "$term" \
            "$DIRECT_OWNER" "$DIRECT_ADMISSION_OWNER" "$DIRECT_EMISSION_OWNER"; then
            fail "direct MIR backend owner reopened forbidden bridge: $term"
        fi
    done
    for term in 'import "runtime_call_abi_structured_fact_owner.pgy";' \
        'CompilerRuntimeCallAbiFormattedPrintFact()' \
        'CompilerRuntimeCallAbiFlatFactOwnerReady(formatted_print)' 'CompilerAbiLayoutIntCValueType()'; do
        grep -Fq -- "$term" "$DIRECT_OWNER" ||
            fail "direct projection does not consume structured ABI fact: $term"
    done
    grep -Fq -- '.symbol' "$DIRECT_EMISSION_OWNER" || fail "direct emission does not consume the structured ABI symbol"
    for term in 'CompilerRuntimeCallAbiFormattedPrintFact' \
        'CompilerRuntimeCallAbiFlatFactOwnerReady' \
        'missing-direct-backend' '!missing.ok'; do
        grep -Fq -- "$term" "$RUNTIME_ABI_FACT_OWNER" ||
            fail "runtime ABI unknown lookup is not missing: $term"
    done
    ! grep -Fq -- 'Split(' "$RUNTIME_ABI_FACT_OWNER" || fail "structured ABI fact reparses serialized rows"
    ! grep -Eq -- 'snprintf|long long' "$DIRECT_OWNER" "$DIRECT_EMISSION_OWNER" || fail "direct projection hardcodes an ABI spelling"
    ! grep -Fq -- 'BuildMirDocumentFactIndex(' "$DIRECT_ADMISSION_OWNER" || fail "direct admission reindexes the MIR document"
}

select_case() {
    CASE="$1"
    SOURCE="$2"
    MIR_ARTIFACT="$WORK_DIR/$CASE.one.mir.json"
    C_ARTIFACT="$WORK_DIR/$CASE.one.c"
    LLVM_ARTIFACT="$WORK_DIR/$CASE.one.ll"
    C_BIN="$WORK_DIR/$CASE.one.c.exe"
    LLVM_BIN="$WORK_DIR/$CASE.one.llvm.exe"
}

run_projection() {
    local target="$1" output="$2"
    local stdout="$WORK_DIR/$CASE.project-$target.out"
    local stderr="$WORK_DIR/$CASE.project-$target.err"
    local mir_rel output_rel
    mir_rel="$(pgy_selfhost_path_relative_to_root "$MIR_ARTIFACT")"
    output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
    rm -f "$output"
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" \
        "--mir-json-backend=$target" "$mir_rel" "$output_rel" \
        >"$stdout" 2>"$stderr"); then
        cat "$stdout" "$stderr" >&2 || true
        fail "$CASE $target projection rejected the admitted MIR artifact"
    fi
    [[ -s "$output" ]] || fail "$CASE $target projection emitted no artifact"
}

expect_rejected_without_artifact() {
    local fact="$1" input="$2" diagnostic_pattern="$3" target
    local mode_override="${4:-}" mode input_rel output output_rel stdout stderr
    local targets=(c llvm)
    input_rel="$(pgy_selfhost_path_relative_to_root "$input")"
    [[ -z "$mode_override" ]] || targets=(invalid)
    for target in "${targets[@]}"; do
        output="$WORK_DIR/$CASE.negative-$target-$fact.artifact"
        stdout="$WORK_DIR/$CASE.negative-$target-$fact.out"
        stderr="$WORK_DIR/$CASE.negative-$target-$fact.err"
        rm -f "$output" "$stdout" "$stderr"
        output_rel="$(pgy_selfhost_path_relative_to_root "$output")"
        mode="$mode_override"
        [[ -n "$mode" ]] || mode="--mir-json-backend=$target"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "$mode" "$input_rel" "$output_rel" \
            >"$stdout" 2>"$stderr"); then
            fail "$CASE $target accepted mutated $fact"
        fi
        [[ ! -e "$output" ]] ||
            fail "$CASE $target emitted before rejecting $fact"
        grep -Eiq -- "$diagnostic_pattern" "$stdout" "$stderr" || {
            cat "$stdout" "$stderr" >&2 || true
            fail "$CASE $target rejection did not distinguish $fact"
        }
    done
}

make_mutation() {
    local fact="$1" expression="$2" expected="$3"
    local output="$WORK_DIR/$CASE.mutated-$fact.json"
    sed "$expression" "$MIR_ARTIFACT" >"$output"
    grep -Fq -- "$expected" "$output" && ! cmp -s "$MIR_ARTIFACT" "$output" ||
        fail "$CASE could not create $fact falsifier"
    printf '%s\n' "$output"
}

compile_artifacts() {
    local -a c_command=("$CC" -x c -std=c11 "$C_ARTIFACT")
    local clang_bin="${PGY_SELFHOST_CLANG:-}"
    local llc_bin="${PGY_SELFHOST_LLC:-}"
    local object="$WORK_DIR/$CASE.one.llvm.o"
    if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
        c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    c_command+=(-lm -o "$C_BIN")
    "${c_command[@]}" >"$WORK_DIR/$CASE.c.compile.log" 2>&1 || {
        cat "$WORK_DIR/$CASE.c.compile.log" >&2
        fail "$CASE C projection did not compile"
    }
    if [[ -z "$clang_bin" ]] && command -v clang >/dev/null 2>&1; then
        clang_bin="$(command -v clang)"
    fi
    if [[ -n "$clang_bin" ]]; then
        "$clang_bin" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
            >"$WORK_DIR/$CASE.llvm.compile.log" 2>&1 || {
            cat "$WORK_DIR/$CASE.llvm.compile.log" >&2
            fail "$CASE LLVM projection did not compile with clang"
        }
        return
    fi
    if [[ -z "$llc_bin" ]] && command -v llc >/dev/null 2>&1; then
        llc_bin="$(command -v llc)"
    fi
    [[ -n "$llc_bin" ]] || fail "LLVM projection requires clang or llc"
    "$llc_bin" -filetype=obj "$LLVM_ARTIFACT" -o "$object" \
        >"$WORK_DIR/$CASE.llvm-llc.log" 2>&1 ||
        fail "$CASE LLVM projection did not compile with llc"
    "$CC" "$object" -o "$LLVM_BIN" \
        >"$WORK_DIR/$CASE.llvm-link.log" 2>&1 ||
        fail "$CASE LLVM projection runtime link failed"
}

run_positive_case() {
    local expected="$1" source_rel mir_rel mir_digest
    require_file "$SOURCE"
    rm -f "$MIR_ARTIFACT" "$C_ARTIFACT" "$LLVM_ARTIFACT"
    source_rel="$(pgy_selfhost_path_relative_to_root "$SOURCE")"
    mir_rel="$(pgy_selfhost_path_relative_to_root "$MIR_ARTIFACT")"
    # projections below consume this exact path and cannot mutate its digest.
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
        "$source_rel" "$mir_rel" >"$WORK_DIR/$CASE.producer.out" \
        2>"$WORK_DIR/$CASE.producer.err"); then
        cat "$WORK_DIR/$CASE.producer.out" \
            "$WORK_DIR/$CASE.producer.err" >&2 || true
        fail "$CASE Pergyra seed failed to produce MIR"
    fi
    grep -Fq '"schema":"pgy.mir.v1"' "$MIR_ARTIFACT" ||
        fail "$CASE producer output is not pgy.mir.v1"
    mir_digest="$(hash_file "$MIR_ARTIFACT")"
    run_projection c "$C_ARTIFACT"
    assert_mir_identity "$mir_digest"
    run_projection llvm "$LLVM_ARTIFACT"
    assert_mir_identity "$mir_digest"
    compile_artifacts

    printf '%s\n' "$expected" >"$WORK_DIR/$CASE.expected.run"
    (cd "$ROOT_DIR" && "$C_BIN") | pgy_selfhost_normalize_text_artifact \
        >"$WORK_DIR/$CASE.c.run"
    (cd "$ROOT_DIR" && "$LLVM_BIN") | pgy_selfhost_normalize_text_artifact \
        >"$WORK_DIR/$CASE.llvm.run"
    for target in c llvm; do
        cmp -s "$WORK_DIR/$CASE.expected.run" \
            "$WORK_DIR/$CASE.$target.run" ||
            fail "$CASE $target runtime output differs from its pinned result"
    done
    assert_mir_identity "$mir_digest"
    echo "[$LABEL] $CASE positive parity ok (sha256=$mir_digest)"
}

require_file "$DRIVER_BIN"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
assert_direct_owner_ratchet
mkdir -p "$WORK_DIR"

select_case hello "$ROOT_DIR/examples/hello.pgy"
run_positive_case 'Hello, Pergyra!'
grep -Fq '"expr0_graph":{' "$MIR_ARTIFACT" || fail "hello graph missing"
grep -Fq '"kind":"stmt"' "$MIR_ARTIFACT" || fail "hello kind missing"
mutation="$(make_mutation expr0_graph \
    's/"expr0_graph"/"expr0_graph_removed"/g' '"expr0_graph_removed"')"
expect_rejected_without_artifact expr0_graph "$mutation" \
    'expr0_graph|expression graph'
mutation="$(make_mutation instruction_kind \
    's/"kind":"stmt"/"kind":"invalid-one-mir-gate"/g' \
    '"kind":"invalid-one-mir-gate"')"
expect_rejected_without_artifact instruction_kind "$mutation" \
    'instruction[^[:alnum:]]+(kind|identity)|kind[^[:alnum:]]+instruction|invalid instruction'
expect_rejected_without_artifact invalid_target "$MIR_ARTIFACT" \
    'target|backend' "--mir-json-backend=invalid"

select_case let_log "$ROOT_DIR/src/self_hosted/mir_lower/fixture/let_log.pgy"
run_positive_case '42'
mutation="$(make_mutation local_result_identity \
    's/"result":"x\.1"/"result":"x.2"/' '"result":"x.2"')"
expect_rejected_without_artifact local_result_identity "$mutation" \
    'local result identity|result identity|result[^[:alnum:]]+x\.1'
mutation="$(make_mutation graph_use_edge \
    's/"uses":\["x\.1"\]/"uses":["x.2"]/' '"uses":["x.2"]')"
expect_rejected_without_artifact graph_use_edge "$mutation" \
    'graph use edge|use edge|uses[^[:alnum:]]+x\.1'
mutation="$(make_mutation missing_use_fact \
    's/"uses":\["x\.1"\]/"uses_removed":["x.1"]/' \
    '"uses_removed":["x.1"]')"
expect_rejected_without_artifact missing_use_fact "$mutation" \
    'use facts|graph use edge|uses'
mutation="$(make_mutation arithmetic_add_node \
    's/"kind":"add"/"kind":"subtract"/' \
    '"kind":"subtract"')"
expect_rejected_without_artifact arithmetic_add_node "$mutation" \
    'arithmetic add node|add node|operator[^[:alnum:]]+add'
mutation="$(make_mutation tostring_call_target \
    's/"call_target_name":"ToString"/"call_target_name":"NoSuchTarget"/' \
    '"call_target_name":"NoSuchTarget"')"
expect_rejected_without_artifact tostring_call_target "$mutation" \
    'ToString call target|call target[^[:alnum:]]+ToString'

select_case multilet "$ROOT_DIR/src/self_hosted/mir_lower/fixture/multilet.pgy"
run_positive_case $'35\n12'
mutation="$(make_mutation second_local_result_use \
    's/"result":"b\.1"/"result":"c.1"/' '"result":"c.1"')"
expect_rejected_without_artifact second_local_result_use "$mutation" \
    'second local|local result|result.*use|b\.1'
mutation="$(make_mutation multiply_operator \
    's/"kind":"multiply"/"kind":"divide"/' '"kind":"divide"')"
expect_rejected_without_artifact multiply_operator "$mutation" \
    'multiply|multiplication|operator'
mutation="$(make_mutation statement_order \
    's/"id":2,"kind":"stmt"/"id":99,"kind":"stmt"/;s/"id":3,"kind":"stmt"/"id":2,"kind":"stmt"/;s/"id":99,"kind":"stmt"/"id":3,"kind":"stmt"/' \
    '"id":3,"kind":"stmt"')"
expect_rejected_without_artifact statement_order "$mutation" \
    'statement.*order|instruction[^[:alnum:]]+(id|order)|expected.*id'

echo "[$LABEL] hello + let_log + multilet one-MIR dual-backend gate ok"
