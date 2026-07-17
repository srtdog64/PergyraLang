#!/usr/bin/env bash
# Worker-count invariance witness (docs/186 P-D, WO-RT-4 residue 4).
#
# The join contract says results are an INDEX-ORDER left fold, independent of
# worker count, execution order, and (since WO-RT-4 B3) chunk boundaries --
# pgy_parallel_chunk_count(n) = min(n, workers x 4), so varying the observable
# PGY_WORKERS override varies the chunk split itself. This smoke runs one
# fixture under PGY_WORKERS in {1, 2, 3, 16} on both backends and requires
# byte-identical output.
#
# The fixture's teeth:
#   - Int sum/min/max over an index range (values span negative..positive);
#   - an element-mode collect whose element-mode re-sum must equal the
#     index-mode sum (chunking over collections too);
#   - a Float sum built from 0.1 steps (binary-inexact, association-sensitive)
#     compared BIT-EQUAL in-language against the serial left fold computed at
#     construction -- if any worker count ever folds per-chunk partials
#     instead of the full index-order walk, this line flips to 0.
#
# Usage: PGY_BIN=bin/pgy.exe bash tests/parallel_worker_invariance_smoke.sh

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="parallel-worker-invariance"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
PGY_EXPLICIT=0
[[ -n "${PGY_BIN:-}" ]] && PGY_EXPLICIT=1
if [[ ! -x "$PGY" ]]; then
    if [[ "$PGY_EXPLICIT" -eq 0 ]]; then
        echo "[$LABEL] SKIP missing compiler binary: $PGY"
        exit 0
    fi
    echo "[$LABEL] missing compiler binary: $PGY" >&2
    exit 1
fi

WORK="$(mktemp -d "${TMPDIR:-/tmp}/pgy_worker_invariance.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

FIXTURE="$WORK/main.pgy"
cat >"$FIXTURE" <<'PGY'
func Weight(i: Int) -> Int {
    return ((i * 31 + 7) % 101) - 50;
}

func Main() -> Void {
    let s: Int = parallel (i in 0..1000) join with sum { give Weight(i); };
    Log(s);
    let mn: Int = parallel (i in 0..1000) join with min { give Weight(i); };
    Log(mn);
    let mx: Int = parallel (i in 0..1000) join with max { give Weight(i); };
    Log(mx);
    let arr: Array<Int> = parallel (i in 0..1000) join with all { give Weight(i); };
    let s2: Int = parallel (x in arr) join with sum { give x; };
    if (s2 == s) { Log(1); } else { Log(0); }
    let mut fs: Array<Float> = [];
    let mut serial: Float = 0.0;
    let mut w: Float = 0.0;
    for k in 0..1000 {
        if (k % 7 == 0) { w = 0.0; }
        ArrayPush(fs, w);
        serial = serial + w;
        w = w + 0.1;
    }
    let fsum: Float = parallel (f in fs) join with sum { give f; };
    if (fsum == serial) { Log(1); } else { Log(0); }
}
PGY

WORKER_COUNTS=(1 2 3 16)
for backend in c llvm; do
    binary="$WORK/witness_$backend"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
        "--backend=$backend" -o "$(pgy_path_for_compiler "$PGY" "$binary")" \
        >"$WORK/$backend.compile.log" 2>&1); then
        echo "[$LABEL] $backend compile failed" >&2
        cat "$WORK/$backend.compile.log" >&2
        exit 1
    fi
    [[ -x "$binary.exe" ]] && binary="$binary.exe"

    reference=""
    for wc in "${WORKER_COUNTS[@]}"; do
        out="$WORK/$backend.w$wc.out"
        if ! PGY_WORKERS="$wc" "$binary" >"$out" 2>"$WORK/$backend.w$wc.err"; then
            echo "[$LABEL] $backend run failed under PGY_WORKERS=$wc" >&2
            cat "$out" "$WORK/$backend.w$wc.err" >&2
            exit 1
        fi
        tr -d '\r' <"$out" >"$out.norm"
        if [[ "$(tail -1 "$out.norm")" != "1" ]]; then
            echo "[$LABEL] $backend PGY_WORKERS=$wc: Float fold diverged from the serial left fold" >&2
            cat "$out.norm" >&2
            exit 1
        fi
        if [[ -z "$reference" ]]; then
            reference="$out.norm"
        elif ! cmp -s "$reference" "$out.norm"; then
            echo "[$LABEL] $backend output varies with worker count (w=${WORKER_COUNTS[0]} vs w=$wc)" >&2
            diff -u "$reference" "$out.norm" >&2 || true
            exit 1
        fi
    done
    echo "[$LABEL] PASS $backend (byte-identical across PGY_WORKERS=${WORKER_COUNTS[*]})"
done

# Cross-backend: the two references must match too (same fold, same spelling).
if ! cmp -s "$WORK/c.w${WORKER_COUNTS[0]}.out.norm" \
            "$WORK/llvm.w${WORKER_COUNTS[0]}.out.norm"; then
    echo "[$LABEL] C and LLVM outputs diverge" >&2
    diff -u "$WORK/c.w${WORKER_COUNTS[0]}.out.norm" \
            "$WORK/llvm.w${WORKER_COUNTS[0]}.out.norm" >&2 || true
    exit 1
fi

echo "[$LABEL] join results are worker-count- and chunk-boundary-invariant"
