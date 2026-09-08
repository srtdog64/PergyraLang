#!/usr/bin/env bash
# Consuming return/strict nested calls and continuations use one sealed flow proof.
# Complete consuming-exit sets and strict record operands own exact caller cleanup.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
pgy_prepend_windows_runtime_paths
LABEL="self-host-owned-array-string-terminal-flow"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
CC="${PGY_SELFHOST_CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
SOURCE=tests/self_hosted/fixtures/direct_mir_owned_array_string_terminal_flow.pgy
fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
mkdir -p "$ROOT_DIR/.tmp/self_hosted"
WORK_DIR="$(mktemp -d "$ROOT_DIR/.tmp/self_hosted/owned-array-terminal-flow.XXXXXX")"
WORK_REL="${WORK_DIR#"$ROOT_DIR/"}"
echo "[$LABEL] evidence: $WORK_REL"
printf 'actual-return\nactual-nested!\n[actual-nary]\nloop:0,loop:1\n\nskipped\nactual-early\nbranch-left\nbranch-right\ncommit-root\nactual-moved-left\nactual-moved-right\nafter:3\nafter:0\n' >"$WORK_DIR/expected.run"
printf 'constructor-two\n[constructor-three]\nprefix\nprefixr0r1\nprefix\nzero:three\none:three\nlast:three\ntarget\nalternate\n' >>"$WORK_DIR/expected.run"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SOURCE" \
    -o "$WORK_REL/program.mir.json") >"$WORK_DIR/producer.log" 2>&1 || {
    cat "$WORK_DIR/producer.log" >&2; fail "verified MIR production failed";
}
for backend in c llvm; do
    artifact="$WORK_DIR/program.$backend"
    (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
        "$WORK_REL/program.mir.json" -o "$WORK_REL/program.$backend") \
        >"$WORK_DIR/$backend.project.log" 2>&1 || {
        cat "$WORK_DIR/$backend.project.log" >&2; fail "$backend projection failed";
    }
    if [[ "$backend" == c ]]; then
        command=("$CC" -x c -std=c11 "$artifact")
        if pgy_selfhost_emitted_c_uses_runtime_headers "$artifact"; then
            command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
        fi
        command+=(-lm -o "$WORK_DIR/direct-c.exe")
    else
        "$CLANG" -DPGY_LLVM_ENABLED -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" \
            -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
            >"$WORK_DIR/runtime.compile.log" 2>&1 || {
            cat "$WORK_DIR/runtime.compile.log" >&2; fail "runtime ABI compilation failed";
        }
        command=("$CLANG" -x ir "$artifact" -x none "$WORK_DIR/runtime.o"
            -pthread -lm -o "$WORK_DIR/direct-llvm.exe")
    fi
    "${command[@]}" >"$WORK_DIR/$backend.compile.log" 2>&1 || {
        cat "$WORK_DIR/$backend.compile.log" >&2; fail "$backend compilation failed";
    }
    "$WORK_DIR/direct-$backend.exe" | tr -d '\r' >"$WORK_DIR/direct-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/direct-$backend.run" ||
        fail "$backend direct runtime output drifted"
    (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_DEBUG_PIPELINE_TIMING=1 \
        "$PGY" "$SOURCE" "--backend=$backend" --opt=dev \
        -o "$WORK_REL/public-$backend.exe") >"$WORK_DIR/$backend.public.log" 2>&1 || {
        cat "$WORK_DIR/$backend.public.log" >&2; fail "$backend public compilation failed";
    }
    if grep -Fq '[pipeline timing]' "$WORK_DIR/$backend.public.log"; then
        fail "$backend public path used the native pipeline"
    fi
    "$WORK_DIR/public-$backend.exe" | tr -d '\r' >"$WORK_DIR/public-$backend.run"
    cmp -s "$WORK_DIR/expected.run" "$WORK_DIR/public-$backend.run" ||
        fail "$backend public runtime output drifted"
done
for name in bypass_exit nested_use duplicate_return_move repeated_loop_move lazy_move \
    successor_use successor_condition missing_exit sequential_exits \
    constructor_use constructor_duplicate; do
    (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "tests/self_hosted/fixtures/direct_mir_owned_array_string_$name.pgy" \
        -o "$WORK_REL/$name.mir.json") >"$WORK_DIR/$name.producer.log" 2>&1 || {
        cat "$WORK_DIR/$name.producer.log" >&2; fail "$name negative MIR production failed";
    }
done
for name in missing-return-graph orphan-return-call duplicate-return-call-operand \
    missing-caller-identity missing-owned-parameter-abi missing-constructor-operand; do
    python "$ROOT_DIR/tests/self_hosted/parity/direct_mir_owned_array_string_terminal_flow_mutations.py" \
        "$WORK_DIR/program.mir.json" "$name" "$WORK_DIR/$name.mir.json"
done
for name in bypass_exit nested_use duplicate_return_move repeated_loop_move lazy_move \
    successor_use successor_condition missing_exit sequential_exits \
    constructor_use constructor_duplicate \
    missing-return-graph orphan-return-call duplicate-return-call-operand \
    missing-caller-identity missing-owned-parameter-abi missing-constructor-operand; do
    for backend in c llvm; do
        output="$WORK_REL/$name.$backend"
        if (cd "$ROOT_DIR" && "$DRIVER" "--mir-json-backend=$backend" \
            "$WORK_REL/$name.mir.json" -o "$output") >"$WORK_DIR/$name.$backend.log" 2>&1; then
            fail "$backend accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$output" ]] || fail "$backend published $name"
        grep -Eq '(CODEGEN|MIR-LOWER) ERROR:' "$WORK_DIR/$name.$backend.log" || {
            cat "$WORK_DIR/$name.$backend.log" >&2; fail "$name/$backend omitted an owned diagnostic";
        }
    done
done
echo "[$LABEL] actual join/CommitRoot + return/nested/loop/branch C/LLVM runtime and negatives: PASS"
