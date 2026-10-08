#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
pgy_require_runnable_binary_here array-pixel-storage-benchmark "$PGY"
cd "$ROOT_DIR"

mkdir -p "$ROOT_DIR/.tmp"
WORK="$(mktemp -d .tmp/array-pixel-storage-benchmark.XXXXXX)"
FIXTURE="benchmarks/perf_pixel_storage.pgy"

for backend in c llvm; do
    "$PGY" --native-pipeline --backend="$backend" "$FIXTURE" \
        -o "$WORK/$backend.exe" >"$WORK/$backend.compile.log" 2>&1 || {
            cat "$WORK/$backend.compile.log" >&2
            echo "[array-pixel-storage] $backend compile failed" >&2
            exit 1
        }
    "$WORK/$backend.exe" >"$WORK/$backend.raw" 2>"$WORK/$backend.err" || {
        cat "$WORK/$backend.raw" "$WORK/$backend.err" >&2
        echo "[array-pixel-storage] $backend execution failed" >&2
        exit 1
    }
    tr -d '\r' <"$WORK/$backend.raw" >"$WORK/$backend.out"
    [[ ! -s "$WORK/$backend.err" ]]
    grep -Fxq 'PIXEL STORAGE BENCH v1' "$WORK/$backend.out"
    grep -Fxq 'timed_growth=0_by_operation_inventory' "$WORK/$backend.out"
    grep -Fxq 'allocation_count=UNMEASURED_no_public_counter' \
        "$WORK/$backend.out"
    grep -Fxq 'lifetime=Main_scope_reused_across_samples_cleanup_UNMEASURED' \
        "$WORK/$backend.out"
    grep -Fxq 'clock=Now_Long_ms_monotonic' "$WORK/$backend.out"
    grep -Fxq 'warmups=3 samples=101 pixels=4096 rounds=2000' \
        "$WORK/$backend.out"

    metric_field() {
        local label="$1"
        local field="$2"
        awk -v label="$label" -v field="$field" '
            $0 == label {
                for (i = 1; i <= 6; i++) {
                    if (getline <= 0) exit 2
                    value[i] = $0
                }
                print value[field]
                found = 1
                exit
            }
            END { if (!found) exit 3 }
        ' "$WORK/$backend.out"
    }
    flat_checksum="$(metric_field flat-int-rgba 1)"
    nominal_checksum="$(metric_field nominal-pixel 1)"
    nested_checksum="$(metric_field nested-int-rgba 1)"
    flat_length="$(metric_field flat-int-rgba 2)"
    nominal_length="$(metric_field nominal-pixel 2)"
    nested_length="$(metric_field nested-int-rgba 2)"
    flat_p50="$(metric_field flat-int-rgba 4)"
    flat_p95="$(metric_field flat-int-rgba 5)"
    nested_p95="$(metric_field nested-int-rgba 5)"
    [[ "$flat_checksum" == 31662080000 ]]
    [[ "$nominal_checksum" == 31662080000 ]]
    [[ "$nested_checksum" == 31662080000 ]]
    [[ "$flat_length" == 16384 ]]
    [[ "$nominal_length" == 4096 ]]
    [[ "$nested_length" == 16384 ]]

    if [[ "$flat_p50" -lt 32 ]]; then
        echo "[array-pixel-storage] $backend flat p50 ${flat_p50}ms is too short for the Now() clock; increase rounds" >&2
        exit 1
    fi

    # Same-layout regression criterion. This is a host-local, manual benchmark:
    # the 101-sample p95 ratio catches sustained nested projection overhead,
    # while the minimum-duration guard prevents sub-timer-quantum evidence.
    ratio_limit="$(( (flat_p95 * 135 + 99) / 100 ))"
    limit="$ratio_limit"
    if [[ "$nested_p95" -gt "$limit" ]]; then
        echo "[array-pixel-storage] $backend nested p95 ${nested_p95}ms exceeds ${limit}ms (flat ${flat_p95}ms)" >&2
        exit 1
    fi
    echo "[array-pixel-storage] $backend"
    cat "$WORK/$backend.out"
    echo "[array-pixel-storage] $backend same-layout criterion: nested p95 ${nested_p95}ms <= ${limit}ms (flat p95 ${flat_p95}ms)"
    printf '%s\n%s\n%s\n%s\n%s\n%s\n' \
        "$flat_checksum" "$flat_length" \
        "$nominal_checksum" "$nominal_length" \
        "$nested_checksum" "$nested_length" >"$WORK/$backend.semantic"
done

cmp -s "$WORK/c.semantic" "$WORK/llvm.semantic" || {
    echo '[array-pixel-storage] C/LLVM semantic records diverged' >&2
    diff -u "$WORK/c.semantic" "$WORK/llvm.semantic" >&2 || true
    exit 1
}
