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

pgy_selfhost_path_relative_to_root() {
    local path="$1"
    local rel="${path#"$ROOT_DIR"/}"

    if [[ "$rel" == "$path" ]]; then
        echo "[self-host-parity] comparator artifact path escapes repo root: $path" >&2
        exit 1
    fi
    case "$rel" in
        /*|[A-Za-z]:*|*\\*)
            echo "[self-host-parity] comparator artifact path must be repo-relative: $path" >&2
            exit 1
            ;;
    esac

    printf '%s\n' "$rel"
}

pgy_selfhost_normalize_text_artifact() {
    tr -d '\r' | awk 'NR > 1 { printf "\n" } { printf "%s", $0 }'
}

pgy_selfhost_backend_output_comparator_bin() {
    local build_dir="$1"
    printf '%s\n' "$build_dir/backend_output_comparator_$$.exe"
}

pgy_selfhost_compile_backend_output_comparator() {
    local label="$1"
    local build_dir="$2"
    local comparator_source="$ROOT_DIR/src/self_hosted/tools/backend_output_comparator/main.pgy"
    local comparator_bin
    local compile_log

    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$build_dir")"
    mkdir -p "$build_dir"

    compile_log="$build_dir/backend_output_comparator_$$.compile.log"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$comparator_source")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$comparator_bin")" \
        >"$compile_log" 2>&1); then
        echo "[$label] backend output comparator failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
}

assert_llvm_leg_with_artifact_owner() {
    local label="$1"
    local build_dir="$2"
    local c_out="$3"
    local llvm_out="$4"
    local comparator_bin
    local cmp_out="$build_dir/main_llvm_leg_compare.out"
    local cmp_err="$build_dir/main_llvm_leg_compare.err"
    local c_rel
    local llvm_rel

    pgy_selfhost_compile_backend_output_comparator "$label" "$build_dir"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$build_dir")"
    c_rel="$(pgy_selfhost_path_relative_to_root "$c_out")"
    llvm_rel="$(pgy_selfhost_path_relative_to_root "$llvm_out")"

    if ! (cd "$ROOT_DIR" && "$comparator_bin" "$c_rel" "$llvm_rel" 0 1 \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[$label] LLVM-compiled tool output diverges from C-compiled tool" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

assert_llvm_leg() {
    local label="$1"
    local tool_arg="$2"
    local build_dir="$3"
    local c_bin="$build_dir/main_c_leg.exe"
    local llvm_bin="$build_dir/main_llvm_leg.exe"
    local c_compile_log="$build_dir/main_c_leg.compile.log"
    local llvm_compile_log="$build_dir/main_llvm_leg.compile.log"
    local c_out="$build_dir/main_c_leg.out"
    local llvm_out="$build_dir/main_llvm_leg.out"
    local c_err="$build_dir/main_c_leg.err"
    local llvm_err="$build_dir/main_llvm_leg.err"

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

    set +e
    (cd "$ROOT_DIR" && "$c_bin" 2>"$c_err" | pgy_selfhost_normalize_text_artifact >"$c_out")
    local c_rc=$?
    set -e
    if [[ "$c_rc" -ne 0 ]]; then
        echo "[$label] C-compiled tool run failed" >&2
        cat "$c_out" "$c_err" >&2
        exit 1
    fi

    set +e
    (cd "$ROOT_DIR" && "$llvm_bin" 2>"$llvm_err" | pgy_selfhost_normalize_text_artifact >"$llvm_out")
    local llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -ne 0 ]]; then
        echo "[$label] LLVM-compiled tool run failed" >&2
        cat "$llvm_out" "$llvm_err" >&2
        exit 1
    fi

    assert_llvm_leg_with_artifact_owner "$label" "$build_dir" "$c_out" "$llvm_out"
    echo "[$label] llvm-leg ok (C-tool==LLVM-tool artifact-equal)"
}
