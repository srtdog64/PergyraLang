#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-runtime-panic-codegen.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

cat > "$WORK_DIR/div_zero.pgy" <<'PGY'
func Main() -> Void {
    let lhs: Int = 10;
    let rhs: Int = 0;
    Log(lhs / rhs);
}
PGY

cat > "$WORK_DIR/mod_zero.pgy" <<'PGY'
func Main() -> Void {
    let lhs: Long = 10;
    let rhs: Long = 0;
    Log(lhs % rhs);
}
PGY

cat > "$WORK_DIR/array_index_oob.pgy" <<'PGY'
func Main() -> Void {
    let values: Array<Int> = [];
    Log(values[0]);
}
PGY

cat > "$WORK_DIR/array_set_oob.pgy" <<'PGY'
func Main() -> Void {
    let values: Array<Int> = [];
    ArraySet(values, 0, 1);
}
PGY

cat > "$WORK_DIR/array_inline_index_oob.pgy" <<'PGY'
func Words() -> Array<Int> {
    return [];
}

func Main() -> Void {
    Log(Words()[0]);
}
PGY

cat > "$WORK_DIR/slice_inline_index_oob.pgy" <<'PGY'
func Words() -> Array<Int> {
    return [1];
}

func Main() -> Void {
    Log(Words().Slice(0, 1)[1]);
}
PGY

cat > "$WORK_DIR/list_get_oob.pgy" <<'PGY'
func Main() -> Void {
    let items: List<Int> = ListNew();
    Log(ListGet(items, 0));
}
PGY

cat > "$WORK_DIR/queue_pop_empty.pgy" <<'PGY'
func Main() -> Void {
    let jobs: Queue<Int> = QueueNew();
    Log(QueuePop(jobs));
}
PGY

cat > "$WORK_DIR/map_get_missing.pgy" <<'PGY'
func Main() -> Void {
    let table: HashMap<String, Int> = MapNew();
    Log(MapGet(table, "missing"));
}
PGY

cat > "$WORK_DIR/list_set_oob.pgy" <<'PGY'
func Main() -> Void {
    let items: List<Int> = ListNew();
    ListSet(items, 0, 1);
}
PGY

cat > "$WORK_DIR/list_remove_oob.pgy" <<'PGY'
func Main() -> Void {
    let items: List<Int> = ListNew();
    ListRemove(items, 0);
}
PGY

cat > "$WORK_DIR/map_remove_missing.pgy" <<'PGY'
func Main() -> Void {
    let table: HashMap<String, Int> = MapNew();
    MapRemove(table, "missing");
}
PGY

cat > "$WORK_DIR/result_unwrap_err.pgy" <<'PGY'
func Main() -> Void {
    let r: Result<Int> = Err("boom");
    Log(Unwrap(r));
}
PGY

cat > "$WORK_DIR/option_unwrap_none.pgy" <<'PGY'
func Main() -> Void {
    let missing: Option<Int> = None();
    Log(UnwrapOption(missing));
}
PGY

expect_codegen_panic() {
    local name="$1"
    local backend="$2"
    local source="$3"
    local expected_class="${4:-divide-by-zero}"
    local output
    local rc

    set +e
    output="$("$PGY" "$source" --run --backend="$backend" -o "$WORK_DIR/$name-$backend.out" 2>&1)"
    rc=$?
    set -e

    if [[ "$rc" -eq 0 ]]; then
        echo "[runtime-panic-codegen] $name/$backend unexpectedly exited 0" >&2
        echo "$output" >&2
        exit 1
    fi
    if ! grep -Fq "[PGY PANIC]" <<<"$output"; then
        echo "[runtime-panic-codegen] $name/$backend missing panic prefix" >&2
        echo "$output" >&2
        exit 1
    fi
    if ! grep -Fq "class=$expected_class" <<<"$output"; then
        echo "[runtime-panic-codegen] $name/$backend missing $expected_class class" >&2
        echo "$output" >&2
        exit 1
    fi
}

expect_codegen_panic "div_zero" "c" "$WORK_DIR/div_zero.pgy" "divide-by-zero"
expect_codegen_panic "div_zero" "llvm" "$WORK_DIR/div_zero.pgy" "divide-by-zero"
expect_codegen_panic "mod_zero" "c" "$WORK_DIR/mod_zero.pgy" "divide-by-zero"
expect_codegen_panic "mod_zero" "llvm" "$WORK_DIR/mod_zero.pgy" "divide-by-zero"
expect_codegen_panic "array_index_oob" "c" "$WORK_DIR/array_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "array_index_oob" "llvm" "$WORK_DIR/array_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "array_set_oob" "c" "$WORK_DIR/array_set_oob.pgy" "out-of-bounds"
expect_codegen_panic "array_set_oob" "llvm" "$WORK_DIR/array_set_oob.pgy" "out-of-bounds"
expect_codegen_panic "array_inline_index_oob" "c" "$WORK_DIR/array_inline_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "array_inline_index_oob" "llvm" "$WORK_DIR/array_inline_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "slice_inline_index_oob" "c" "$WORK_DIR/slice_inline_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "slice_inline_index_oob" "llvm" "$WORK_DIR/slice_inline_index_oob.pgy" "out-of-bounds"
expect_codegen_panic "list_get_oob" "c" "$WORK_DIR/list_get_oob.pgy" "out-of-bounds"
expect_codegen_panic "list_get_oob" "llvm" "$WORK_DIR/list_get_oob.pgy" "out-of-bounds"
expect_codegen_panic "queue_pop_empty" "c" "$WORK_DIR/queue_pop_empty.pgy" "out-of-bounds"
expect_codegen_panic "queue_pop_empty" "llvm" "$WORK_DIR/queue_pop_empty.pgy" "out-of-bounds"
expect_codegen_panic "map_get_missing" "c" "$WORK_DIR/map_get_missing.pgy" "out-of-bounds"
expect_codegen_panic "map_get_missing" "llvm" "$WORK_DIR/map_get_missing.pgy" "out-of-bounds"
expect_codegen_panic "list_set_oob" "c" "$WORK_DIR/list_set_oob.pgy" "out-of-bounds"
expect_codegen_panic "list_set_oob" "llvm" "$WORK_DIR/list_set_oob.pgy" "out-of-bounds"
expect_codegen_panic "list_remove_oob" "c" "$WORK_DIR/list_remove_oob.pgy" "out-of-bounds"
expect_codegen_panic "list_remove_oob" "llvm" "$WORK_DIR/list_remove_oob.pgy" "out-of-bounds"
expect_codegen_panic "map_remove_missing" "c" "$WORK_DIR/map_remove_missing.pgy" "out-of-bounds"
expect_codegen_panic "map_remove_missing" "llvm" "$WORK_DIR/map_remove_missing.pgy" "out-of-bounds"
expect_codegen_panic "result_unwrap_err" "c" "$WORK_DIR/result_unwrap_err.pgy" "internal-invariant"
expect_codegen_panic "result_unwrap_err" "llvm" "$WORK_DIR/result_unwrap_err.pgy" "internal-invariant"
expect_codegen_panic "option_unwrap_none" "c" "$WORK_DIR/option_unwrap_none.pgy" "internal-invariant"
expect_codegen_panic "option_unwrap_none" "llvm" "$WORK_DIR/option_unwrap_none.pgy" "internal-invariant"

echo "[runtime-panic-codegen] generated C and LLVM divide-by-zero, collection out-of-bounds, and unwrap invariant panic classes are executable"
