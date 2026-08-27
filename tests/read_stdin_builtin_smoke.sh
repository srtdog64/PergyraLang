#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() { echo "[read-stdin] FAIL: $*" >&2; exit 1; }

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"

[[ -x "$PGY" ]] || fail "missing compiler binary: $PGY"
pgy_require_runnable_binary_here "read-stdin" "$PGY"

require_text() {
    local file="$1"
    local text="$2"
    grep -Fq "$text" "$ROOT_DIR/$file" ||
        fail "$file lost required ReadStdin substrate text: $text"
}

forbid_text() {
    local file="$1"
    local text="$2"
    if grep -Fq "$text" "$ROOT_DIR/$file"; then
        fail "$file retained forbidden ReadStdin substrate text: $text"
    fi
}

require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "HostIORuntimeCReadStdinFn"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "read(STDIN_FILENO"
require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "if (rd < 0) { free(buf); abort(); }"
require_text "src/runtime/pgy_runtime_lib_io_string_exports.h" "if (result.tag != PGY_RUNTIME_IO_RESULT_OK)"
require_text "src/runtime/pgy_runtime_io_qubit_inline.h" "if (result.tag != PGY_RUNTIME_IO_RESULT_OK)"
require_text "src/runtime/pgy_runtime_io_qubit_inline.h" \
    "_setmode(_fileno(stdout), _O_BINARY)"
require_text "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    "void pgy_print(const char *msg)"
require_text "src/runtime/pgy_runtime_lib_io_string_exports.h" \
    "_setmode(_fileno(stdout), _O_BINARY)"
require_text "src/self_hosted/codegen/runtime_abi/string_runtime_owner.pgy" \
    "_setmode(_fileno(stdout), _O_BINARY)"
require_text "src/codegen/llvm_expr_stdlib_scalar_io_calls.c" \
    '{ "Print", "stdlib io", "pgy_print", 1 }'
forbid_text "src/codegen/llvm_expr_stdlib_scalar_io_calls.c" '"printf"'
require_text "src/self_hosted/codegen/emission/runtime_call_rewrite_owner.pgy" \
    'source_name == "ReadStdin"'
require_text "src/self_hosted/codegen/emission/program_emit.pgy" \
    "HostIORuntimeCFileIOBlock()"
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" "ReadStdin("

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="$(command -v python3)"
else
    PYTHON_BIN="$(command -v python)"
fi
[[ -n "$PYTHON_BIN" ]] || fail "python is required for the no-EOF chunk falsifier"

WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_read_stdin.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat > "$WORK_DIR/read_stdin_ok.pgy" <<'EOF'
func Main() -> Void with caps io_read {
    let first: String = ReadStdin(5);
    let second: String = ReadStdin(6);
    Log(first);
    Log(second);
}
EOF

cat > "$WORK_DIR/read_stdin_bad_type.pgy" <<'EOF'
func Main() -> Void with caps io_read {
    let s: String = ReadStdin("wrong");
    Log(s);
}
EOF

cat > "$WORK_DIR/read_stdin_chunk.pgy" <<'EOF'
func Main() -> Void with caps io_read {
    let chunk: String = ReadStdin(4096);
    Print(Concat("chunk:", Concat(chunk, "\n")));
}
EOF

run_backend() {
    local backend="$1"
    local source_arg
    local out_base
    local out_arg
    local exe
    local compile_log
    local output

    source_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/read_stdin_ok.pgy")"
    out_base="$WORK_DIR/read_stdin_ok_${backend}"
    out_arg="$(pgy_path_for_compiler "$PGY" "$out_base")"
    compile_log="$WORK_DIR/compile_${backend}.log"

    set +e
    (cd "$ROOT_DIR" && "$PGY" "$source_arg" --backend="$backend" -o "$out_arg" >"$compile_log" 2>&1)
    local rc=$?
    set -e
    if [[ "$rc" -ne 0 ]]; then
        if [[ "$backend" == "llvm" ]] && grep -Eqi 'llvm|backend.*unavailable|not enabled' "$compile_log"; then
            echo "[read-stdin] backend=$backend skip (LLVM unavailable)"
            return 0
        fi
        echo "--- compiler output ---" >&2
        cat "$compile_log" >&2
        echo "-----------------------" >&2
        fail "backend=$backend compile failed"
    fi

    exe="$(pgy_select_optional_exe_binary "$out_base")"
    [[ -x "$exe" ]] || fail "backend=$backend did not produce runnable binary: $exe"
    output="$(printf 'hello-world-extra' | "$exe" | tr -d '\r')"
    if [[ "$output" != $'hello\n-world' ]]; then
        fail "backend=$backend expected byte-count stdin output, got: $output"
    fi

    local chunk_source_arg
    local chunk_out_base
    local chunk_out_arg
    local chunk_exe
    chunk_source_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/read_stdin_chunk.pgy")"
    chunk_out_base="$WORK_DIR/read_stdin_chunk_${backend}"
    chunk_out_arg="$(pgy_path_for_compiler "$PGY" "$chunk_out_base")"
    if ! (cd "$ROOT_DIR" && "$PGY" "$chunk_source_arg" --backend="$backend" \
        -o "$chunk_out_arg" >"$WORK_DIR/chunk_compile_${backend}.log" 2>&1); then
        cat "$WORK_DIR/chunk_compile_${backend}.log" >&2
        fail "backend=$backend no-EOF chunk fixture failed to compile"
    fi
    chunk_exe="$(pgy_select_optional_exe_binary "$chunk_out_base")"
    [[ -x "$chunk_exe" ]] || fail "backend=$backend missing no-EOF chunk binary"
    "$PYTHON_BIN" - "$chunk_exe" "$backend" <<'PY'
import queue
import subprocess
import sys
import threading

binary, backend = sys.argv[1:]
process = subprocess.Popen(
    [binary],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
)
lines = queue.Queue()
threading.Thread(
    target=lambda: lines.put(process.stdout.readline()), daemon=True
).start()
process.stdin.write(b"hello")
process.stdin.flush()
try:
    line = lines.get(timeout=3.0)
except queue.Empty:
    process.kill()
    process.wait(timeout=5.0)
    raise SystemExit(
        f"[read-stdin] FAIL: backend={backend} blocked until EOF"
    )
if line != b"chunk:hello\n":
    process.kill()
    process.wait(timeout=5.0)
    raise SystemExit(
        f"[read-stdin] FAIL: backend={backend} wrong chunk {line!r}"
    )
process.stdin.close()
if process.wait(timeout=5.0) != 0:
    raise SystemExit(
        f"[read-stdin] FAIL: backend={backend} chunk process failed"
    )
PY
    echo "[read-stdin] backend=$backend ok"
}

BACKENDS="${PGY_READ_STDIN_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    run_backend "$backend"
done

bad_arg="$(pgy_path_for_compiler "$PGY" "$WORK_DIR/read_stdin_bad_type.pgy")"
set +e
(cd "$ROOT_DIR" && "$PGY" "$bad_arg" --backend=c -o "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/bad")" >"$WORK_DIR/bad.log" 2>&1)
bad_rc=$?
set -e
if [[ "$bad_rc" -eq 0 ]]; then
    fail "ReadStdin accepted a non-Int max byte count"
fi
grep -Eq "Int|ReadStdin|builtin" "$WORK_DIR/bad.log" ||
    fail "ReadStdin bad-type rejection did not name an actionable type/signature"

echo "[read-stdin] ReadStdin builtin/runtime/self-host substrate locked"
