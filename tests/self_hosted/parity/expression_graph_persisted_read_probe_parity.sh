#!/usr/bin/env bash
# Executable fail-closed proof for the one-pass persisted expression-graph
# reader: the canonical and reordered documents append; every malformation
# mode must exit nonzero through the exact consumer-owned reader.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
source "$ROOT_DIR/tests/self_hosted/parity/llvm_leg_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="self-host-parity:persisted-read-probe"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[$LABEL] missing compiler binary: $PGY" >&2; exit 1; }

BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/expression_graph_persisted_read_probe}"
SOURCE="$ROOT_DIR/src/self_hosted/tools/expression_graph_persisted_read_probe/main.pgy"
[[ -f "$SOURCE" ]] || { echo "[$LABEL] probe source is missing" >&2; exit 1; }
mkdir -p "$BUILD_DIR"

TOOL_ARG="$(pgy_path_for_compiler "$PGY" "$SOURCE")"
# Positive default lane plus C/LLVM artifact equality where LLVM exists.
assert_llvm_leg "$LABEL" "$TOOL_ARG" "$BUILD_DIR"

PROBE_BIN="$BUILD_DIR/main_c_leg.exe"
[[ -x "$PROBE_BIN" ]] || { echo "[$LABEL] C-leg probe binary is missing" >&2; exit 1; }

REORDERED_OUT="$BUILD_DIR/reordered.out"
(cd "$ROOT_DIR" && "$PROBE_BIN" --reordered-fields >"$REORDERED_OUT" 2>&1) && \
    grep -Fq 'persisted-graph-read=one-pass-exact' "$REORDERED_OUT" || {
    echo "[$LABEL] reordered-fields lane failed" >&2
    cat "$REORDERED_OUT" >&2
    exit 1
}

# The probe owns the verdict: a negative lane exits 0 only after the reader
# rejected the document, and exits 1 itself when a malformation is accepted.
for mode in --duplicate-node-field --unknown-header-field \
    --unreachable-node --overflow-root; do
    NEG_OUT="$BUILD_DIR/neg${mode}.out"
    (cd "$ROOT_DIR" && "$PROBE_BIN" "$mode" >"$NEG_OUT" 2>&1) && \
        grep -Fq 'persisted-graph-negative=closed' "$NEG_OUT" || {
        echo "[$LABEL] negative lane did not fail closed: $mode" >&2
        cat "$NEG_OUT" >&2
        exit 1
    }
done

echo "[$LABEL] one-pass persisted reader admits canonical shapes and fails closed on malformation"
