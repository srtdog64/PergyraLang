#!/usr/bin/env bash
# Scalar Array<Int>/Array<Bool> index reads are typed once and panic identically.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-direct-mir-scalar-array-index"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/direct-mir-scalar-array-index.XXXXXX")"
WORK_REL=".tmp/self_hosted/${WORK_DIR##*/}"
POSITIVE="tests/self_hosted/fixtures/direct_mir_scalar_array_index.pgy"
OOB="tests/self_hosted/fixtures/direct_mir_scalar_array_index_oob.pgy"
MUTATE="$ROOT_DIR/tests/self_hosted/parity/direct_mir_scalar_array_index_mutations.py"
ABI_MUTATE="$ROOT_DIR/tests/self_hosted/parity/direct_mir_multi_routine_mutations.py"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
for path in "$MUTATE" "$ABI_MUTATE"; do [[ -f "$path" ]] || fail "missing $path"; done
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$CC" >/dev/null 2>&1 || fail "missing C compiler: $CC"
command -v "$CLANG" >/dev/null 2>&1 || fail "missing LLVM compiler: $CLANG"

for name in positive oob; do
    source="$POSITIVE"; [[ "$name" == oob ]] && source="$OOB"
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$source" \
        -o "$WORK_REL/$name.mir.json") >"$WORK_DIR/$name.producer.out" \
        2>"$WORK_DIR/$name.producer.err" || {
            cat "$WORK_DIR/$name.producer.out" "$WORK_DIR/$name.producer.err" >&2
            fail "$name MIR production failed"
        }
done

printf '7\n3\n0\n' >"$WORK_DIR/expected.run"
for name in positive oob; do
    for backend in c llvm; do
        artifact_rel="$WORK_REL/$name.$backend"
        artifact="$ROOT_DIR/$artifact_rel"
        bin="$WORK_DIR/$name-$backend.exe"
        (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$name.mir.json" -o "$artifact_rel") \
            >"$WORK_DIR/$name.$backend.project.out" \
            2>"$WORK_DIR/$name.$backend.project.err" || {
                cat "$WORK_DIR/$name.$backend.project.out" \
                    "$WORK_DIR/$name.$backend.project.err" >&2
                fail "$name $backend projection failed"
            }
        [[ -s "$artifact" ]] || fail "$name $backend emitted no artifact"
        if [[ "$backend" == c ]]; then
            grep -Fq 'pgy_ab_get(' "$artifact" || fail "C omitted Array<Bool> get"
            if [[ "$name" == positive ]]; then
                grep -Fq 'pgy_ai_get(' "$artifact" || fail "C omitted Array<Int> get"
                grep -Fq '.length)' "$artifact" || fail "C omitted scalar ArrayLength"
            fi
            command=("$CC" -x c -std=c11 -fwrapv "$artifact")
            if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
                command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
            fi
            command+=(-lm -o "$bin")
        else
            grep -Fq 'call i1 @pgy_ab_get' "$artifact" || fail "LLVM omitted Array<Bool> get"
            if [[ "$name" == positive ]]; then
                grep -Fq 'call i64 @pgy_ai_get' "$artifact" ||
                    fail "LLVM omitted Array<Int> get"
                grep -Fq 'extractvalue %pgy.array.int' "$artifact" ||
                    fail "LLVM omitted Array<Int> length"
                grep -Fq 'extractvalue %pgy.array.bool' "$artifact" ||
                    fail "LLVM omitted Array<Bool> length"
            fi
            grep -Fq 'declare void @pgy_runtime_panic_out_of_bounds_export(ptr)' "$artifact" ||
                fail "LLVM omitted bounds panic ABI"
            runtime_obj="$WORK_DIR/runtime-$name.o"
            "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
                -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$runtime_obj" \
                >"$WORK_DIR/$name.runtime.compile.out" \
                2>"$WORK_DIR/$name.runtime.compile.err" || {
                    cat "$WORK_DIR/$name.runtime.compile.err" >&2
                    fail "runtime ABI object did not compile"
                }
            command=("$CLANG" -x ir "$artifact" -x none "$runtime_obj" -pthread -lm -o "$bin")
        fi
        "${command[@]}" >"$WORK_DIR/$name.$backend.compile.out" \
            2>"$WORK_DIR/$name.$backend.compile.err" || {
                cat "$WORK_DIR/$name.$backend.compile.err" >&2
                fail "$name $backend artifact did not compile"
            }
        if [[ "$name" == positive ]]; then
            "$bin" | tr -d '\r' >"$WORK_DIR/$name.$backend.run"
            cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/$name.$backend.run" ||
                fail "$backend valid index result drifted"
        else
            if "$bin" >"$WORK_DIR/$name.$backend.run" \
                    2>"$WORK_DIR/$name.$backend.run.err"; then
                fail "$backend accepted an out-of-bounds read"
            fi
            grep -Fq 'class=out-of-bounds reason=array index out of bounds' \
                "$WORK_DIR/$name.$backend.run.err" ||
                fail "$backend out-of-bounds panic identity drifted"
        fi
    done
done

python "$MUTATE" "$WORK_DIR/positive.mir.json" "$WORK_DIR/bad-index-type.mir.json"
python "$ABI_MUTATE" "$WORK_DIR/positive.mir.json" \
    logical-record-array-int-abi-layout "$WORK_DIR/bad-int-abi.mir.json"
python "$ABI_MUTATE" "$WORK_DIR/positive.mir.json" \
    logical-record-nested-array-bool-abi-layout "$WORK_DIR/bad-bool-abi.mir.json"
for mutation in bad-index-type bad-int-abi bad-bool-abi; do
    for backend in c llvm; do
        output="$WORK_DIR/$mutation.$backend"
        rm -f "$output"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
                "$WORK_REL/$mutation.mir.json" -o "$WORK_REL/$mutation.$backend") \
                >"$WORK_DIR/$mutation.$backend.out" \
                2>"$WORK_DIR/$mutation.$backend.err"; then
            fail "$backend accepted $mutation"
        fi
        [[ ! -e "$output" ]] || fail "$backend published $mutation"
    done
done

echo "[$LABEL] scalar array index C/LLVM parity + panic/ABI negatives: PASS"
