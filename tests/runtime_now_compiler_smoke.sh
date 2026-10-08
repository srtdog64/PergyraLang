#!/usr/bin/env bash
# Compiler type/ABI consumer; use a freshly built binary for source claims.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:?PGY_BIN must name the compiler under test}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$ROOT_DIR"
for backend in c llvm; do
    "$PGY" --native-pipeline --backend="$backend" --opt=dev \
        tests/fixtures/runtime_now_long.pgy -o "$work/now-$backend" \
        >"$work/$backend.build.out" 2>"$work/$backend.build.err" || {
            cat "$work/$backend.build.err" >&2; exit 1;
        }
    env -u PGY_CAP_GRANT "$work/now-$backend" >"$work/$backend.out"
    printf 'true\ntrue\n' >"$work/expected"
    cmp "$work/expected" "$work/$backend.out"
done
"$PGY" --native-pipeline --opt=dev --emit-llvm \
    tests/fixtures/runtime_now_long.pgy -o "$work/now.ll" >"$work/llvm.out" 2>"$work/llvm.err"
grep -Eq '^(declare|define).*i64 @pgy_now_ms\(' "$work/now.ll"
if grep -Eq '^(declare|define).*i32 @pgy_now_ms\(' "$work/now.ll"; then
    echo '[runtime-now-compiler] retired Int ABI returned' >&2; exit 1
fi
echo '[runtime-now-compiler] fresh compiler C/LLVM run + Long ABI PASS'
