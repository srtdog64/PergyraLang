#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

run_ok() {
    local name="$1"
    local main_file="$2"
    shift 2
    local output

    output="$("$PGY" "$main_file" --run --backend=c 2>&1)"
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[module-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[module-smoke] $name ok"
}

run_fail() {
    local name="$1"
    local expected="$2"
    local main_file="$3"
    local output

    if output="$("$PGY" "$main_file" --backend=c -o "$WORK_DIR/out" 2>&1)"; then
        echo "[module-smoke] $name unexpectedly succeeded" >&2
        echo "$output" >&2
        exit 1
    fi
    if ! grep -Fq "$expected" <<<"$output"; then
        echo "[module-smoke] $name missing expected error" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    echo "[module-smoke] $name ok"
}

mkdir -p "$WORK_DIR/explicit_export"
cat > "$WORK_DIR/explicit_export/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$WORK_DIR/explicit_export/main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.Add(2, 5));
}
EOF
run_ok "explicit_export" "$WORK_DIR/explicit_export/main.pgy" "7"

mkdir -p "$WORK_DIR/private_hidden"
cat > "$WORK_DIR/private_hidden/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$WORK_DIR/private_hidden/main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.HiddenAdd(2, 5));
}
EOF
run_fail "private_hidden" "Undefined function 'Math.HiddenAdd'" \
    "$WORK_DIR/private_hidden/main.pgy"

mkdir -p "$WORK_DIR/implicit_export"
cat > "$WORK_DIR/implicit_export/util.pgy" <<'EOF'
namespace Util {
    func Twice(n: Int) -> Int { return n + n; }
}
EOF
cat > "$WORK_DIR/implicit_export/main.pgy" <<'EOF'
import "util.pgy";
func Main() -> Void {
    Log(Util.Twice(4));
}
EOF
run_ok "implicit_export" "$WORK_DIR/implicit_export/main.pgy" "8"

mkdir -p "$WORK_DIR/circular"
cat > "$WORK_DIR/circular/a.pgy" <<'EOF'
import "b.pgy";
func A() -> Int { return 1; }
EOF
cat > "$WORK_DIR/circular/b.pgy" <<'EOF'
import "a.pgy";
func B() -> Int { return 2; }
EOF
cat > "$WORK_DIR/circular/main.pgy" <<'EOF'
import "a.pgy";
func Main() -> Void { Log(1); }
EOF
run_fail "circular_import" "circular import detected" \
    "$WORK_DIR/circular/main.pgy"
