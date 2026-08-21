#!/usr/bin/env bash
# Emitted-C warning-cleanliness gate (docs/189 C14).
#
# The driver compiles emitted C with only -Wall; nothing failed on a
# warning, so emitted-code defects that gcc/clang can already see (sign
# compares, unused values, format mismatches) sailed through. This gate
# emits C for a representative fixture sample and compiles each unit at
# -Wall -Wextra -Werror on the strictest compiler available.
#
# Suppressions are ONLY for the single-TU inline-runtime shape (a
# generated program includes the whole runtime as static functions, so
# unused-function/parameter noise is structural, not a defect):
#   -Wno-unused-function -Wno-unused-parameter -Wno-missing-field-initializers
#
# Subject of this gate:
#   native C-emitter warning cleanliness across the representative corpus.
# Delegating would turn a self-host collection-surface gap into an emitted-C
# regression. This is the declared in-process opt-out, never a fallback.
# See docs/152_validation_isolation_policy.md.
PGY_NATIVE_PIPELINE=1
export PGY_NATIVE_PIPELINE

set -euo pipefail

if ! command -v dirname >/dev/null 2>&1 \
    || ! command -v pwd >/dev/null 2>&1; then
    PATH="/usr/bin:/bin:$PATH"
    export PATH
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

CC_BIN="${CC:-gcc}"
if ! command -v "$CC_BIN" >/dev/null 2>&1; then
    echo "[emitted-c-warnings] SKIP: no C compiler on PATH" >&2
    exit 0
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-emitted-warn.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

WARN_FLAGS=(-std=c11 -fwrapv -fno-strict-aliasing
            -Wall -Wextra -Werror
            -Wno-unused-function -Wno-unused-parameter
            -Wno-missing-field-initializers
            -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime")

# Representative slice of the corpus: scalars/casts, strings, collections,
# control flow, classes, options/results, channels, parallel.
CASES=(
    cast_numeric
    float_arith_chain
    long_cast_roundtrip
    bool_short_circuit_calls
    string_alphanum_extract
    string_compress_runlength
    array_binary_search
    map_count_unique
    class_bool_method_filter
    for_in_double_option_match
    match_guard_or_pattern
    channel_send_recv_basic
    select_ready
    parallel_channel_sum
    triple_paradigm
)

checked=0
for case_name in "${CASES[@]}"; do
    src="$ROOT_DIR/tests/cases/backend_compare/$case_name/main.pgy"
    if [[ ! -f "$src" ]]; then
        echo "[emitted-c-warnings] missing fixture: $case_name" >&2
        exit 1
    fi
    emitted="$WORK_DIR/$case_name.c"
    if ! "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" \
        --emit-c -o "$(pgy_path_for_compiler "$PGY" "$emitted")" \
        >"$WORK_DIR/$case_name.emit.log" 2>&1; then
        echo "[emitted-c-warnings] emit failed for $case_name" >&2
        tail -5 "$WORK_DIR/$case_name.emit.log" >&2
        exit 1
    fi
    if ! "$CC_BIN" "${WARN_FLAGS[@]}" -c "$emitted" \
        -o "$WORK_DIR/$case_name.o" >"$WORK_DIR/$case_name.cc.log" 2>&1; then
        echo "[emitted-c-warnings] $case_name is not warning-clean under $CC_BIN -Wall -Wextra -Werror:" >&2
        grep -E "warning:|error:" "$WORK_DIR/$case_name.cc.log" | head -10 >&2
        exit 1
    fi
    checked=$((checked + 1))
done

echo "[emitted-c-warnings] $checked emitted units compile warning-clean at -Wall -Wextra -Werror ($CC_BIN)"
