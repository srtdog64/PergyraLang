#!/usr/bin/env bash
# Emitted-shape golden for the parallel join lowering (docs/188 R4).
#
# The join fan-out is guarded by behavior fixtures and the run-to-run
# determinism gate, but neither pins the SHAPE of what the emitters produce
# -- a regression that keeps behavior (drop the chunk-ctx free, widen the
# await bound back to n, bypass the chunk entry points) can hide from both.
# This gate emits one index-mode reduce join with --emit-c/--emit-llvm and
# greps stable identifiers only (docs/188 R2 pin doctrine): entry-point
# calls, loop bounds, and the release of the heap the fan-out owns.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/parallel_join_emit_shape_smoke.sh
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="parallel-join-emit-shape"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
if [[ ! -x "$PGY" ]]; then
    echo "[$LABEL] SKIP missing compiler binary: $PGY"
    exit 0
fi
if ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[$LABEL] SKIP compiler binary is not runnable here: $PGY"
    exit 0
fi

TMP_BASE="${TMPDIR:-${TEMP:-/tmp}}"
if pgy_binary_expects_windows_paths "$PGY"; then
    TMP_BASE="$ROOT_DIR/.tmp"
    mkdir -p "$TMP_BASE"
fi
WORK_DIR="$(mktemp -d "${TMP_BASE%/}/pgy_join_shape.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat >"$WORK_DIR/main.pgy" <<'PGYSRC'
func Main() -> Void {
    let total: Int = parallel (i in 0..100) join with sum { give i; };
    Log(total);
}
PGYSRC

require_shape() { # $1 = artifact, $2 = pin, $3 = human name
    if ! grep -qF -- "$2" "$1"; then
        echo "[$LABEL] shape drift: $3 pin '$2' missing from $(basename "$1")" >&2
        exit 1
    fi
}

# --- C leg ---
C_ART="$WORK_DIR/main.c"
if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/main.pgy")" \
    --emit-c -o "$(pgy_path_for_compiler "$PGY" "$C_ART")") \
    >"$WORK_DIR/c.emit.log" 2>&1; then
    echo "[$LABEL] --emit-c failed" >&2
    cat "$WORK_DIR/c.emit.log" >&2
    exit 1
fi
require_shape "$C_ART" "pgy_parallel_chunk_count(_pj_n_"      "C chunk-count call"
require_shape "$C_ART" "pgy_parallel_chunk_ctxs_alloc(_pj_nch_" "C chunk-table alloc"
require_shape "$C_ART" "pgy_parallel_spawn_chunk_at(_pj_cc_"  "C chunk spawn call"
require_shape "$C_ART" "_pj_k < _pj_nch_"                     "C spawn loop bound (chunks)"
require_shape "$C_ART" "_pj_i < _pj_nch_"                     "C await loop bound (chunks)"
require_shape "$C_ART" "pgy_lane_await(_pj_hs_"               "C handle await"
require_shape "$C_ART" "free(_pj_cc_"                         "C chunk-table free"
require_shape "$C_ART" "free(_pj_ctxs_"                       "C ctx-array free"
require_shape "$C_ART" "free(_pj_hs_"                         "C handle-array free"
echo "[$LABEL] PASS c (9 shape pins)"

# --- LLVM leg ---
if "$PGY" --help 2>&1 | grep -q -- "--emit-llvm"; then
    LL_ART="$WORK_DIR/main.ll"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$WORK_DIR/main.pgy")" \
        --emit-llvm -o "$(pgy_path_for_compiler "$PGY" "$LL_ART")") \
        >"$WORK_DIR/ll.emit.log" 2>&1; then
        echo "[$LABEL] --emit-llvm failed" >&2
        cat "$WORK_DIR/ll.emit.log" >&2
        exit 1
    fi
    require_shape "$LL_ART" "pgy_parallel_chunk_count_export"      "LLVM chunk-count call"
    require_shape "$LL_ART" "pgy_parallel_chunk_ctxs_alloc_export" "LLVM chunk-table alloc"
    require_shape "$LL_ART" "pgy_parallel_spawn_chunk_at_export"   "LLVM chunk spawn call"
    require_shape "$LL_ART" "pgy_await_export"                     "LLVM handle await"
    require_shape "$LL_ART" "@free"                                "LLVM chunk-table free"
    echo "[$LABEL] PASS llvm (5 shape pins)"
else
    echo "[$LABEL] SKIP llvm (compiler built without LLVM support)"
fi

echo "[$LABEL] join lowering keeps the chunked fan-out shape on both backends"
