#!/usr/bin/env bash
#
# nested_array_smoke.sh — one-level nested arrays Array<Array<scalar>> must work
# identically on the C and LLVM backends (declaration, nested literal, single
# and chained index, ArrayLength, nested for-in), and nesting deeper than two
# levels (or with an unsupported element type) must FAIL CLOSED with a clean
# semantic error rather than compiling and breaking at the codegen layer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY_BIN="${PGY_BIN:-$ROOT_DIR/bin/pgy.exe}"
[ -x "$PGY_BIN" ] || PGY_BIN="$ROOT_DIR/bin/pgy"
WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/pgy-nested-array.XXXXXX")"
trap 'rm -rf "$WORK_DIR"' EXIT

RUNTIME_BC="${PGY_RUNTIME_BC:-$ROOT_DIR/src/runtime/pgy_runtime_lib.bc}"

fail() { echo "[nested-array] FAIL: $*" >&2; exit 1; }

run_backend() {
    # run_backend <backend> <source> -> prints program stdout (Log/Print lines)
    local backend="$1" src="$2" out
    if [ "$backend" = "llvm" ]; then
        out="$(PGY_RUNTIME_BC="$RUNTIME_BC" "$PGY_BIN" "$src" \
            --backend=llvm --run 2>&1)" || fail "$backend run failed: $out"
    else
        out="$("$PGY_BIN" "$src" --backend=c --run 2>&1)" \
            || fail "$backend run failed: $out"
    fi
    printf '%s\n' "$out" | tr -d '\r' \
        | grep -vE 'error\(s\)|compiled' | grep -vE '^[[:space:]]*$' || true
}

# ---- depth-2 works identically on both backends --------------------------
ok_src="$WORK_DIR/ok.pgy"
cat > "$ok_src" <<'EOF'
func Main() -> Void {
    let a: Array<Array<Int>> = [[1, 2], [3, 4]];
    Log(ArrayLength(a));
    let row: Array<Int> = a[1];
    Log(ArrayLength(row));
    Log(a[1][0]);
    let s: Int = 0;
    for r in a {
        for x in r {
            s = s + x;
        }
    }
    Log(s);
}
EOF

c_out="$(run_backend c "$ok_src")"
llvm_out="$(run_backend llvm "$ok_src")"
[ "$c_out" = "$llvm_out" ] \
    || fail "backend output mismatch:
C:
$c_out
LLVM:
$llvm_out"

want=$'2\n2\n3\n10'
[ "$c_out" = "$want" ] || fail "depth-2 output = '$c_out', expected '$want'"
echo "[nested-array] depth-2 decl/index/chained/for-in parity ok (2 2 3 10)"

# ---- depth-3 fails closed with a clean semantic error (both backends) -----
bad_src="$WORK_DIR/depth3.pgy"
cat > "$bad_src" <<'EOF'
func Main() -> Void {
    let a: Array<Array<Array<Int>>> = [[[1]]];
    Log(ArrayLength(a));
}
EOF

assert_fail_closed() {
    local backend="$1" out rc
    if [ "$backend" = "llvm" ]; then
        out="$(PGY_RUNTIME_BC="$RUNTIME_BC" "$PGY_BIN" "$bad_src" \
            --backend=llvm --run 2>&1)" && rc=0 || rc=$?
    else
        out="$("$PGY_BIN" "$bad_src" --backend=c --run 2>&1)" && rc=0 || rc=$?
    fi
    [ "$rc" -ne 0 ] || fail "$backend depth-3 compiled instead of failing closed"
    echo "$out" | grep -qiE 'nested array nesting deeper than two levels' \
        || fail "$backend depth-3 lacked the nested-array semantic diagnostic: $out"
    echo "$out" | grep -qiE 'gcc|ld returned|LLVM verify|Native compilation failed' \
        && fail "$backend depth-3 reached the codegen layer (not fail-closed): $out"
    echo "[nested-array] $backend depth-3 -> clean semantic error (fail-closed)"
}

assert_fail_closed c
assert_fail_closed llvm

echo "[nested-array] PASS — depth-2 works on both backends, depth-3 fails closed"
