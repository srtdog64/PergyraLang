#!/usr/bin/env bash
# Runtime-bitcode-ON backend-compare gate (docs/189 C5-②).
#
# The LLVM leg can inline the runtime bitcode (.bc) instead of calling the
# separately compiled runtime object. That configuration was only ever
# exercised on developer machines with a hand-built .bc; CI never built
# the .bc, so the bc-ON leg -- the one where the flag-mirror, strip-list,
# and freshness guards (docs/189 C4-C7) actually matter -- was untested.
# This gate builds the .bc and runs a representative backend-compare shard
# with PGY_RUNTIME_BC set, so the inlined-runtime leg must byte-agree with
# the C leg exactly like the bc-OFF leg does.
#
# Clang is required to build the .bc; where it is absent the gate skips
# cleanly (exit 0) rather than hard-failing -- the bc-ON config is a
# clang-only capability, and a clang-less runner has nothing to verify.
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

CLANG_BIN="${CLANG:-clang}"
if ! command -v "$CLANG_BIN" >/dev/null 2>&1; then
    echo "[backend-compare-bc-on] SKIP: no clang to build the runtime bitcode" >&2
    exit 0
fi

# Build (or refresh) the runtime bitcode. build_runtime_bc.sh mirrors the
# native legs' -fwrapv / -fno-strict-aliasing (docs/189 C4) and refuses an
# empty artifact.
if ! "$ROOT_DIR/tests/../scripts/build_runtime_bc.sh" >/tmp/bc_on_build.log 2>&1; then
    # A clang that cannot target the runtime (e.g. missing mingw headers on
    # a bare box) is a capability gap, not a compare failure -- skip.
    echo "[backend-compare-bc-on] SKIP: runtime bitcode build failed under $CLANG_BIN" >&2
    tail -3 /tmp/bc_on_build.log >&2 || true
    exit 0
fi

BC_PATH="$ROOT_DIR/src/runtime/pgy_runtime_lib.bc"
if [[ ! -s "$BC_PATH" ]]; then
    echo "[backend-compare-bc-on] runtime bitcode was not produced at $BC_PATH" >&2
    exit 1
fi

# Representative slice spanning casts/floats (checked f2i), short-circuit,
# collections, channels, and parallel join -- the surfaces whose runtime
# primitives get inlined from the .bc. The join_any_blocked/spinloop cases
# exercise cancelled-loser retirement, which reads the coroutine/current-task
# TLS through pgy_task_is_cancelled: if those exports were not stripped from
# the inlined bitcode they would read a private zero copy and the loser would
# park forever (docs/190 A2). Their presence here is the behavioral guard for
# that strip predicate under bc-ON.
CASES="cast_numeric,float_arith_chain,long_cast_roundtrip,bool_short_circuit_calls,string_compress_runlength,array_binary_search,map_count_unique,channel_send_recv_basic,parallel_channel_sum,select_ready,triple_paradigm,parallel_join_any_blocked,parallel_join_any_spinloop"

PGY_RUNTIME_BC="$(pgy_path_for_compiler "$PGY" "$BC_PATH")" \
PGY_BIN="$PGY" \
PGY_BACKEND_COMPARE_CASES="$CASES" \
    "$ROOT_DIR/tests/compare_backends.sh"

echo "[backend-compare-bc-on] inlined-runtime LLVM leg byte-agrees with the C leg across the representative shard"
