#!/usr/bin/env bash
# The admitted parameter signature, not source literal suffixes, owns call ABI.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:?PGY_BIN must name the compiler under test}"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cd "$ROOT_DIR"
printf 'true\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\ntrue\n' >"$work/expected"
for backend in c llvm; do
    "$PGY" --native-pipeline --backend="$backend" --opt=dev \
        tests/fixtures/call_scalar_widening.pgy -o "$work/widen-$backend" \
        >"$work/$backend.build.out" 2>"$work/$backend.build.err" || {
            cat "$work/$backend.build.err" >&2; exit 1;
        }
    "$work/widen-$backend" >"$work/$backend.out"
    cmp "$work/expected" "$work/$backend.out"
    for rejected in narrowing incompatible; do
        output="$work/$backend-$rejected"
        if "$PGY" --native-pipeline --backend="$backend" --opt=dev --error-format=json \
            "tests/fixtures/call_scalar_${rejected}_rejected.pgy" -o "$output" \
            >"$output.out" 2>"$output.err"; then
            echo "[call-scalar-widening] $backend admitted $rejected" >&2; exit 1
        fi
        grep -Eq 'PGY_SEM_(ARG_TYPE_MISMATCH|TYPE_MISMATCH)' "$output.err"
        [[ ! -e "$output" ]]
    done
done
echo '[call-scalar-widening] C/LLVM signed and Float widening, call routes and refusal PASS'
