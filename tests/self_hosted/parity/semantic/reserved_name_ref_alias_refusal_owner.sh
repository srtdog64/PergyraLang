#!/usr/bin/env bash
# Two programs native refuses that the default route (self-hosted front end)
# used to compile and run:
#   - a reserved language word as a name. The self-hosted parser read the
#     registry only for `let` names and fields, so `func F(local: Int)` and a
#     match binding `case Full(local):` compiled. Native refuses both at parse.
#   - `let first = values` for a `ref` formal `values`. Native refuses the new
#     binding in semantic (a borrowed ref value cannot escape into a new
#     binding) for every type that is not copy-only. The self-host admitted it
#     for Array<Int> and refused Array<String> only at codegen.
# Each refused program fails on native C/LLVM and the default C/LLVM route
# with the stated reason and no artifact. Each neighbour, the same program
# without the escape or with a non-reserved name, compiles and prints the
# same output on all four legs.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="reserved-name-ref-alias-refusal"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/reserved_name_ref_alias_refusal"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

cat > "$WORK_DIR/reserved_param.pgy" <<'PGY'
func Pick(local: Int) -> Int { return local + 1; }
func Main() -> Void { Log(Pick(2)); }
PGY
cat > "$WORK_DIR/reserved_match_binding.pgy" <<'PGY'
enum Box { Full(Int), Empty }
func Peek(box: Box) -> Int {
    match box {
        case Full(local): return local;
        default: return 0;
    }
    return 0;
}
func Main() -> Void { Log(Peek(Box.Full(4))); }
PGY
cat > "$WORK_DIR/reserved_neighbour.pgy" <<'PGY'
enum Box { Full(Int), Empty }
func Pick(local_count: Int, state: Int) -> Int { return local_count + state; }
func Peek(box: Box) -> Int {
    match box {
        case Full(found): return found;
        default: return 0;
    }
    return 0;
}
func Main() -> Void { Log(Pick(2, 1)); Log(Peek(Box.Full(4))); }
PGY
cat > "$WORK_DIR/ref_array_alias.pgy" <<'PGY'
func First(ref values: Array<Int>) -> Int {
    let first = values;
    return ArrayLength(first);
}
func Main() -> Void { let xs: Array<Int> = [1, 2]; Log(First(xs)); }
PGY
cat > "$WORK_DIR/ref_field_alias.pgy" <<'PGY'
struct Table { count: Int; ids: Array<Int>; }
func Size(ref table: Table) -> Int {
    let ids: Array<Int> = table.ids;
    return ArrayLength(ids) + table.count;
}
func Main() -> Void { let table: Table = Table(1, [4, 5]); Log(Size(table)); }
PGY
cat > "$WORK_DIR/ref_neighbour.pgy" <<'PGY'
struct Table { count: Int; name: String; ids: Array<Int>; }
func Size(ref table: Table, ref values: Array<Int>, ref names: Array<String>) -> Int {
    let count: Int = table.count;
    let name: String = table.name;
    let head: Int = values[0];
    let label: String = names[0];
    return count + StringLength(name) + head + StringLength(label) + ArrayLength(values);
}
func Main() -> Void {
    let table: Table = Table(1, "ab", [4]);
    let values: Array<Int> = [7, 8];
    let names: Array<String> = ["xyz"];
    Log(Size(table, values, names));
}
PGY

LEGS=(native-c native-llvm default-c default-llvm)
flags_for() {
    case "$1" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        native-llvm) flags=(--native-pipeline --backend=llvm) ;;
        default-c) flags=(--backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
    esac
}

# refused NAME NATIVE_TEXT DEFAULT_CODE [DEFAULT_FACT]
refused() {
    local name="$1" native_text="$2" code="$3" fact="${4:-}" leg log out_rel
    for leg in "${LEGS[@]}"; do
        flags_for "$leg"
        out_rel="$WORK_REL/$name-$leg.exe"
        log="$WORK_DIR/$name-$leg.log"
        if (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" "${flags[@]}" -o "$out_rel") >"$log" 2>&1; then
            fail "$leg accepted $name"
        fi
        [[ ! -e "$ROOT_DIR/$out_rel" ]] || fail "$leg published an artifact for $name"
        case "$leg" in
            native-*) grep -Fq -- "$native_text" "$log" ||
                { cat "$log" >&2; fail "$leg refused $name without: $native_text"; } ;;
            default-*)
                tr -d '\r' <"$log" | grep -Fxq "Code: $code" ||
                    { cat "$log" >&2; fail "$leg refused $name without code $code"; }
                [[ -z "$fact" ]] || tr -d '\r' <"$log" | grep -Fxq -- "$fact" ||
                    { cat "$log" >&2; fail "$leg refused $name without fact $fact"; } ;;
        esac
    done
}

# admitted NAME EXPECTED_OUTPUT
admitted() {
    local name="$1" expected="$2" leg out_rel
    printf '%s\n' "$expected" >"$WORK_DIR/$name.expected"
    for leg in "${LEGS[@]}"; do
        flags_for "$leg"
        out_rel="$WORK_REL/$name-$leg.exe"
        (cd "$ROOT_DIR" && env -u PGY_NATIVE_PIPELINE PGY_SELF_DRIVER_BIN="$DRIVER" \
            "$PGY" "$WORK_REL/$name.pgy" "${flags[@]}" -o "$out_rel") \
            >"$WORK_DIR/$name-$leg.log" 2>&1 ||
            { cat "$WORK_DIR/$name-$leg.log" >&2; fail "$leg refused $name"; }
        "$ROOT_DIR/$out_rel" </dev/null 2>&1 | tr -d '\r' >"$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name exited non-zero"
        cmp -s "$WORK_DIR/$name.expected" "$WORK_DIR/$name-$leg.out" ||
            fail "$leg $name printed $(tr '\n' ' ' <"$WORK_DIR/$name-$leg.out")"
    done
}

refused reserved_param "'local' is a reserved language word" binding_name_reserved
refused reserved_match_binding "'local' is a reserved language word" reserved_word_not_a_value
admitted reserved_neighbour $'3\n4'
refused ref_array_alias "cannot escape into new binding 'first'" borrow_boundary_escape \
    "- boundary: new_binding"
refused ref_field_alias "cannot escape into new binding 'ids'" borrow_boundary_escape \
    "- boundary: new_binding"
admitted ref_neighbour 15

echo "[$LABEL] 4 programs refused and 2 neighbours admitted on native C/LLVM and default C/LLVM: PASS"
