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

require_text "src/self_hosted/codegen/runtime_abi/host_io_runtime_owner.pgy" "HostIORuntimeCReadStdinFn"
require_text "src/self_hosted/codegen/emission/expr_rewrite.pgy" "ReadStdin("
require_text "src/self_hosted/codegen/emission/program_emit.pgy" "pgy_readstdin"
require_text "src/self_hosted/codegen/type_facts/type_env.pgy" "ReadStdin("

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
