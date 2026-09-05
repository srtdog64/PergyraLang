#!/usr/bin/env bash
# Subject of this gate:
#   Native immutable object versus immutable-field struct substitution.
# This is a bounded source rewrite, not deletion of a compiler concept.
# Installed source-MIR currently omits both write rejections; do not make
# that acceptance an expected-success test or claim self-host closure here.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT/bin/pgy}")"
LABEL="concept-nominal-substitution"
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
WORK="$ROOT/.tmp/self_hosted/concept_semantics_20260905/nominal_retained"
mkdir -p "$WORK"
fail() { echo "[$LABEL] $*" >&2; exit 1; }
printf '7\n' > "$WORK/expected"
for backend in c llvm; do
    for stem in object_read struct_read; do
        binary="$WORK/$stem-$backend.exe"
        "$PGY" --native-pipeline \
            "$ROOT/tests/concept_semantics/nominal/$stem.pgy" \
            "--backend=$backend" -o "$binary" \
            > "$WORK/$stem-$backend.out" 2> "$WORK/$stem-$backend.err" ||
            fail "$stem $backend compile failed"
        "$binary" | tr -d '\r' > "$WORK/$stem-$backend.run"
        cmp -s "$WORK/expected" "$WORK/$stem-$backend.run" ||
            fail "$stem $backend changed the read result"
    done
done
for stem in object_write_rejected struct_write_rejected; do
    if "$PGY" --native-pipeline \
        "$ROOT/tests/concept_semantics/nominal/$stem.pgy" \
        --mir-json --error-format=json \
        > "$WORK/$stem.out" 2> "$WORK/$stem.err"; then
        fail "$stem accepted an immutable-field write"
    fi
    [[ ! -s "$WORK/$stem.out" ]] || fail "$stem published MIR"
    grep -Fq '"code":"PGY_SEM_IMMUTABLE_FIELD_WRITE"' \
        "$WORK/$stem.err" || fail "$stem lost the owned diagnostic"
done
echo "[$LABEL] native C/LLVM rewrite parity and two immutable-write rejections: PASS"
