#!/usr/bin/env bash
#
# memory_safety_failclosed_smoke.sh — security evidence.
#
# Pergyra has no raw pointers and no pointer arithmetic; array indexing is
# bounds-checked and integer division is checked, all fail-closed (a panic, never
# memory corruption / a wrong result silently used). This gate compiles and runs
# counterexamples that each attempt a memory- or arithmetic-safety violation and
# asserts every one PANICS instead of corrupting memory — on both backends.
#
# Each maps to a real-world memory-corruption class:
#   oob_write -> the OOB-write-overwrites-a-pointer class (e.g. FFmpeg RASC,
#                objdump DLX, VLC VP9): in Pergyra the write panics, it cannot
#                reach an adjacent heap object.
#   oob_read  -> the OOB-read-to-leak class (e.g. PHP StreamBucket HashTable leak).
#   div_zero / mod_zero -> checked integer arithmetic (operator form).
#   checked_mul_overflow / checked_add_overflow -> the surface CheckedMul/CheckedAdd
#                builtins: a signed *-overflow / +-overflow (e.g. the malloc(n*size)
#                integer-overflow-to-undersized-allocation class) panics instead of
#                wrapping to a smaller-than-expected value.
#   lifecycle_wrong_state -> the use-after-free / use-in-wrong-state class: a
#                domain-lifecycle operation applied in a state that forbids it
#                (Capture on a non-Authorized Payment) panics instead of silently
#                mutating an invalid-state object.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"

fail() { echo "[mem-safety-failclosed] FAIL: $*" >&2; exit 1; }

if { [ ! -x "$PGY" ] && [ ! -f "$PGY" ]; } || ! pgy_binary_is_runnable_here "$PGY"; then
    echo "[mem-safety-failclosed] SKIP: compiler binary not runnable"
    exit 0
fi

backends="c"
if "$PGY" --help 2>/dev/null | grep -qiE 'llvm'; then backends="c llvm"; fi

WORK="$(mktemp -d)"
cases="oob_write:out-of-bounds oob_read:out-of-bounds div_zero:divide-by-zero mod_zero:divide-by-zero checked_mul_overflow:arithmetic-overflow checked_add_overflow:arithmetic-overflow lifecycle_wrong_state:invalid-lifecycle-state"

for be in $backends; do
    for spec in $cases; do
        name="${spec%%:*}"; want_class="${spec##*:}"
        src="$ROOT_DIR/tests/security/$name.pgy"
        [ -f "$src" ] || fail "missing counterexample: $src"
        exe="$WORK/${name}_${be}"
        "$PGY" "$(pgy_path_for_compiler "$PGY" "$src")" --backend="$be" -o "$exe" \
            >"$WORK/c.out" 2>"$WORK/c.err" || { cat "$WORK/c.err" >&2; fail "$name did not compile (backend=$be)"; }

        out="$("$exe" 2>&1)" && rc=0 || rc=$?
        if [ "$rc" -eq 0 ]; then
            fail "$name ran to completion (backend=$be) — the violation was NOT fail-closed. Output: $out"
        fi
        echo "$out" | grep -qi 'PGY PANIC' \
            || fail "$name exited $rc but without a PGY PANIC (backend=$be): $out"
        echo "$out" | grep -qi "class=$want_class" \
            || fail "$name panicked but not class=$want_class (backend=$be): $out"
        echo "[mem-safety-failclosed] backend=$be $name -> fail-closed (class=$want_class)"
    done
done

# ---- checked-arith surface arg typing rejects at compile time --------------
# Regression: CheckedAdd/CheckedMul lower to the i32 overflow-checked helpers,
# so non-Int operands must be a clean SEMANTIC rejection on both backends. A
# shared Min/Max-style promoting check once let Float through: the C backend
# silently truncated and LLVM failed verification (accepted-then-broken).
reject_src="$WORK/checked_arith_float_arg.pgy"
cat > "$reject_src" <<'EOF'
func Main() -> Void {
    Log(ToString(CheckedAdd(1.5, 2.5)));
}
EOF
for be in $backends; do
    out="$("$PGY" "$(pgy_path_for_compiler "$PGY" "$reject_src")" --backend="$be" -o "$WORK/rej_$be" 2>&1)" && rc=0 || rc=$?
    [ "$rc" -ne 0 ] \
        || fail "CheckedAdd(Float) compiled (backend=$be) — must be rejected at semantic"
    echo "$out" | grep -qiE 'Type mismatch|cannot assign' \
        || fail "CheckedAdd(Float) failed without the type diagnostic (backend=$be): $out"
    echo "$out" | grep -qiE 'LLVM verify|gcc|In function' \
        && fail "CheckedAdd(Float) reached the codegen layer (backend=$be): $out"
    echo "[mem-safety-failclosed] backend=$be checked_arith_float_arg -> clean semantic reject"
done

echo "[mem-safety-failclosed] PASS — no raw pointers, every violation fail-closed"
