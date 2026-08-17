#!/usr/bin/env bash
# Registry-owned FileExists with a direct-call path, C/LLVM exact parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-direct-mir-scalar-file-exists-direct-call"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_file_exists_direct_call"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_file_exists_direct_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_file_exists_direct_call_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"
grep -Fq 'func DirectMirScalarProgramExprFileExists() -> Int { return 96; }' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_expression_kind_id_owner.pgy" ||
    fail "FileExists expression identity is missing"
grep -Fq '"host-io", "file-exists"' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy" ||
    fail "FileExists does not consume the host-io runtime ABI operation"
grep -Fq 'HostIORuntimeCFileExistsFn()' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy" ||
    fail "FileExists does not consume the host-io runtime ABI row"
grep -Fq 'return pgy_file_exists(path) ? 1 : 0' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_file_exists_materialization_owner.pgy" ||
    fail "C FileExists does not consume the native runtime owner"
grep -Fq '@pgy_file_exists(ptr %path)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_file_exists_materialization_owner.pgy" ||
    fail "LLVM FileExists does not consume the native runtime owner"

mkdir -p "$WORK_DIR"
printf 'present\n' >"$WORK_DIR/present.txt"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"call_target_name":"FileExists"' "$MIR" ||
    fail "producer omitted FileExists identity"
grep -Fq '"call_target_name":"DirectMirFileExistsFixturePath"' "$MIR" ||
    fail "producer omitted nested direct-call identity"
printf 'yes\n' >"$WORK_DIR/expected.run"

runtime_obj="$WORK_DIR/pgy-runtime.o"
"$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" ||
    fail "runtime support did not compile"
for backend in c llvm; do
    extension=c; [[ "$backend" == llvm ]] && extension=ll
    artifact_rel="$WORK_REL/program.$extension"
    artifact="$ROOT_DIR/$artifact_rel"
    binary="$WORK_DIR/program-$backend.exe"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$MIR_REL" -o "$artifact_rel") >"$WORK_DIR/$backend.project.out" \
        2>"$WORK_DIR/$backend.project.err" || fail "$backend projection failed"
    [[ -s "$artifact" ]] || fail "$backend emitted no artifact"
    if [[ "$backend" == c ]]; then
        "$CC" -std=c11 -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -pthread "$artifact" -o "$binary" || fail "C artifact did not compile"
    else
        ! grep -Fq 'define internal void @pgy_as_push' "$artifact" ||
            fail "scalar FileExists materialized unused Array<String> helpers"
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread \
            -o "$binary" || fail "LLVM artifact did not compile"
    fi
    (cd "$ROOT_DIR" && "$binary") | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in file-exists-target-name file-exists-target-syntax path-target-syntax; do
    mutated_rel="$WORK_REL/$mutation.mir.json"
    python "$MUTATIONS" "$MIR" "$mutation" "$ROOT_DIR/$mutated_rel"
    for backend in c llvm; do
        output_rel="$WORK_REL/$mutation.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$mutated_rel" -o "$output_rel") \
            >"$WORK_DIR/$mutation.$backend.out" \
            2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$ROOT_DIR/$output_rel" ]] || fail "$backend published $mutation"
    done
done
echo "[$LABEL] FileExists nested path C/LLVM parity + negatives: PASS"
