#!/usr/bin/env bash
# One admitted MIR identity -> one MIR-bound AIR/CFG plan -> both backends.
# one admitted ifelse CFG drives both backends through one AIR certificate and verified plan
# MIR-bound strict certificate and evidence mutations reject before output
# one target-neutral plan drives both C and LLVM
# typed string line format drives both CFG emitters
# direct CFG plan target mutation rejects before output

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

LABEL="self-host-one-mir-cfg-air-plan"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
DRIVER_BUILD="${PGY_SELFHOST_DRIVER_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/bootstrap_v67_multilet_r3}"
DRIVER_BIN="${PGY_SELFHOST_ONE_MIR_DRIVER_BIN:-$DRIVER_BUILD/driver_seed.exe}"
WORK_DIR="${PGY_SELFHOST_ONE_MIR_CFG_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_cfg_air_plan}"
SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/fixture/ifelse.pgy"
DIRECT_OWNER="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
CFG_OWNER_REL="${PGY_SELFHOST_ONE_MIR_CFG_OWNER:-src/self_hosted/compiler/direct_mir_cfg_plan_owner.pgy}"
CFG_OWNER="$ROOT_DIR/$CFG_OWNER_REL"
AIR_OWNER="$ROOT_DIR/src/self_hosted/air/mir_cfg_certificate_owner.pgy"
SCALAR_GATE="$ROOT_DIR/tests/self_hosted/parity/one_mir_dual_backend_projection.sh"
CC="${PGY_SELFHOST_CC:-gcc}"

MIR_ARTIFACT="$WORK_DIR/ifelse.one.mir.json"
C_ARTIFACT="$WORK_DIR/ifelse.one.c"
LLVM_ARTIFACT="$WORK_DIR/ifelse.one.ll"
C_BIN="$WORK_DIR/ifelse.one.c.exe"
LLVM_BIN="$WORK_DIR/ifelse.one.llvm.exe"
ORACLE_BIN="$WORK_DIR/ifelse.oracle.exe"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
require_file() { [[ -f "$1" ]] || fail "missing required file: ${1#"$ROOT_DIR/"}"; }
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
root_relative() { pgy_selfhost_path_relative_to_root "$1"; }
assert_mir_identity() {
    [[ "$(hash_file "$MIR_ARTIFACT")" == "$1" ]] ||
        fail "$2 mutated the one admitted MIR identity"
}

assert_shared_plan_ratchet() {
    local term
    # CLOSED fallback identities: serialized_air_reparse,
    # unbound_mir_certificate, certificate_fallback_or_drift,
    # post_issue_identity_mutation, backend_mir_or_air_read,
    # backend_specific_cfg_plan, unbound_target_fingerprint, and
    # post_verification_plan_mutation.
    require_file "$DIRECT_OWNER"
    require_file "$CFG_OWNER"
    require_file "$AIR_OWNER"
    for term in DirectMirCfgPlanFromAdmitted DirectMirCfgPlanReady \
        DirectMirCfgPlanMutationRejected; do
        grep -Fq -- "$term" "$CFG_OWNER" ||
            fail "shared CFG/AIR owner is missing $term"
    done
    grep -Fq -- 'DirectMirCfgCertificateReady' "$AIR_OWNER" ||
        fail "MIR-bound AIR owner is missing DirectMirCfgCertificateReady"
    grep -Fq -- 'DirectMirCfgCertificateMutationRejected' "$AIR_OWNER" ||
        fail "MIR-bound AIR owner lacks certificate mutation negatives"
    grep -Fq -- 'import "../air/mir_cfg_certificate_owner.pgy";' "$CFG_OWNER" ||
        fail "shared CFG plan does not import its AIR certificate owner"
    grep -Fq -- 'import "direct_mir_cfg_plan_owner.pgy";' "$DIRECT_OWNER" ||
        fail "direct backend projection does not import the shared CFG plan"
    grep -Fq -- 'DirectMirCfgPlanFromAdmitted' "$DIRECT_OWNER" ||
        fail "direct backends do not consume the shared CFG/AIR plan"
    for term in 'MirProgramRoutineIndex' 'MirRoutineFactIndex'; do
        grep -Fq -- "$term" "$CFG_OWNER" ||
            fail "CFG/AIR plan bypasses typed owner $term"
    done
    for term in '"succ_true"' '"succ_false"' '"expr0"' \
        'air_json' 'AirJson' 'ifelse.pgy'; do
        ! grep -Fq -- "$term" "$CFG_OWNER" "$AIR_OWNER" ||
            fail "CFG/AIR plan reopened raw or fixture-specific input: $term"
    done
    ! grep -Eiq -- '(read|parse).*(air[_ -]?json)|air[_ -]?json.*(read|parse)' \
        "$CFG_OWNER" "$DIRECT_OWNER" ||
        fail "a backend path reparses AIR JSON"
    ! grep -Fq -- 'BuildMirDocumentFactIndex(' "$CFG_OWNER" ||
        fail "CFG plan rebuilds the already admitted MIR document index"
    ! grep -Eq -- 'DirectMirCfg[A-Za-z0-9_]*(C|Llvm|LLVM)(Reader|Read|Plan)' \
        "$CFG_OWNER" || fail "CFG facts gained a backend-specific reader"
}

