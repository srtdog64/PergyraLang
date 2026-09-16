#!/usr/bin/env bash
# Native and installed self-host admission must reject an affine Future stored
# in an aggregate before MIR/C/LLVM artifact publication. The public launcher
# may relay the self-host receipt, but it may not retry the native pipeline.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
SELF_DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
[[ -x "$PGY" && -x "$SELF_DRIVER" ]] || {
    echo "[future-aggregate-storage] compiler or driver is missing" >&2
    exit 1
}

WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/future-aggregate-storage.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT
fail() { echo "[future-aggregate-storage] $*" >&2; exit 1; }
require_text() { grep -Fq -- "$2" "$1" || fail "missing $2 in ${1#"$ROOT_DIR/"}"; }

negative_cases=(
    negative_future_array_alias
    negative_future_empty_array
    negative_future_option_store
    negative_future_struct_store
    negative_future_aggregate_param
    negative_future_aggregate_return
    negative_future_set_store
    negative_future_enum_payload
)

for case_name in "${negative_cases[@]}"; do
    source_rel="tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    set +e
    (cd "$ROOT_DIR" && "$SELF_DRIVER" \
        --emit-mir-json-diagnostic-verified "$source_rel") \
        >"$WORK_DIR/$case_name.self.out" \
        2>"$WORK_DIR/$case_name.self.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/$case_name.self.err" ]] ||
        fail "self-host did not reject $case_name on its owned stdout channel"
    grep -Fxq 'pgy.selfhost.public-diagnostic.v1' \
        "$WORK_DIR/$case_name.self.out" ||
        fail "self-host $case_name lost its public wire marker"
    for fact in \
        '"code":"PGY_SEM_TASK_LIFECYCLE"' \
        '"cause_ir":"semantic:task:lifecycle"' \
        '"fix_source":"await-task-before-exit"'; do
        require_text "$WORK_DIR/$case_name.self.out" "$fact"
    done
done

# The array-alias case reaches both installed public artifact modes. Compare
# the relayed JSON byte-for-byte with the Pergyra-owned direct receipt body.
tail -n +2 "$WORK_DIR/negative_future_array_alias.self.out" \
    >"$WORK_DIR/expected.json"
for backend in c llvm; do
    artifact="$WORK_DIR/public.$backend"
    args=(tests/cases/structured_spawn_lifecycle/negative_future_array_alias.pgy \
        --backend="$backend" --error-format=json)
    if [[ "$backend" == c ]]; then
        artifact="$artifact.c"
        args+=(--emit-c -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
    else
        artifact="$artifact.ll"
        args+=(--emit-llvm -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
    fi
    set +e
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE \
        PGY_SELF_DRIVER_BIN="$SELF_DRIVER" PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "${args[@]}") >"$WORK_DIR/public.$backend.out" \
        2>"$WORK_DIR/public.$backend.err"
    rc=$?
    set -e
    [[ "$rc" -ne 0 && ! -s "$WORK_DIR/public.$backend.out" ]] ||
        fail "public $backend accepted aggregate Future storage"
    cmp -s "$WORK_DIR/expected.json" "$WORK_DIR/public.$backend.err" ||
        fail "public $backend did not relay the exact self-host receipt"
    [[ ! -e "$artifact" ]] ||
        fail "public $backend published a refused artifact"
    ! grep -Fq '[pipeline timing]' "$WORK_DIR/public.$backend.err" ||
        fail "public $backend retried the native pipeline"
done

# Native admission owns the same semantic identity and must also refuse before
# either target emits an artifact.
for case_name in negative_future_array_alias negative_future_enum_payload; do
    source_rel="tests/cases/structured_spawn_lifecycle/${case_name}.pgy"
    for backend in c llvm; do
        artifact="$WORK_DIR/native.$case_name.$backend"
        args=("$source_rel" --native-pipeline --backend="$backend" \
            --error-format=json)
        if [[ "$backend" == c ]]; then
            artifact="$artifact.c"
            args+=(--emit-c -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
        else
            artifact="$artifact.ll"
            args+=(--emit-llvm -o "$(pgy_path_for_compiler "$PGY" "$artifact")")
        fi
        set +e
        (cd "$ROOT_DIR" && "$PGY" "${args[@]}") \
            >"$WORK_DIR/native.$case_name.$backend.out" \
            2>"$WORK_DIR/native.$case_name.$backend.err"
        rc=$?
        set -e
        [[ "$rc" -ne 0 && ! -s "$WORK_DIR/native.$case_name.$backend.out" ]] ||
            fail "native $backend accepted $case_name"
        [[ ! -e "$artifact" ]] ||
            fail "native $backend published $case_name"
        for fact in \
            '"code":"PGY_SEM_TASK_LIFECYCLE"' \
            '"cause_ir":"semantic:task:lifecycle"' \
            '"fix_source":"await-task-before-exit"'; do
            require_text "$WORK_DIR/native.$case_name.$backend.err" "$fact"
        done
    done
done

# A direct Future binding and an unrelated value array remain admitted.
(cd "$ROOT_DIR" && "$SELF_DRIVER" --emit-mir-json-diagnostic-verified \
    tests/cases/structured_spawn_lifecycle/positive_future_with_value_array.pgy) \
    >"$WORK_DIR/positive.mir" 2>"$WORK_DIR/positive.err" ||
    fail "ordinary value array plus direct Future was over-rejected"
[[ -s "$WORK_DIR/positive.mir" && ! -s "$WORK_DIR/positive.err" ]] ||
    fail "positive control changed its MIR diagnostic channels"
require_text "$WORK_DIR/positive.mir" 'abi-type="Array<Int>"'
require_text "$WORK_DIR/positive.mir" 'abi-type="Future<Int>"'

OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_future_storage_admission_owner.pgy"
PROCESS_OWNER="$ROOT_DIR/src/compiler/self_host_artifact_process_owner.c"
require_text "$OWNER" 'func SemanticAstFutureStorageAdmissionFromFacts('
require_text "$OWNER" 'SemanticFutureAggregateStorageContainsHandle('
! grep -Fq 'negative_future_' "$OWNER" ||
    fail "semantic owner gained fixture-specific spelling"
! grep -Fq 'task_lifecycle_invalid' "$PROCESS_OWNER" ||
    fail "native receipt transport gained Future semantic authority"

echo "[future-aggregate-storage] native/public MIR+C+LLVM refusal, exact receipt, no artifact, and value-array control: PASS"
