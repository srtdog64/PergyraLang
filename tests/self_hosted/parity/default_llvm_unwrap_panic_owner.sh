#!/usr/bin/env bash
# A failed unwrap on the default LLVM route reports the runtime panic.
# The direct-MIR LLVM helpers for UnwrapOption, Unwrap and UnwrapErr called
# @abort directly, so the program died with no panic line where native C
# prints `[PGY PANIC] ... class=internal-invariant reason=...` (red-team audit
# M10). Each failed path now calls the registered runtime panic export.
# - Every helper family the route lowers from source is exercised:
#   Option<Int>, Option<Bool>, Option<String>, Option<record> and Unwrap on a
#   Result<Int> Err. UnwrapErr is reached only through match lowering, after
#   the tag test, so no source program can fail it.
# - Native C is the oracle: the same exit status, class and reason.
# - A failing unwrap helper is emitted only for a program that calls it.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="default-llvm-unwrap-panic"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/self_hosted/default_llvm_unwrap_panic"
WORK_DIR="$ROOT_DIR/$WORK_REL"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"

write_case() {
    local name="$1" type="$2" some="$3" pick="$4" call="$5"
    cat >"$WORK_DIR/$name.pgy" <<EOF
func Pick(flag: Bool) -> $type {
    if flag { return $some; }
    return $pick;
}
func Main() -> Void {
    Log(1);
    let picked: $type = Pick(false);
    $call
}
EOF
}

write_case option-int "Option<Int>" "Some(5)" "None" "Log(UnwrapOption(picked));"
write_case option-bool "Option<Bool>" "Some(true)" "None" "Log(UnwrapOption(picked));"
write_case option-string "Option<String>" 'Some("five")' "None" "Log(UnwrapOption(picked));"
write_case result-err "Result<Int>" "Ok(5)" 'Err("bad")' "Log(Unwrap(picked));"
cat >"$WORK_DIR/option-record.pgy" <<'EOF'
struct Point {
    x: Int;
    y: Int;
}
func Pick(flag: Bool) -> Option<Point> {
    if flag { return Some(Point { x: 1, y: 2 }); }
    return None;
}
func Main() -> Void {
    Log(1);
    let picked: Option<Point> = Pick(false);
    let point: Point = UnwrapOption(picked);
    Log(point.x);
}
EOF

# run_leg CASE LEG: compile, run, and record the panic line without its
# file:line position, which names each leg's own runtime source.
run_leg() {
    local name="$1" leg="$2" flags=()
    case "$leg" in
        native-c) flags=(--native-pipeline --backend=c) ;;
        default-llvm) flags=(--backend=llvm) ;;
        *) fail "unknown leg $leg" ;;
    esac
    local base="$WORK_DIR/$name-$leg"
    (cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
        "$PGY" "$WORK_REL/$name.pgy" "${flags[@]}" -o "$WORK_REL/$name-$leg.exe") \
        >"$base.log" 2>&1 ||
        { cat "$base.log" >&2; fail "$leg did not compile $name"; }
    set +e
    # The subshell keeps bash's "Aborted (core dumped)" notice out of the log.
    ( "$base.exe" </dev/null >"$base.out" 2>"$base.err"; exit $? ) 2>/dev/null
    local rc=$?
    set -e
    echo "$rc" >"$base.rc"
    grep -E '^\[PGY PANIC\] ' "$base.err" | sed -E 's/^\[PGY PANIC\] [^ ]+ /[PGY PANIC] /' \
        >"$base.panic" || true
}

check_case() {
    local name="$1" reason="$2"
    run_leg "$name" native-c
    run_leg "$name" default-llvm
    local oracle="$WORK_DIR/$name-native-c" leg="$WORK_DIR/$name-default-llvm"
    [[ "$(cat "$oracle.rc")" != 0 ]] || fail "native C did not stop on $name"
    grep -Fqx "[PGY PANIC] class=internal-invariant reason=$reason" "$oracle.panic" ||
        { cat "$oracle.err" >&2; fail "native C oracle reports another panic for $name"; }
    cmp -s "$oracle.rc" "$leg.rc" ||
        fail "default LLVM exit status $(cat "$leg.rc") differs from native C $(cat "$oracle.rc") on $name"
    cmp -s "$oracle.panic" "$leg.panic" ||
        { cat "$leg.err" >&2; fail "default LLVM does not report the runtime panic for $name"; }
    cmp -s <(tr -d '\r' <"$oracle.out") <(tr -d '\r' <"$leg.out") ||
        fail "default LLVM printed other output than native C before the panic on $name"
}

check_case option-int "Option unwrap on None value"
check_case option-bool "Option unwrap on None value"
check_case option-string "Option unwrap on None value"
check_case option-record "Option unwrap on None value"
check_case result-err "Result unwrap on Err value"

# A program that has Options, a Result and a record but never unwraps gets no
# failing unwrap helper, so its IR does not reference the runtime panic export
# and still links without the runtime object.
cat >"$WORK_DIR/no-unwrap.pgy" <<'EOF'
struct Point {
    x: Int;
    y: Int;
}
func Pick(flag: Bool) -> Option<Point> {
    if flag { return Some(Point { x: 1, y: 2 }); }
    return None;
}
func Count(flag: Bool) -> Option<Int> {
    if flag { return Some(5); }
    return None;
}
func Main() -> Void {
    Log(IsSome(Pick(true)));
    Log(IsSome(Count(false)));
}
EOF
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
    "$PGY" "$WORK_REL/no-unwrap.pgy" --backend=llvm --emit-llvm -o "$WORK_REL/no-unwrap.ll") \
    >"$WORK_DIR/no-unwrap.log" 2>&1 ||
    { cat "$WORK_DIR/no-unwrap.log" >&2; fail "default LLVM did not emit IR for the unwrap-free program"; }
grep -Fq "@pgy.scalar.option.int.some" "$WORK_DIR/no-unwrap.ll" ||
    fail "unwrap-free program lost the Option helpers this check is about"
if grep -Fq "pgy_runtime_panic_internal_invariant_export" "$WORK_DIR/no-unwrap.ll"; then
    fail "unwrap-free program references the runtime panic export"
fi

echo "[$LABEL] five failed unwraps report native C's runtime panic class and reason on the default LLVM route, and an unwrap-free program carries no panic reference: PASS"
