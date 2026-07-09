#!/usr/bin/env bash
# clean-vs-cache parity gate for the self-host completeness harness.
#
# This is a narrow rung: it proves the current completeness cache does not
# change the focused ledger artifact for one owner source. It is not a claim
# that precise import/module invalidation is complete.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    if [[ -z "${PGY_BIN:-}" ]]; then
        echo "[self-host-incremental-cache] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[self-host-incremental-cache] missing compiler binary: $PGY" >&2
    exit 1
fi

SOURCE_FILTER="${PGY_SELFHOST_INCREMENTAL_CACHE_SOURCE:-src/self_hosted/compiler/incremental_fact_graph_owner.pgy}"
STAGE_FILTER="${PGY_SELFHOST_INCREMENTAL_CACHE_STAGES:-lexer,parser,semantic,codegen}"
BUILD_ROOT="${PGY_SELFHOST_INCREMENTAL_CACHE_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/incremental_cache_parity}"
mkdir -p "$BUILD_ROOT"
WORK_DIR="$(mktemp -d "$BUILD_ROOT/run.XXXXXX")"

run_ledger() {
    local label="$1"
    local cache_mode="$2"
    local build_dir="$3"
    local out="$WORK_DIR/${label}.out"
    local err="$WORK_DIR/${label}.err"

    mkdir -p "$build_dir"
    if ! PGY_BIN="$PGY" \
        PGY_SELFHOST_COMPLETENESS_CACHE="$cache_mode" \
        PGY_SELFHOST_COMPLETENESS_BUILD_DIR="$build_dir" \
        PGY_SELFHOST_COMPLETENESS_SOURCES="$SOURCE_FILTER" \
        PGY_SELFHOST_COMPLETENESS_STAGES="$STAGE_FILTER" \
        "${BASH:-bash}" "$ROOT_DIR/tests/self_hosted/parity/completeness_ledger.sh" >"$out" 2>"$err"; then
        echo "[self-host-incremental-cache] $label completeness run failed" >&2
        cat "$out" >&2
        cat "$err" >&2
        exit 1
    fi

    if [[ ! -f "$build_dir/ledger.tsv" ]]; then
        echo "[self-host-incremental-cache] $label missing ledger artifact" >&2
        cat "$out" >&2
        cat "$err" >&2
        exit 1
    fi

    grep -E '^(lexer|parser|semantic|codegen)[[:space:]]+pass=[0-9]+[[:space:]]+fail=[0-9]+$' \
        "$build_dir/ledger.tsv" >"$WORK_DIR/${label}.ledger"
    grep -F "[self-host-completeness] focused source-filter ledger ok:" "$out" \
        >"$WORK_DIR/${label}.summary"
}

run_ledger clean 0 "$WORK_DIR/clean"
run_ledger cache_prime 1 "$WORK_DIR/cached"
run_ledger cache_hit 1 "$WORK_DIR/cached"

if ! cmp -s "$WORK_DIR/clean.ledger" "$WORK_DIR/cache_prime.ledger"; then
    echo "[self-host-incremental-cache] clean and cache-prime ledger artifacts differ" >&2
    diff -u "$WORK_DIR/clean.ledger" "$WORK_DIR/cache_prime.ledger" >&2 || true
    exit 1
fi

if ! cmp -s "$WORK_DIR/clean.ledger" "$WORK_DIR/cache_hit.ledger"; then
    echo "[self-host-incremental-cache] clean and cache-hit ledger artifacts differ" >&2
    diff -u "$WORK_DIR/clean.ledger" "$WORK_DIR/cache_hit.ledger" >&2 || true
    exit 1
fi

if ! cmp -s "$WORK_DIR/clean.summary" "$WORK_DIR/cache_prime.summary"; then
    echo "[self-host-incremental-cache] clean and cache-prime summaries differ" >&2
    diff -u "$WORK_DIR/clean.summary" "$WORK_DIR/cache_prime.summary" >&2 || true
    exit 1
fi

if ! cmp -s "$WORK_DIR/clean.summary" "$WORK_DIR/cache_hit.summary"; then
    echo "[self-host-incremental-cache] clean and cache-hit summaries differ" >&2
    diff -u "$WORK_DIR/clean.summary" "$WORK_DIR/cache_hit.summary" >&2 || true
    exit 1
fi

if ! grep -E '\[self-host-completeness\] cache: schema=pgy[.]selfhost[.]completeness-cache[.]v1 hits=[1-9][0-9]*' \
    "$WORK_DIR/cache_hit.out" >/dev/null; then
    echo "[self-host-incremental-cache] cache-hit run did not report a cache hit" >&2
    cat "$WORK_DIR/cache_hit.out" >&2
    exit 1
fi

echo "[self-host-incremental-cache] clean/cache ledger parity ok source=$SOURCE_FILTER stages=$STAGE_FILTER"
