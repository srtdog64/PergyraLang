# Shared LLVM-leg assertion for self-host tool parity gates.
#
# The self-host parity claim is that a Pergyra tool compiled by the C backend
# and by the LLVM backend produces byte-identical output when the compiler build
# includes the LLVM backend. C-only CI builds still prove the C leg; they must
# skip the LLVM leg explicitly instead of turning a build configuration into a
# tool parity failure. Running the compiled binaries avoids the compile banner
# that --run interleaves into stdout.
#
# Requires the caller to have defined: ROOT_DIR, PGY, and to have sourced
# tests/pgy_binary_path_helpers.sh (for pgy_path_for_compiler).

pgy_selfhost_log_reports_no_llvm() {
    local log_file="$1"
    [[ -f "$log_file" ]] || return 1
    grep -Eiq \
        'compiled without LLVM backend support|unknown option.*--backend=llvm|LLVM backend (is )?(not enabled|disabled|unavailable|not built)' \
        "$log_file"
}

assert_llvm_leg() {
    local label="$1"
    local tool_arg="$2"
    local build_dir="$3"
    local c_bin="$build_dir/main_c_leg.exe"
    local llvm_bin="$build_dir/main_llvm_leg.exe"
    local c_compile_log="$build_dir/main_c_leg.compile.log"
    local llvm_compile_log="$build_dir/main_llvm_leg.compile.log"
    local c_out
    local llvm_out

    if ! (cd "$ROOT_DIR" && "$PGY" "$tool_arg" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$c_bin")" >"$c_compile_log" 2>&1); then
        echo "[$label] C leg compile failed" >&2
        cat "$c_compile_log" >&2
        exit 1
    fi
    if ! (cd "$ROOT_DIR" && "$PGY" "$tool_arg" --backend=llvm \
        -o "$(pgy_path_for_compiler "$PGY" "$llvm_bin")" >"$llvm_compile_log" 2>&1); then
        if pgy_selfhost_log_reports_no_llvm "$llvm_compile_log"; then
            echo "[$label] llvm-leg skipped (compiler built without LLVM backend support)"
            return 0
        fi
        echo "[$label] LLVM leg compile failed" >&2
        cat "$llvm_compile_log" >&2
        exit 1
    fi

    c_out="$(cd "$ROOT_DIR" && "$c_bin" 2>/dev/null | tr -d '\r')"
    llvm_out="$(cd "$ROOT_DIR" && "$llvm_bin" 2>/dev/null | tr -d '\r')"

    if [[ "$llvm_out" != "$c_out" ]]; then
        echo "[$label] LLVM-compiled tool output diverges from C-compiled tool" >&2
        diff <(printf '%s\n' "$c_out") <(printf '%s\n' "$llvm_out") | head -20 >&2
        exit 1
    fi
    echo "[$label] llvm-leg ok (C-tool==LLVM-tool byte-identical)"
}
