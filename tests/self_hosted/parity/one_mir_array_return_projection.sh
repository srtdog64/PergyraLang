#!/usr/bin/env bash
# One source-produced two-routine MIR graph drives runtime-free C and LLVM.
# The caller owns returned Array<Int> backing storage, and routine order is not
# semantic authority.
# Registry forbidden-fallback inventory exercised below:
# first_routine_entrypoint, row_order_authority, backend_mir_read, name_only_callee_without_signature,
# missing_return_void_default, call_target_text_fallback, native_codegen_fallback, callee_stack_pointer_return,
# flattened_call_graph, stale_caller_ssa_use, nonterminal_or_unreachable_straight_line, forged_log_scalar_fact,
# backend_specific_return_plan, unbound_target_fingerprint, single_routine_retry, post_issue_plan_mutation.
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

LABEL="self-host-one-mir-array-return"
DRIVER_BIN="${PGY_SELFHOST_ARRAY_RETURN_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}"
DRIVER_BIN="$(pgy_select_optional_exe_binary "$DRIVER_BIN")"
WORK_DIR="${PGY_SELFHOST_ARRAY_RETURN_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/driver/one_mir_array_return}"
SOURCE="$ROOT_DIR/src/self_hosted/codegen/fixture/array_return_literal.pgy"
MIR="$WORK_DIR/array_return.one.mir.json"
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
    local graph="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_return_graph_fact_owner.pgy"
    local identity="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_return_program_identity_owner.pgy"
    local plan="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_return_plan_owner.pgy"
    local emission="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_return_emission_owner.pgy"
    local multi="$ROOT_DIR/src/self_hosted/compiler/direct_mir_multi_routine_projection_owner.pgy"
    local backend="$ROOT_DIR/src/self_hosted/compiler/direct_mir_backend_projection_owner.pgy"
    local abi_fact="$ROOT_DIR/src/self_hosted/compiler/direct_mir_array_int_abi_fact_owner.pgy"
    local owner cap lines term multi_line first_row_line
    while IFS='|' read -r owner cap; do
        require_file "$owner"
        lines="$(wc -l < "$owner")"
        [[ "$lines" -le "$cap" ]] ||
            fail "owner hard cap exceeded: ${owner#"$ROOT_DIR/"}=$lines/$cap"
    done <<EOF
$graph|120
$identity|240
$plan|380
$emission|240
$multi|80
$backend|200
$abi_fact|60
EOF
    for term in BuildMirDocumentFactIndex CompileMirJsonToCVerified \
        GenerateCFromVerifiedSemanticArtifact llvm_codegen_ \
        driver_run_pipeline; do
        ! grep -Fq -- "$term" "$graph" "$identity" "$plan" "$emission" "$multi" ||
            fail "Array return owner reopened a forbidden path: $term"
    done
    for term in DirectMirArrayReturnProgramIdentityFromAdmitted \
        MirRoutineFactIndexUniqueResultDefinition \
        BuildMirRoutineInstructionUseFacts \
        MirCapturedRequiredAbiLayoutRowAdmission \
        DirectMirArrayReturnPlanMutationRejected \
        caller_owned_fixed_array; do
        grep -Fq -- "$term" "$identity" "$plan" "$emission" ||
            fail "Array return plan does not consume owner fact: $term"
    done
    grep -Fq 'if routine_count > 1 {' "$backend" ||
        fail "multi-routine classification is not first"
    multi_line="$(grep -nF 'if routine_count > 1 {' "$backend" | head -1 | cut -d: -f1)"
    first_row_line="$(grep -nF 'routine_block_counts[0]' "$backend" | head -1 | cut -d: -f1)"
    [[ -n "$multi_line" && -n "$first_row_line" && "$multi_line" -lt "$first_row_line" ]] ||
        fail "root reads routine row zero before multi-routine classification"
    grep -Fq 'CompileAdmittedDirectMirMultiRoutineForTarget' "$backend" ||
        fail "backend does not delegate the multi-routine plan"
    for term in DirectMirLiteralLogPlanFromAdmitted \
        DirectMirScalarBlockProjectionFromAdmitted \
        DirectMirArrayIntPlanFromAdmitted \
        DirectMirOptionMatchCfgPlanFromAdmitted DirectMirCfgPlanFromAdmitted; do
        ! grep -Fq -- "$term" "$multi" ||
            fail "multi-routine projection can retry single-routine owner: $term"
    done
    grep -Fq 'DirectMirArrayIntCapturedAbiReady' "$abi_fact" ||
        fail "canonical Array<Int> ABI fact owner is missing"
    ! grep -Fq '@pgy_' "$emission" ||
        fail "runtime-free Array return emission references Pergyra runtime"
}

project() {
    local input="$1" target="$2" output="$3" stdout stderr
    stdout="$output.stdout"
    stderr="$output.stderr"
    rm -f "$output" "$stdout" "$stderr"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
        "$(root_relative "$input")" -o "$(root_relative "$output")" \
        >"$stdout" 2>"$stderr") || {
        cat "$stdout" "$stderr" >&2 || true
        fail "$target rejected admitted Array return MIR"
    }
    [[ -s "$output" ]] || fail "$target emitted no Array return artifact"
}

