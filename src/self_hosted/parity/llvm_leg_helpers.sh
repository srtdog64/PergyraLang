# Shared LLVM-leg assertion for self-host tool parity gates.
#
# The self-host parity claim is that a Pergyra tool compiled by the C backend
# and by the LLVM backend produces byte-identical output. Tool gates that only
# exercise the default (C) backend leave the LLVM compilation of that tool
# ungated. assert_llvm_leg closes that path: it compiles the tool with both
# backends, runs each native binary from the repository root, and requires the
# two stdout streams to be byte-identical. Running the compiled binaries avoids
# the compile banner that --run interleaves into stdout.
#
# Requires the caller to have defined: ROOT_DIR, PGY, and to have sourced
# tests/pgy_binary_path_helpers.sh (for pgy_path_for_compiler).

assert_llvm_leg() {
    local label="$1"
    local tool_arg="$2"
    local build_dir="$3"
    local c_bin="$build_dir/main_c_leg.exe"
    local llvm_bin="$build_dir/main_llvm_leg.exe"
    local c_out
    local llvm_out

    (cd "$ROOT_DIR" && "$PGY" "$tool_arg" --backend=c \
        -o "$(pgy_path_for_compiler "$PGY" "$c_bin")" >/dev/null)
    (cd "$ROOT_DIR" && "$PGY" "$tool_arg" --backend=llvm \
        -o "$(pgy_path_for_compiler "$PGY" "$llvm_bin")" >/dev/null)

    c_out="$(cd "$ROOT_DIR" && "$c_bin" 2>/dev/null | tr -d '\r')"
    llvm_out="$(cd "$ROOT_DIR" && "$llvm_bin" 2>/dev/null | tr -d '\r')"

    if [[ "$llvm_out" != "$c_out" ]]; then
        echo "[$label] LLVM-compiled tool output diverges from C-compiled tool" >&2
        diff <(printf '%s\n' "$c_out") <(printf '%s\n' "$llvm_out") | head -20 >&2
        exit 1
    fi
    echo "[$label] llvm-leg ok (C-tool==LLVM-tool byte-identical)"
}
