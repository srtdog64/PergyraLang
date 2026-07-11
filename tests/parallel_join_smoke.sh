#!/usr/bin/env bash
#
# parallel_join_smoke.sh — the join-form parallel block, rung 0
# (docs/181 SS1): `parallel (x in xs) [join with all] { body }`.
#
#   - parallel_join_collection (compare corpus) runs on BOTH backends
#     and prints 204 (sum of squares 1..8 through a channel -- the sum is
#     order-independent, so the output is deterministic)
#   - reject_no_binding    binding-less sketch form        -> reject
#   - reject_base_in_body  body touches the collection     -> reject
#   - reject_elem_write    element binding written         -> reject
#   - reject_outer_write   outer binding written           -> reject
#     (replicated arms: no single-writer evidence can hold)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe ]] && pgy_binary_expects_windows_paths "${PGY}.exe"; then
    PGY="${PGY}.exe"
fi
[[ -x "$PGY" ]] || { echo "[parallel-join] SKIP: pgy binary not found at $PGY" >&2; exit 0; }

FIXTURES="$ROOT_DIR/tests/cases/parallel_join"
ACCEPT_SRC="$ROOT_DIR/tests/cases/backend_compare/parallel_join_collection/main.pgy"
OUT_DIR="$(mktemp -d)"
trap 'rm -rf "$OUT_DIR"' EXIT

fail() { echo "[parallel-join] FAIL: $*" >&2; exit 1; }

compile() {
    local backend="$1" src_path="$2" out_name="$3"
    local src out rc
    src="$(pgy_path_for_compiler "$PGY" "$src_path")"
    out="$(pgy_path_for_compiler "$PGY" "$OUT_DIR/$out_name")"
    set +e
    (cd "$ROOT_DIR" && "$PGY" "$src" --backend="$backend" -o "$out") \
        >"$OUT_DIR/$out_name.log" 2>&1
    rc=$?
    set -e
    return $rc
}

expect_reject() {
    # The admission is parse/semantic-layer, so one backend's voice suffices.
    local fixture="$1" needle="$2"
    if compile c "$FIXTURES/$fixture" "rej_${fixture%.pgy}.exe"; then
        fail "$fixture compiled but must fail closed"
    fi
    grep -Fq "$needle" "$OUT_DIR/rej_${fixture%.pgy}.exe.log" ||
        fail "$fixture failed without the expected diagnostic: $needle"
}

expect_runs() {
    local backend="$1" want="$2"
    local exe="run_${backend}_join.exe"
    compile "$backend" "$ACCEPT_SRC" "$exe" ||
        fail "$backend/parallel_join_collection must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe" | tr -d '\r')" || fail "$backend crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend printed '$got', expected '$want'"
}

# C-only platforms (macOS CI, Windows C-only) narrow the voice set via env.
BACKENDS="${PGY_PARALLEL_JOIN_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_runs "$backend" "204"
done

expect_reject reject_no_binding.pgy   "requires an element binding"
expect_reject reject_base_in_body.pgy "cannot reference the collection"
expect_reject reject_elem_write.pgy   "element binding is read-only"
expect_reject reject_outer_write.pgy  "join arms are replicated"

echo "[parallel-join] rung 0 admitted (204 on: $BACKENDS); no-binding/base-in-body/elem-write/outer-write all fail closed"
