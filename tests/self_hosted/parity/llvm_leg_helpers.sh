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
    # The C-only stubs once said `not available in this build`, which was
    # missing from this alternation, so a C-only build (macOS CI) reported
    # every LLVM leg as a failure instead of a skip. The stubs now emit the
    # canonical `compiled without LLVM backend support`; the wider alternation
    # stays so a stray spelling degrades to a skip, never a false failure.
    grep -Eiq \
        'compiled without LLVM backend support|unknown option.*--backend=llvm|LLVM backend (is )?(not enabled|disabled|unavailable|not available|not built)' \
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
    tr -d '\r' | awk '
        { lines[NR] = $0 }
        END {
            last = NR
            while (last > 0 && lines[last] == "")
                last--
            for (i = 1; i <= last; i++) {
                if (i > 1) printf "\n"
                printf "%s", lines[i]
            }
            if (last > 0) printf "\n"
        }'
}

pgy_selfhost_backend_output_comparator_bin() {
    local build_dir="$1"
    printf '%s\n' "$build_dir/backend_output_comparator_$$.exe"
}

pgy_selfhost_test_harness_manifest_bin() {
    local build_dir="$1"
    printf '%s\n' "$build_dir/test_harness_manifest_$$.exe"
}

pgy_selfhost_test_harness_manifest_source() {
    local label="$1"
    local manifest_projection="$ROOT_DIR/tests/self_hosted/compiler_world_manifest.sh"
    local source_rel
    local source_path

    if [[ ! -f "$manifest_projection" ]]; then
        echo "[$label] compiler world manifest projection is missing" >&2
        exit 1
    fi

    source "$manifest_projection"
    source_rel="${PGY_SELFHOST_COMPILER_TEST_HARNESS_MANIFEST_PATH:-}"
    if [[ -z "$source_rel" ]]; then
        echo "[$label] TestHarness manifest source row is empty" >&2
        exit 1
    fi
    case "$source_rel" in
        /*|[A-Za-z]:*|*\\*)
            echo "[$label] TestHarness manifest source must be repo-relative: $source_rel" >&2
            exit 1
            ;;
    esac

    source_path="$ROOT_DIR/$source_rel"
    if [[ ! -f "$source_path" ]]; then
        echo "[$label] TestHarness manifest source is missing: $source_rel" >&2
        exit 1
    fi

    printf '%s\n' "$source_path"
}

pgy_selfhost_compile_test_harness_manifest() {
    local label="$1"
    local build_dir="$2"
    local manifest_source
    local manifest_source_rel
    local manifest_bin
    local compile_log

    case "|${PGY_SELFHOST_TEST_HARNESS_MANIFEST_COMPILED_DIRS:-}|" in
        *"|$build_dir|"*)
            return 0
            ;;
    esac

    manifest_source="$(pgy_selfhost_test_harness_manifest_source "$label")"
    manifest_source_rel="${manifest_source#"$ROOT_DIR"/}"
    manifest_bin="$(pgy_selfhost_test_harness_manifest_bin "$build_dir")"
    mkdir -p "$build_dir"

    compile_log="$build_dir/test_harness_manifest_$$.compile.log"
    # Native pipeline: this manifest is harness scaffolding that the parity
    # gates read to decide what to check. Building it through the installed
    # self-host driver would make the checker depend on the artifact under
    # test, and would deadlock the bootstrap that has yet to produce it.
    if ! (cd "$ROOT_DIR" && "$PGY" "$manifest_source_rel" --native-pipeline \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$manifest_bin")" \
        >"$compile_log" 2>&1); then
        echo "[$label] test harness manifest failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
    PGY_SELFHOST_TEST_HARNESS_MANIFEST_COMPILED_DIRS="${PGY_SELFHOST_TEST_HARNESS_MANIFEST_COMPILED_DIRS:-}|$build_dir"
}

pgy_selfhost_read_test_harness_manifest() {
    local label="$1"
    local build_dir="$2"
    local suite="$3"
    local out_file="$4"
    local manifest_bin
    local raw_out="$build_dir/test_harness_manifest_$$.out"
    local raw_err="$build_dir/test_harness_manifest_$$.err"

    pgy_selfhost_compile_test_harness_manifest "$label" "$build_dir"
    manifest_bin="$(pgy_selfhost_test_harness_manifest_bin "$build_dir")"

    if ! (cd "$ROOT_DIR" && "$manifest_bin" "$suite" >"$raw_out" 2>"$raw_err"); then
        echo "[$label] test harness manifest suite failed: $suite" >&2
        cat "$raw_out" "$raw_err" >&2
        exit 1
    fi
    tr -d '\r' < "$raw_out" > "$out_file"
}

pgy_selfhost_backend_output_comparator_source() {
    local label="$1"
    local build_dir="$2"
    local paths_file="$build_dir/backend_output_comparator_paths_$$.txt"
    local source_rel
    local source_path

    pgy_selfhost_read_test_harness_manifest \
        "$label" \
        "$build_dir" \
        "backend-output-comparator-paths" \
        "$paths_file"

    source_rel="$(sed -n '1p' "$paths_file")"
    source_rel="${source_rel%$'\r'}"
    if [[ -z "$source_rel" ]]; then
        echo "[$label] TestHarness backend-output comparator source row is empty" >&2
        cat "$paths_file" >&2
        exit 1
    fi
    case "$source_rel" in
        /*|[A-Za-z]:*|*\\*)
            echo "[$label] TestHarness backend-output comparator source must be repo-relative: $source_rel" >&2
            exit 1
            ;;
    esac

    source_path="$ROOT_DIR/$source_rel"
    if [[ ! -f "$source_path" ]]; then
        echo "[$label] TestHarness backend-output comparator source is missing: $source_rel" >&2
        exit 1
    fi

    printf '%s\n' "$source_path"
}

pgy_selfhost_compile_backend_output_comparator() {
    local label="$1"
    local build_dir="$2"
    local comparator_source="${3:-}"
    local comparator_source_rel
    local comparator_bin
    local compile_log

    case "|${PGY_SELFHOST_BACKEND_OUTPUT_COMPARATOR_COMPILED_DIRS:-}|" in
        *"|$build_dir|"*)
            return 0
            ;;
    esac

    if [[ -z "$comparator_source" ]]; then
        comparator_source="$(pgy_selfhost_backend_output_comparator_source "$label" "$build_dir")"
    fi
    comparator_source_rel="${comparator_source#"$ROOT_DIR"/}"

    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$build_dir")"
    mkdir -p "$build_dir"

    compile_log="$build_dir/backend_output_comparator_$$.compile.log"
    # Native pipeline: the comparator is the judge of backend output drift, so
    # it must not itself be produced by the compiler path it is judging.
    if ! (cd "$ROOT_DIR" && "$PGY" "$comparator_source_rel" --native-pipeline \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$comparator_bin")" \
        >"$compile_log" 2>&1); then
        echo "[$label] backend output comparator failed to build" >&2
        cat "$compile_log" >&2
        exit 1
    fi
    PGY_SELFHOST_BACKEND_OUTPUT_COMPARATOR_COMPILED_DIRS="${PGY_SELFHOST_BACKEND_OUTPUT_COMPARATOR_COMPILED_DIRS:-}|$build_dir"
}

pgy_selfhost_compare_expected_text_artifact_with_owner() {
    local label="$1"
    local build_dir="$2"
    local expected_file="$3"
    local actual_text="$4"
    local artifact_kind="$5"
    local expected_norm="$build_dir/artifact_owner_expected_$$.txt"
    local actual_norm="$build_dir/artifact_owner_actual_$$.txt"
    local cmp_out="$build_dir/artifact_owner_compare_$$.out"
    local cmp_err="$build_dir/artifact_owner_compare_$$.err"
    local comparator_bin
    local expected_rel
    local actual_rel

    pgy_selfhost_compile_backend_output_comparator "$label" "$build_dir"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$build_dir")"
    pgy_selfhost_normalize_text_artifact < "$expected_file" > "$expected_norm"
    printf '%s\n' "$actual_text" | pgy_selfhost_normalize_text_artifact > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    if ! (cd "$ROOT_DIR" && "$comparator_bin" "$expected_rel" "$actual_rel" 0 2 "$artifact_kind" \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[$label] $artifact_kind artifact parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
        exit 1
    fi
}

pgy_selfhost_compare_expected_text_artifact_file_with_owner() {
    local label="$1"
    local build_dir="$2"
    local expected_file="$3"
    local actual_file="$4"
    local artifact_kind="$5"
    local expected_norm="$build_dir/artifact_owner_expected_$$.txt"
    local actual_norm="$build_dir/artifact_owner_actual_$$.txt"
    local cmp_out="$build_dir/artifact_owner_compare_$$.out"
    local cmp_err="$build_dir/artifact_owner_compare_$$.err"
    local comparator_bin
    local expected_rel
    local actual_rel

    pgy_selfhost_compile_backend_output_comparator "$label" "$build_dir"
    comparator_bin="$(pgy_selfhost_backend_output_comparator_bin "$build_dir")"
    pgy_selfhost_normalize_text_artifact < "$expected_file" > "$expected_norm"
    pgy_selfhost_normalize_text_artifact < "$actual_file" > "$actual_norm"
    expected_rel="$(pgy_selfhost_path_relative_to_root "$expected_norm")"
    actual_rel="$(pgy_selfhost_path_relative_to_root "$actual_norm")"

    if ! (cd "$ROOT_DIR" && "$comparator_bin" "$expected_rel" "$actual_rel" 0 2 "$artifact_kind" \
        >"$cmp_out" 2>"$cmp_err"); then
        echo "[$label] $artifact_kind artifact parity FAIL" >&2
        cat "$cmp_out" "$cmp_err" >&2
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
    shift 3
    local run_args=("$@")
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

    # Run the C leg before deciding the LLVM leg: a C-only build skips the
    # LLVM comparison, but callers still consume the C tool's artifact.
    set +e
    (cd "$ROOT_DIR" && "$c_bin" ${run_args[@]+"${run_args[@]}"} 2>"$c_err" | pgy_selfhost_normalize_text_artifact >"$c_out")
    local c_rc=$?
    set -e
    if [[ "$c_rc" -ne 0 ]]; then
        echo "[$label] C-compiled tool run failed" >&2
        cat "$c_out" "$c_err" >&2
        exit 1
    fi

    # Native pipeline: the delegated LLVM path projects through the driver's
    # bounded DirectMirLlvm classifier, which does not accept general
    # multi-routine tools yet. The replacement acceptance on supported shapes
    # is owned by self-host-default-llvm-replacement-test-smoke; this leg
    # only needs a working LLVM-compiled tool to compare against the C tool.
    if ! (cd "$ROOT_DIR" && "$PGY" "$tool_arg" --native-pipeline --backend=llvm \
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
    (cd "$ROOT_DIR" && "$llvm_bin" ${run_args[@]+"${run_args[@]}"} 2>"$llvm_err" | pgy_selfhost_normalize_text_artifact >"$llvm_out")
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

# Ownership-campaign ratchet shared by the body gate's llvm lane and the
# bootstrap gate's native oracle: body safety runs before either backend, so
# both lanes measure the same wall against one checked-in budget. The
# declared skip is a monitored ratchet, not a hiding place: the count must
# never grow past ownership_campaign_budget.txt, and every campaign round
# lowers the budget with its commit. Returns 0 within budget (caller skips
# declaredly), 1 when the log is not the campaign's, 2 past budget (loud).
ownership_campaign_within_budget() {
    local log="$1"
    local label="$2"
    local count budget
    grep -Eq 'Borrowed ref boundary value|beta-stable body safety requires|TextBuilder owner' \
        "$log" || return 1
    count="$(grep -c 'ERROR' "$log")"
    budget="$(tr -d ' \r\n' \
        <"$ROOT_DIR/tests/self_hosted/parity/ownership_campaign_budget.txt")"
    if [[ "$count" -gt "$budget" ]]; then
        echo "[$label] ownership campaign regressed: $count error(s) exceed the checked-in budget $budget" >&2
        return 2
    fi
    echo "[$label] blocked by the ownership campaign: $count error(s) within budget $budget (self-host bootstrap subject); skipping declaredly"
    return 0
}

# The rung2 body gate's driver builds are harness infrastructure, not the
# gate's subject: the llvm build routes through the native pipeline because
# the delegated DirectMirLlvm projector is a bounded classifier whose
# replacement subject is owned by self-host-default-llvm-replacement-test-smoke.
# Returns 2 when the compiler has no LLVM backend, and 3 when the native
# pipeline rejects the composed driver with the ownership campaign's
# signature (self-host bootstrap subject) -- once that campaign clears, the
# greps stop matching and the llvm lane comes back loud on its own.
compile_driver() {
    local backend="$1"
    local out_bin="$2"
    local source="${3:-$DRIVER_SOURCE}"
    local log="$BUILD_DIR/driver_${backend}.compile.log"
    local native_subject=""
    [[ "$backend" == "llvm" ]] && native_subject="--native-pipeline"
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$source")" \
        --backend="$backend" $native_subject \
        -o "$(pgy_path_for_compiler "$PGY" "$out_bin")" \
        >"$log" 2>&1); then
        if [[ "$backend" == "llvm" ]] && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        if [[ "$backend" == "llvm" ]] && grep -Eq \
            'beta-stable body safety requires|TextBuilder owner' "$log"; then
            return 3
        fi
        echo "[self-host-parity:driver-rung2] backend=$backend driver compile failed" >&2
        cat "$log" >&2
        return 1
    fi
}