reject_mutation() {
    local name="$1" diagnostic="$2" target output stdout stderr
    for target in c llvm; do
        output="$WORK_DIR/$name.$target.artifact"
        stdout="$WORK_DIR/$name.$target.out"
        stderr="$WORK_DIR/$name.$target.err"
        rm -f "$output" "$stdout" "$stderr"
        if (cd "$ROOT_DIR" && "$DRIVER_BIN" "--mir-json-backend=$target" \
            "$(root_relative "$WORK_DIR/$name.json")" -o \
            "$(root_relative "$output")" >"$stdout" 2>"$stderr"); then
            fail "$target accepted Array return mutation: $name"
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

rm -f "$MIR"
(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "$(root_relative "$SOURCE")" -o "$(root_relative "$MIR")") ||
    fail "source-to-MIR producer rejected array_return_literal.pgy"
mir_digest="$(hash_file "$MIR")"

for target in c llvm; do
    project "$MIR" "$target" "$WORK_DIR/baseline.$target"
    [[ "$(hash_file "$MIR")" == "$mir_digest" ]] ||
        fail "$target projection mutated baseline MIR"
done
! grep -Fq '@pgy_' "$WORK_DIR/baseline.llvm" ||
    fail "direct LLVM reopened Pergyra runtime"
grep -Fq 'pgy_direct_array_return_producer' "$WORK_DIR/baseline.c" ||
    fail "C flattened the producer/caller graph"
grep -Fq 'pgy_direct_array_return_producer(int32_t *pgy_return_storage)' \
    "$WORK_DIR/baseline.c" || fail "C producer does not borrow caller storage"
grep -Fq 'int32_t pgy_return_storage[4];' "$WORK_DIR/baseline.c" ||
    fail "C caller does not own returned storage"
grep -Fq 'pgy_direct_array_return_producer(pgy_return_storage)' \
    "$WORK_DIR/baseline.c" || fail "C caller does not pass owned storage"
c_producer_body="$(awk '/pgy_direct_array_return_producer\(/{inside=1} inside{print} inside && /^}/{exit}' "$WORK_DIR/baseline.c")"
! grep -Fq 'int32_t pgy_return_storage[' <<<"$c_producer_body" ||
    fail "C producer recreated callee-local backing storage"
grep -Fq '@pgy.direct.array.return.producer(ptr %pgy.return.data)' \
    "$WORK_DIR/baseline.llvm" || fail "LLVM flattened the producer/caller graph"
producer_body="$(awk '/define .*@pgy.direct.array.return.producer/{inside=1} inside{print} inside && /^}/{exit}' "$WORK_DIR/baseline.llvm")"
! grep -Fq 'alloca' <<<"$producer_body" ||
    fail "LLVM producer returned dead frame storage"
grep -Fq '%pgy.return.storage = alloca' "$WORK_DIR/baseline.llvm" ||
    fail "LLVM caller does not own returned storage"

"$CC" -x c -std=c11 "$WORK_DIR/baseline.c" -o "$WORK_DIR/baseline.c.exe" \
    >"$WORK_DIR/c.compile.log" 2>&1 || fail "C artifact did not compile"
"$CLANG" -x ir "$WORK_DIR/baseline.llvm" -o "$WORK_DIR/baseline.llvm.exe" \
    >"$WORK_DIR/llvm.compile.log" 2>&1 || fail "LLVM artifact did not compile"
(cd "$ROOT_DIR" && "$WORK_DIR/baseline.c.exe") |
    pgy_selfhost_normalize_text_artifact >"$WORK_DIR/c.run"
(cd "$ROOT_DIR" && "$WORK_DIR/baseline.llvm.exe") |
    pgy_selfhost_normalize_text_artifact >"$WORK_DIR/llvm.run"
printf '4\n3\n' >"$WORK_DIR/expected.run"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/c.run" || fail "C output drifted"
cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/llvm.run" || fail "LLVM output drifted"

"$PYTHON_BIN" \
    "$ROOT_DIR/tests/self_hosted/parity/one_mir_array_return_mutations.py" \
    "$MIR" "$WORK_DIR"

for target in c llvm; do
    project "$WORK_DIR/routine-order-swap.json" "$target" \
        "$WORK_DIR/routine-order-swap.$target"
    cmp -s "$WORK_DIR/baseline.$target" "$WORK_DIR/routine-order-swap.$target" ||
        fail "$target artifact depends on routine row order"
done
reject_mutation entrypoint-name "return entrypoint identity is invalid"
reject_mutation call-target "return call target is unresolved"
reject_mutation producer-return-kind "return producer instruction is invalid"
reject_mutation producer-return-type "return call target is unresolved"
reject_mutation missing-producer-return \
    "MIR machine-layer facts are missing or invalid"
reject_mutation duplicate-producer-return \
    "MIR machine-layer facts are missing or invalid"
reject_mutation stale-result-definition "return plan identity is invalid"
reject_mutation stale-use "return caller uses disagree"
reject_mutation abi-offset "return ABI admission is invalid"
reject_mutation abi-field-shape-repaired-id "return ABI layout is unsupported or mutated"
reject_mutation unreachable-main "return routine facts are invalid"
reject_mutation producer-successor "return routine facts are invalid"
reject_mutation forged-log-result "return caller instructions are invalid"
[[ "$(hash_file "$MIR")" == "$mir_digest" ]] || fail "negative gates mutated MIR"
echo "[$LABEL] caller-owned Array<Int> return C/LLVM parity, row permutation, and thirteen negatives are fail-closed (sha256=$mir_digest)"
