#!/usr/bin/env bash
# Registry-owned DirWalk with a direct-call path argument, C/LLVM exact parity.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-dir-walk-direct-call"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
WORK_REL=".tmp/self_hosted/direct_mir_scalar_dir_walk_direct_call"
WORK_DIR="$ROOT_DIR/$WORK_REL"
SOURCE_REL="tests/self_hosted/fixtures/direct_mir_dir_walk_direct_call.pgy"
MIR_REL="$WORK_REL/program.mir.json"
MIR="$ROOT_DIR/$MIR_REL"
MUTATIONS="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_dir_walk_direct_call_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

grep -Fq 'func DirectMirScalarProgramExprDirWalk() -> Int { return 95; }' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_external_runtime_expression_kind_owner.pgy" ||
    fail "DirWalk expression identity is missing"
grep -Fq '"host-io", "dir-walk"' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy" ||
    fail "DirWalk does not consume the host-io runtime ABI operation"
grep -Fq 'HostIORuntimeCDirWalkFn()' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_host_io_runtime_requirement_owner.pgy" ||
    fail "DirWalk does not consume the host-io runtime ABI row"
grep -Fq 'PgyArray_String source = pgy_dir_walk(root)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy" ||
    fail "C DirWalk does not consume the native runtime owner"
# C DirWalk consumes projected ArrayString fields and rejects layout drift.
grep -Fq 'projection.storage.data_field' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy" ||
    fail "C DirWalk does not consume the ArrayString target projection"
! grep -Fq 'pgy_as out = {(const char **)source.data' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_dir_walk_materialization_owner.pgy" ||
    fail "C DirWalk restored its positional ArrayString layout"
grep -Fq 'DirectMirScalarProgramCDirWalkBlock(plan, runtime, projection)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_c_string_collection_materialization_owner.pgy" ||
    fail "C DirWalk projection is not carried from the collection materializer"
grep -Fq '@pgy_dir_walk(ptr %root)' \
    "$ROOT_DIR/src/self_hosted/compiler/direct_mir_scalar_program_llvm_dir_walk_materialization_owner.pgy" ||
    fail "LLVM DirWalk does not consume the native runtime owner"

mkdir -p "$WORK_DIR/tree/sub"
printf 'a\n' >"$WORK_DIR/tree/a.txt"
printf 'b\n' >"$WORK_DIR/tree/sub/b.txt"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
    "$SOURCE_REL" -o "$MIR_REL") >"$WORK_DIR/producer.out" \
    2>"$WORK_DIR/producer.err" || fail "MIR production failed"
grep -Fq '"call_target_name":"DirWalk"' "$MIR" ||
    fail "producer omitted DirWalk identity"
grep -Fq '"call_target_name":"DirectMirDirWalkFixtureDir"' "$MIR" ||
    fail "producer omitted nested direct-call identity"
printf '2\n' >"$WORK_DIR/expected.run"

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
        "$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -o "$binary" ||
            fail "LLVM artifact did not compile"
    fi
    (cd "$ROOT_DIR" && "$binary") | tr -d '\r' >"$WORK_DIR/$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$backend.run" ||
        fail "$backend runtime output drifted"
done

for mutation in dirwalk-target-name dirwalk-target-syntax fixture-target-syntax \
    array-layout-offset; do
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

echo "[$LABEL] DirWalk nested path C/LLVM parity + negatives: PASS"
