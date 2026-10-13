#!/usr/bin/env bash
# Focused C/LLVM parity for semantic-owned assignment projections.

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v tr >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:assignment-projection"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
fi
pgy_reject_wsl_windows_pgy_parity_mix "$LABEL" "$PGY"

BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/assignment_projection"
PATHS_FILE="$BUILD_DIR/codegen_paths.txt"
mkdir -p "$BUILD_DIR"
pgy_selfhost_read_test_harness_manifest \
    "$LABEL" "$BUILD_DIR" "codegen-parity-paths" "$PATHS_FILE"

paths=()
while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ -n "$line" ]] && paths+=("$line")
done <"$PATHS_FILE"
if [[ "${#paths[@]}" -ne 21 ]]; then
    echo "[$LABEL] expected 21 codegen TestHarness paths, got ${#paths[@]}" >&2
    exit 1
fi

PROBE_SOURCE="$ROOT_DIR/${paths[11]}"
EXPECTED="$ROOT_DIR/${paths[12]}"
for input in "$PROBE_SOURCE" "$EXPECTED"; do
    [[ -f "$input" ]] || { echo "[$LABEL] missing input: $input" >&2; exit 1; }
done

compile_probe() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local log="$BUILD_DIR/probe_${backend}.compile.log"
    local compile_pid
    local compile_rc
    local elapsed=0
    local subtree_rss_kib
    local mem_available_kib
    local swap_free_kib
    # This gate owns projection semantics and fail-closed runtime behavior, not
    # optimizer throughput. Both profiles use the installed self-host artifact
    # owners; dev keeps the fixed compiler-scale C/LLVM probes at -O0 instead
    # of making this focused gate pay the unrelated -O3 resource peak.
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$PROBE_SOURCE")" \
        --backend="$backend" \
        --opt=dev \
        -o "$(pgy_path_for_compiler "$PGY" "$bin")" >"$log" 2>&1) &
    compile_pid="$!"
    while kill -0 "$compile_pid" 2>/dev/null; do
        sleep 15
        if ! kill -0 "$compile_pid" 2>/dev/null; then
            break
        fi
        elapsed=$((elapsed + 15))
        subtree_rss_kib="unknown"
        if command -v ps >/dev/null 2>&1 && command -v awk >/dev/null 2>&1; then
            subtree_rss_kib="$(
                ps -eo pid=,ppid=,rss= 2>/dev/null |
                    awk -v root="$compile_pid" '
                        { rss[$1] = $3; parent[$1] = $2 }
                        END {
                            for (p in rss) {
                                q = p
                                while (q > 1 && q != root && (q in parent)) {
                                    q = parent[q]
                                }
                                if (q == root) { total += rss[p] }
                            }
                            print total + 0
                        }
                    ' || true
            )"
        fi
        mem_available_kib="unknown"
        swap_free_kib="unknown"
        if [[ -r /proc/meminfo ]]; then
            mem_available_kib="$(
                awk '$1 == "MemAvailable:" { print $2 }' /proc/meminfo
            )"
            swap_free_kib="$(
                awk '$1 == "SwapFree:" { print $2 }' /proc/meminfo
            )"
        fi
        echo "[$LABEL] backend=$backend compile heartbeat elapsed=${elapsed}s subtree_rss_kib=${subtree_rss_kib:-unknown} mem_available_kib=${mem_available_kib:-unknown} swap_free_kib=${swap_free_kib:-unknown}"
    done
    set +e
    wait "$compile_pid"
    compile_rc="$?"
    set -e
    if [[ "$compile_rc" -ne 0 ]]; then
        if [[ "$backend" == "llvm" ]] \
            && pgy_selfhost_log_reports_no_llvm "$log"; then
            return 2
        fi
        echo "[$LABEL] backend=$backend compile failed" >&2
        cat "$log" >&2
        exit 1
    fi
}

