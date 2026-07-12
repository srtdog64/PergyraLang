#!/usr/bin/env bash
#
# parallel_join_smoke.sh — the join-form parallel block, rungs 0+1
# (docs/181 SS1): `parallel (x in xs)` / `parallel (i in lo..hi)`.
#
#   - parallel_join_collection (compare corpus) runs on BOTH backends
#     and prints 204 (sum of squares 1..8 through a channel -- the sum is
#     order-independent, so the output is deterministic)
#   - parallel_join_index (compare corpus, R1) prints 164 then 328:
#     in-place element writes buckets[i] under index-disjointness
#   - parallel_join_expr (compare corpus, R2) prints 204/182/2/72:
#     `let rs = parallel ... { give <expr>; };` collects per-task
#     results into an Array<R> in index order
#   - reject_no_binding    binding-less sketch form        -> reject
#   - reject_base_in_body  body touches the collection     -> reject
#   - reject_elem_write    element binding written         -> reject
#   - reject_outer_write   outer binding written           -> reject
#     (replicated arms: no single-writer evidence can hold)
#   - reject_index_nonbinding      buckets[0] in index form -> reject
#   - reject_index_whole_array     ArrayPush in index form  -> reject
#   - reject_collection_array_write ys[0]=x in element mode -> reject
#   - reject_collection_array_read  ys[0] read in element mode -> reject
#   - reject_expr_missing_give        expression form, no give   -> reject
#   - reject_give_in_statement_form   give without a result sink -> reject
#   - reject_give_not_last            give before other stmts    -> reject
#   - parallel_join_reduce (compare corpus, R4) prints
#     204/24/3/-2/0/1: `join with sum|product|min|max` folds gives in
#     index order (Int lanes on the checked-arith exports; the Float row
#     pins the fixed left fold)
#   - panic_reduce_empty_min          min over empty fan-out -> runtime
#     fail-closed panic (class=out-of-bounds, shared reason string on
#     both backends; identity extremes must never leak)
#   - reject_reduce_unknown_op        combinator set is closed    -> reject
#   - reject_reduce_bool_give         reduce folds numbers only   -> reject
#   - reject_reduce_statement_form    scalar result must be bound -> reject
#   - parallel_join_stencil (compare corpus, R5) prints
#     33/33/33/0/0/100/103: Jacobi double buffering -- an unwritten
#     captured array may be read at ANY index (snapshot-read fact);
#     writes keep the [i] discipline on a different array
#   - panic_stencil_alias             `let b = a;` handle copy makes the
#     read snapshot alias the written array -> fan-out entry panic
#     (class=authority-mismatch, same reason on both backends)
#   - reject_stencil_inplace          in-place neighbor stencil    -> reject
#     (only the Jacobi shape is admitted)

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
INDEX_SRC="$ROOT_DIR/tests/cases/backend_compare/parallel_join_index/main.pgy"
EXPR_SRC="$ROOT_DIR/tests/cases/backend_compare/parallel_join_expr/main.pgy"
REDUCE_SRC="$ROOT_DIR/tests/cases/backend_compare/parallel_join_reduce/main.pgy"
STENCIL_SRC="$ROOT_DIR/tests/cases/backend_compare/parallel_join_stencil/main.pgy"
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
    local backend="$1" src="$2" name="$3" want="$4"
    local exe="run_${backend}_${name}.exe"
    compile "$backend" "$src" "$exe" ||
        fail "$backend/$name must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    local got
    got="$("$OUT_DIR/$exe" | tr -d '\r')" || fail "$backend/$name crashed at runtime"
    [ "$got" = "$want" ] || fail "$backend/$name printed '$got', expected '$want'"
}

# R4 runtime fail-closed witness: the binary must die with the shared
# panic reason, never print a fake min, never exit clean.
expect_panics() {
    local backend="$1" src="$2" name="$3" needle="$4"
    local exe="pan_${backend}_${name}.exe"
    compile "$backend" "$src" "$exe" ||
        fail "$backend/$name must compile: $(tail -2 "$OUT_DIR/$exe.log")"
    set +e
    "$OUT_DIR/$exe" >"$OUT_DIR/$exe.out" 2>&1
    local rc=$?
    set -e
    [ "$rc" -ne 0 ] || fail "$backend/$name exited clean but must panic"
    grep -Fq "$needle" "$OUT_DIR/$exe.out" ||
        fail "$backend/$name died without the shared panic reason: $needle"
}

# C-only platforms (macOS CI, Windows C-only) narrow the voice set via env.
BACKENDS="${PGY_PARALLEL_JOIN_BACKENDS:-c llvm}"
for backend in $BACKENDS; do
    expect_runs "$backend" "$ACCEPT_SRC" "join" "204"
    expect_runs "$backend" "$INDEX_SRC" "join_index" "$(printf '164\n328')"
    expect_runs "$backend" "$EXPR_SRC" "join_expr" "$(printf '204\n182\n2\n72')"
    expect_runs "$backend" "$REDUCE_SRC" "join_reduce" \
        "$(printf '204\n24\n3\n-2\n0\n1')"
    expect_panics "$backend" "$FIXTURES/panic_reduce_empty_min.pgy" \
        "reduce_empty_min" \
        "min/max reduce over an empty parallel join range"
    expect_runs "$backend" "$STENCIL_SRC" "join_stencil" \
        "$(printf '33\n33\n33\n0\n0\n100\n103')"
    expect_panics "$backend" "$FIXTURES/panic_stencil_alias.pgy" \
        "stencil_alias" \
        "read-only capture aliases an index-written array"
done

expect_reject reject_no_binding.pgy   "requires an element binding"
expect_reject reject_base_in_body.pgy "cannot reference the collection"
expect_reject reject_elem_write.pgy   "element binding is read-only"
expect_reject reject_outer_write.pgy  "join arms are replicated"
expect_reject reject_index_nonbinding.pgy      "outside the index-disjoint form"
expect_reject reject_index_whole_array.pgy     "outside the index-disjoint form"
expect_reject reject_collection_array_write.pgy "join arms are replicated"
expect_reject reject_collection_array_read.pgy  "cannot capture mutable collection"
expect_reject reject_expr_missing_give.pgy      "requires a final 'give'"
expect_reject reject_give_in_statement_form.pgy "give names a per-task result"
expect_reject reject_give_not_last.pgy          "must be the final statement"
expect_reject reject_reduce_unknown_op.pgy \
    "Expected join mode 'all', 'sum', 'product', 'min', or 'max'"
expect_reject reject_reduce_bool_give.pgy       "folds numeric gives only"
expect_reject reject_reduce_statement_form.pgy  "produces a value; bind it"
expect_reject reject_stencil_inplace.pgy        "outside the index-disjoint form"

echo "[parallel-join] rungs 0+1+2+R4+R5 admitted (204 + 164/328 + 204/182/2/72 + reduce 204/24/3/-2/0/1 + stencil 33x3/0/0/100/103 + 2 runtime panic witnesses on: $BACKENDS); 15 reject shapes fail closed"