run_scalar_regression() {
    [[ "${PGY_SELFHOST_CFG_SKIP_SCALAR_GATE:-0}" == 1 ]] && return
    require_file "$SCALAR_GATE"
    PGY_BIN="$PGY" \
    PGY_SELFHOST_ONE_MIR_DRIVER_BIN="$DRIVER_BIN" \
    PGY_SELFHOST_ONE_MIR_BUILD_DIR="$WORK_DIR/scalar-regression" \
        bash "$SCALAR_GATE"
}

produce_one_mir() {
    local source_rel mir_rel
    source_rel="$(root_relative "$SOURCE")"
    mir_rel="$(root_relative "$MIR_ARTIFACT")"
    rm -f "$MIR_ARTIFACT"
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
        "$source_rel" "$mir_rel" >"$WORK_DIR/producer.out" \
        2>"$WORK_DIR/producer.err"); then
        cat "$WORK_DIR/producer.out" "$WORK_DIR/producer.err" >&2 || true
        fail "Pergyra seed failed to produce ifelse MIR"
    fi
    grep -Fq '"schema":"pgy.mir.v1"' "$MIR_ARTIFACT" ||
        fail "producer output is not pgy.mir.v1"
}

project_one_target() {
    local target="$1" output="$2" digest="$3"
    local input_rel output_rel
    input_rel="$(root_relative "$MIR_ARTIFACT")"
    output_rel="$(root_relative "$output")"
    rm -f "$output"
    if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" \
        "--mir-json-backend=$target" "$input_rel" "$output_rel" \
        >"$WORK_DIR/project-$target.out" 2>"$WORK_DIR/project-$target.err"); then
        cat "$WORK_DIR/project-$target.out" \
            "$WORK_DIR/project-$target.err" >&2 || true
        fail "$target rejected admitted ifelse MIR"
    fi
    [[ -s "$output" ]] || fail "$target emitted no artifact"
    assert_mir_identity "$digest" "$target projection"
}

make_mutation() {
    local fact="$1" expression="$2" expected="$3"
    local output="$WORK_DIR/ifelse.mutated-$fact.json"
    sed "$expression" "$MIR_ARTIFACT" >"$output"
    grep -Fq -- "$expected" "$output" &&
        ! cmp -s "$MIR_ARTIFACT" "$output" ||
        fail "could not create $fact falsifier"
    printf '%s\n' "$output"
}

expect_rejected_without_artifact() {
    local fact="$1" input="$2" diagnostic_pattern="$3" target
    local input_rel output output_rel stdout stderr
    input_rel="$(root_relative "$input")"
    for target in c llvm; do
        output="$WORK_DIR/ifelse.negative-$target-$fact.artifact"
        stdout="$WORK_DIR/ifelse.negative-$target-$fact.out"
        stderr="$WORK_DIR/ifelse.negative-$target-$fact.err"
        output_rel="$(root_relative "$output")"
        rm -f "$output" "$stdout" "$stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" \
            "--mir-json-backend=$target" "$input_rel" "$output_rel" \
            >"$stdout" 2>"$stderr"); then
            fail "$target accepted mutated $fact"
        fi
        [[ ! -e "$output" ]] ||
            fail "$target emitted before rejecting $fact"
        grep -Eiq -- "$diagnostic_pattern" "$stdout" "$stderr" || {
            cat "$stdout" "$stderr" >&2 || true
            fail "$target rejection did not distinguish $fact"
        }
    done
}