run_positive() {
    local backend="$1"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local raw="$BUILD_DIR/probe_${backend}.raw"
    local out="$BUILD_DIR/probe_${backend}.out"
    local err="$BUILD_DIR/probe_${backend}.err"
    if ! (cd "$ROOT_DIR" && "$bin" >"$raw" 2>"$err"); then
        echo "[$LABEL] backend=$backend positive probe failed" >&2
        cat "$raw" "$err" >&2
        exit 1
    fi
    pgy_selfhost_normalize_text_artifact <"$raw" >"$out"
    pgy_selfhost_compare_expected_text_artifact_file_with_owner \
        "$LABEL" "$BUILD_DIR" "$EXPECTED" "$out" "run_output"
}

run_missing_fact_negative() {
    local backend="$1"
    local mode="$2"
    local diagnostic="$3"
    local bin="$BUILD_DIR/probe_${backend}.exe"
    local out="$BUILD_DIR/probe_${backend}.${mode}.out"
    local err="$BUILD_DIR/probe_${backend}.${mode}.err"
    local rc
    set +e
    (cd "$ROOT_DIR" && "$bin" "--$mode" >"$out" 2>"$err")
    rc=$?
    set -e
    if [[ "$rc" -eq 0 ]]; then
        echo "[$LABEL] backend=$backend $mode did not fail closed" >&2
        exit 1
    fi
    if ! grep -Fq "$diagnostic" "$out" "$err"; then
        echo "[$LABEL] backend=$backend $mode diagnostic drifted" >&2
        cat "$out" "$err" >&2
        exit 1
    fi
}

run_backend() {
    local backend="$1"
    compile_probe "$backend"
    run_positive "$backend"
    run_missing_fact_negative "$backend" "missing-expected-type" \
        "assignment expected type is missing"
    run_missing_fact_negative "$backend" "missing-target-type" \
        "indexed assignment target type is missing"
    run_missing_fact_negative "$backend" "missing-call-target" \
        "semantic direct-call target fact is missing"
    run_missing_fact_negative "$backend" "missing-c-binding" \
        "assignment target C binding fact is missing"
    run_missing_fact_negative "$backend" "missing-place-kind" \
        "semantic leaf place-kind fact is missing"
    run_missing_fact_negative "$backend" "missing-leaf-c-binding" \
        "semantic leaf binding fact is missing"
    run_missing_fact_negative "$backend" "collection-cref-only" \
        "collection target C binding fact is missing"
}

run_backend c

BACKENDS="${PGY_SELFHOST_ASSIGNMENT_PROJECTION_BACKENDS:-c llvm}"
if [[ " $BACKENDS " == *" llvm "* ]]; then
    set +e
    compile_probe llvm
    llvm_rc=$?
    set -e
    if [[ "$llvm_rc" -eq 0 ]]; then
        run_positive llvm
        run_missing_fact_negative llvm "missing-expected-type" \
            "assignment expected type is missing"
        run_missing_fact_negative llvm "missing-target-type" \
            "indexed assignment target type is missing"
        run_missing_fact_negative llvm "missing-call-target" \
            "semantic direct-call target fact is missing"
        run_missing_fact_negative llvm "missing-c-binding" \
            "assignment target C binding fact is missing"
        run_missing_fact_negative llvm "missing-place-kind" \
            "semantic leaf place-kind fact is missing"
        run_missing_fact_negative llvm "missing-leaf-c-binding" \
            "semantic leaf binding fact is missing"
        assert_llvm_leg_with_artifact_owner \
            "$LABEL" "$BUILD_DIR" \
            "$BUILD_DIR/probe_c.out" "$BUILD_DIR/probe_llvm.out"
    elif [[ "$llvm_rc" -eq 2 ]]; then
        echo "[$LABEL] llvm-leg skipped (compiler built without LLVM backend support)"
    else
        exit "$llvm_rc"
    fi
fi

echo "[$LABEL] semantic assignment projection parity ok"
