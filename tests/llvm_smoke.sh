#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="$ROOT_DIR/bin/pgy"

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

run_case() {
    local name="$1"
    local file="$2"
    shift 2
    local output

    output="$("$PGY" "$file" --run --backend=llvm 2>&1)"
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[llvm-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[llvm-smoke] $name ok"
}

cat > "$TMPDIR/loop_break_continue.pgy" <<'EOF'
func Main() -> Void {
    let sum: Int = 0;
    for i in 0..6 {
        if i == 2 { continue; }
        if i == 5 { break; }
        sum = sum + i;
    }
    Log(sum);
}
EOF
run_case "break_continue" "$TMPDIR/loop_break_continue.pgy" "8"

cat > "$TMPDIR/array_enum.pgy" <<'EOF'
enum Color { Red, Green, Blue }
func Main() -> Void {
    let values: Array<Int> = [1, 2, 3];
    let c: Color = Red;
    Log(values[1]);
    if c == Color.Red { Log(9); }
}
EOF
run_case "array_enum" "$TMPDIR/array_enum.pgy" "2" "9"

DATA_FILE="$TMPDIR/io.txt"
cat > "$TMPDIR/string_io.pgy" <<EOF
func Main() -> Void {
    let s: String = Concat(Upper(Trim("  hi")), Lower(" THERE"));
    Log(StringLength(s));
    Log(Contains(s, "HI"));
    WriteFile("$DATA_FILE", Replace(s, "HI", "BYE"));
    let out: String = ReadFile("$DATA_FILE");
    Log(out);
}
EOF
run_case "string_io" "$TMPDIR/string_io.pgy" "8" "true" "BYE there"

cat > "$TMPDIR/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$TMPDIR/module_main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.Add(2, 5));
}
EOF
run_case "namespace_export_import" "$TMPDIR/module_main.pgy" "7"

cat > "$TMPDIR/operator_overload.pgy" <<'EOF'
class Vec2 {
    let x: Int;
    let y: Int;
}

func operator_add_Vec2(a: Vec2, b: Vec2) -> Vec2 {
    return a;
}

func Main() -> Void {
    let a: Vec2 = Vec2(1, 2);
    let b: Vec2 = Vec2(3, 4);
    let c: Vec2 = a + b;
    Log(c.x);
}
EOF
run_case "operator_overload" "$TMPDIR/operator_overload.pgy" "1"

cat > "$TMPDIR/role_operator_overload.pgy" <<'EOF'
ability Arithmetic {
    func Add(other: Int) -> Int;
}

role IntMath for Int {
    impl ability Arithmetic {
        func Add(other: Int) -> Int {
            return 123;
        }
    }
}

func Main() -> Void {
    let a: Int = 1;
    let b: Int = 2;
    Log(a + b);
}
EOF
run_case "role_operator_overload" "$TMPDIR/role_operator_overload.pgy" "123"

cat > "$TMPDIR/qubit_slot.pgy" <<'EOF'
func Main() -> Void {
    let q: QubitSlot = ClaimQubit();
    Log(QubitState(q));
    let first: Int = Measure(q);
    let second: Int = Measure(q);
    Log(first);
    Log(second);

    let a: QubitSlot = ClaimQubit();
    let b: QubitSlot = ClaimQubit();
    Entangle(a, b);
    let ra: Int = Measure(a);
    let rb: Int = Measure(b);
    Log(ra);
    Log(rb);

    ReleaseQubit(q);
    ReleaseQubit(a);
    ReleaseQubit(b);
}
EOF
run_case "qubit_slot" "$TMPDIR/qubit_slot.pgy" "2"