compile_artifacts() {
    local -a c_command=("$CC" -x c -std=c11 "$C_ARTIFACT")
    local clang_bin="${PGY_SELFHOST_CLANG:-}" llc_bin="${PGY_SELFHOST_LLC:-}"
    local object="$WORK_DIR/ifelse.one.llvm.o"
    if pgy_selfhost_emitted_c_uses_runtime_headers "$C_ARTIFACT"; then
        c_command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    c_command+=(-lm -o "$C_BIN")
    "${c_command[@]}" >"$WORK_DIR/c.compile.log" 2>&1 || {
        cat "$WORK_DIR/c.compile.log" >&2; fail "C projection did not compile";
    }
    [[ -n "$clang_bin" ]] || clang_bin="$(command -v clang 2>/dev/null || true)"
    if [[ -n "$clang_bin" ]]; then
        "$clang_bin" -x ir "$LLVM_ARTIFACT" -o "$LLVM_BIN" \
            >"$WORK_DIR/llvm.compile.log" 2>&1 || {
            cat "$WORK_DIR/llvm.compile.log" >&2
            fail "LLVM projection did not compile with clang"
        }
        return
    fi
    [[ -n "$llc_bin" ]] || llc_bin="$(command -v llc 2>/dev/null || true)"
    [[ -n "$llc_bin" ]] || fail "LLVM projection requires clang or llc"
    "$llc_bin" -filetype=obj "$LLVM_ARTIFACT" -o "$object" \
        >"$WORK_DIR/llvm-llc.log" 2>&1 || fail "llc rejected projection"
    "$CC" "$object" -o "$LLVM_BIN" >"$WORK_DIR/llvm-link.log" 2>&1 ||
        fail "LLVM projection runtime link failed"
}

run_and_compare() {
    local source_arg oracle_arg target
    source_arg="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
    oracle_arg="$(pgy_path_for_compiler "$PGY" "$ORACLE_BIN")"
    (cd "$ROOT_DIR" && "$PGY" "$source_arg" --backend=c -o "$oracle_arg" \
        >"$WORK_DIR/oracle.compile.log" 2>&1) || fail "native oracle compile failed"
    (cd "$ROOT_DIR" && "$ORACLE_BIN") | pgy_selfhost_normalize_text_artifact \
        >"$WORK_DIR/oracle.run"
    [[ "$(cat "$WORK_DIR/oracle.run")" == pos ]] ||
        fail "ifelse native oracle did not produce pos"
    for target in c llvm; do
        (cd "$ROOT_DIR" && "$WORK_DIR/ifelse.one.$target.exe") |
            pgy_selfhost_normalize_text_artifact >"$WORK_DIR/$target.run"
        pgy_selfhost_compare_expected_text_artifact_file_with_owner \
            "$LABEL:$target-runtime" "$WORK_DIR" "$WORK_DIR/oracle.run" \
            "$WORK_DIR/$target.run" run_output
    done
}

require_file "$PGY"; require_file "$DRIVER_BIN"; require_file "$SOURCE"
pgy_require_runnable_binary_here "$LABEL" "$DRIVER_BIN" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
mkdir -p "$WORK_DIR"
run_scalar_regression
assert_shared_plan_ratchet
produce_one_mir
mir_digest="$(hash_file "$MIR_ARTIFACT")"
project_one_target c "$C_ARTIFACT" "$mir_digest"
project_one_target llvm "$LLVM_ARTIFACT" "$mir_digest"
compile_artifacts
run_and_compare
assert_mir_identity "$mir_digest" "compiled backend executions"

mutation="$(make_mutation missing_successor \
    's/,"succ_false":2/,"succ_false_removed":2/' '"succ_false_removed":2')"
expect_rejected_without_artifact missing_successor "$mutation" \
    'CFG|successor|conditional[^[:alnum:]]+edge'
mutation="$(make_mutation out_of_range_successor \
    's/,"succ_false":2/,"succ_false":9/' '"succ_false":9')"
expect_rejected_without_artifact out_of_range_successor "$mutation" \
    'CFG|successor|out[^[:alnum:]]+of[^[:alnum:]]+range'
mutation="$(make_mutation block_id_order \
    's/{"id":1,"reachable":true/{"id":2,"reachable":true/' \
    '{"id":2,"reachable":true')"
expect_rejected_without_artifact block_id_order "$mutation" \
    'block[^[:alnum:]]+(id|identity|order)|program[^[:alnum:]]+structure|machine[^[:alnum:]]+layer'
mutation="$(make_mutation reachability_fact \
    's/{"id":2,"reachable":true/{"id":2,"reachable":false/' \
    '{"id":2,"reachable":false')"
expect_rejected_without_artifact reachability_fact "$mutation" \
    'CFG.*reachability|reachability.*graph|reachable'
mutation="$(make_mutation branch_graph_use \
    's/"uses":\["x\.1"\]/"uses":["x.2"]/' '"uses":["x.2"]')"
expect_rejected_without_artifact branch_graph_use "$mutation" \
    'branch.*(graph|use)|condition.*(leaf|use)|SSA|use[^[:alnum:]]+edge'

echo "[$LABEL] ifelse one-MIR CFG/AIR-plan dual-backend gate ok (sha256=$mir_digest)"
