#!/usr/bin/env bash
# Source after projection: this owner executes only the five admitted programs.
[[ "${ROOT_DIR:-}" == /* && "${WORK_DIR:-}" == "$ROOT_DIR"/.tmp/self_hosted/* && -d "$WORK_DIR" ]] || {
    echo '[one-mir-string-array-execution] verified caller context is absent' >&2
    return 1
}
source "$ROOT_DIR/tests/self_hosted/parity/emitted_c_runtime_header_owner.sh"
CC="${CC:-gcc}"
CLANG="${PGY_SELFHOST_CLANG:-clang}"
command -v "$CC" >/dev/null || fail "C compiler is unavailable"
command -v "$CLANG" >/dev/null || fail "clang is unavailable"
command -v timeout >/dev/null || fail "bounded execution tool is unavailable"
pgy_selfhost_select_emitted_c_compile_profile || fail "emitted-C compiler profile is invalid"
"$CLANG" -std=c11 -O0 -DPGY_LLVM_ENABLED "-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" \
    -c "$ROOT_DIR/src/runtime/pgy_runtime_lib.c" -o "$WORK_DIR/runtime.o" \
    >"$WORK_DIR/runtime.compile.log" 2>&1 || fail "runtime object did not compile"

compile_and_run() {
    local stem="$1" expected="$2"
    local -a command=("$CC" -x c -std=c11 "${PGY_SELFHOST_EMITTED_C_COMPILE_FLAGS[@]}" "$WORK_DIR/$stem.c")
    if pgy_selfhost_emitted_c_uses_runtime_headers "$WORK_DIR/$stem.c"; then
        command+=("-I$ROOT_DIR/src" "-I$ROOT_DIR/src/runtime" -pthread)
    fi
    command+=(-lm -o "$WORK_DIR/$stem-c.exe")
    "${command[@]}" >"$WORK_DIR/$stem-c.compile" 2>&1 || fail "$stem C did not compile"
    "$CLANG" -x ir "$WORK_DIR/$stem.ll" -x none "$WORK_DIR/runtime.o" -pthread -lm \
        -o "$WORK_DIR/$stem-llvm.exe" \
        >"$WORK_DIR/$stem-llvm.compile" 2>&1 || fail "$stem LLVM did not compile"
    printf '%b' "$expected" >"$WORK_DIR/$stem.expected"
    for backend in c llvm; do
        (cd "$ROOT_DIR" && timeout 30 "$WORK_DIR/$stem-$backend.exe") | tr -d '\r' >"$WORK_DIR/$stem-$backend.run"
        cmp -s "$WORK_DIR/$stem.expected" "$WORK_DIR/$stem-$backend.run" || fail "$stem $backend output drift"
    done
}
compile_and_run base 'alice\nbob\ncarol\nBOB\n'
compile_and_run graph-values 'ant\nbee\ncat\nBOB\n'
compile_and_run empty-set-value 'alice\nbob\ncarol\n\n'
compile_and_run graph-set-value 'alice\nbob\ncarol\nBobby\n'
compile_and_run set-log-reordered 'alice\nbob\ncarol\nbob\n'
