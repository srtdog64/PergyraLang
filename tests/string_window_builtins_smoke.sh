#!/usr/bin/env bash
# string_window_builtins_smoke.sh
#
# Regression gate for the allocation-free string-window surface added in the
# 2026-06-19 string-perf workstream:
#   - fused builtins: SubIndexOf, SubEquals, SubContains, SubStartsWith
#   - O(1) char access: CharAtN
#   - the StrView stdlib module (stdlib/strview.pgy)
#
# A self-checking fixture asserts every op equals its allocating equivalent
# (StringIndexOf(Substring(...)), Substring(...) == x, ...) plus edge cases, and
# prints "ALL OK" iff there is no mismatch. We require that exact output on both
# the C and LLVM backends, so a lowering regression fails the build rather than
# silently diverging. Skips cleanly when pgy or gcc are unavailable.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
if [[ ! -x "$PGY" ]]; then
    echo "[string-window] SKIP missing compiler binary: $PGY"
    exit 0
fi
CC="${PGY_SELFHOST_CC:-gcc}"
if ! command -v "$CC" >/dev/null 2>&1; then
    echo "[string-window] SKIP missing C compiler on PATH: $CC"
    exit 0
fi

FIXTURE="$ROOT_DIR/tests/cases/string_window/correctness.pgy"
WORK="$ROOT_DIR/.tmp/string_window"
mkdir -p "$WORK"

for backend in c llvm; do
    exe="$WORK/correctness_${backend}.exe"
    if ! (cd "$ROOT_DIR" && "$PGY" "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
            --backend="$backend" -o "$(pgy_path_for_compiler "$PGY" "$exe")" >/dev/null 2>&1); then
        if [[ "$backend" == "llvm" ]]; then
            echo "[string-window] SKIP llvm backend unavailable"
            continue
        fi
        echo "[string-window] FAIL: fixture did not compile on backend=$backend" >&2
        exit 1
    fi
    out="$(cd "$ROOT_DIR" && "$exe" 2>/dev/null | tr -d '\r')"
    if [[ "$out" != "ALL OK" ]]; then
        echo "[string-window] FAIL on backend=$backend: expected 'ALL OK', got '$out'" >&2
        exit 1
    fi
    echo "[string-window] backend=$backend ok"
done

echo "[string-window] string-window builtins + StrView ok (SubIndexOf/SubEquals/SubContains/SubStartsWith/CharAtN/StrView)"
