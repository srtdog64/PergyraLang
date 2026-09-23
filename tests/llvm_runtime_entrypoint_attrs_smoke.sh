#!/usr/bin/env bash
# Runtime attribute facts reach only the runtime's own declarations.
# llvm_run_optimization gave every declaration in the module nounwind and
# willreturn, and any name containing "panic" noreturn and cold (red-team audit
# E2). A user extern "c" declaration is a declaration too: user_panic_probe
# became noreturn and LLVM deleted the call to user_after_probe that follows
# it. The facts are now keyed on the runtime registry row, not on spelling.
# - Native LLVM: the object keeps the call after user_panic_probe, and the
#   optimized module gives the two user declarations no attribute group while
#   the runtime panic export keeps noreturn, cold and nounwind.
# - Default LLVM: while the route refuses extern "c" declarations the gate
#   requires the refusal; once it accepts them, its IR is held to the same
#   rule.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

LABEL="llvm-runtime-entrypoint-attrs"
PGY="$(pgy_select_optional_exe_binary "${PGY_BIN:-$ROOT_DIR/bin/pgy}")"
DRIVER="$(pgy_select_optional_exe_binary "${PGY_SELF_DRIVER_BIN:-$ROOT_DIR/bin/pgy-self-driver}")"
WORK_REL=".tmp/llvm_runtime_entrypoint_attrs"
WORK_DIR="$ROOT_DIR/$WORK_REL"
NM="${NM:-nm}"

fail() { echo "[$LABEL] $*" >&2; exit 1; }
pgy_require_runnable_binary_here "$LABEL" "$PGY" || exit 1
pgy_require_runnable_binary_here "$LABEL" "$DRIVER" || exit 1
command -v "$NM" >/dev/null || fail "missing $NM"

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR"
cat >"$WORK_DIR/user_panic_extern.pgy" <<'EOF'
extern "c" {
    func user_panic_probe(code: Int) -> Int;
    func user_after_probe(code: Int) -> Int;
}

func Main() -> Void {
    let first: Int = user_panic_probe(1);
    let second: Int = user_after_probe(first);
    Log(second);
}
EOF

# --- native LLVM: object and optimized module ---
OPT_IR="$WORK_DIR/optimized.ll"
set +e
(cd "$ROOT_DIR" && PGY_LLVM_DUMP_OPT_IR="$(pgy_path_for_compiler "$PGY" "$OPT_IR")" \
    "$PGY" "$WORK_REL/user_panic_extern.pgy" --native-pipeline --backend=llvm \
    -o "$WORK_REL/user_panic_extern.exe") >"$WORK_DIR/native.log" 2>&1
set -e
if grep -Fq "compiled without LLVM backend support" "$WORK_DIR/native.log"; then
    echo "[$LABEL] SKIP: this build was compiled without LLVM backend support"
    exit 0
fi
# No object defines the two user symbols, so the link is what must fail, and
# the object it was given stays behind for inspection.
grep -Fq "LLVM link failed" "$WORK_DIR/native.log" ||
    { cat "$WORK_DIR/native.log" >&2; fail "native LLVM did not reach the link step"; }
OBJECT="$WORK_DIR/user_panic_extern.o"
[[ -s "$OBJECT" ]] || fail "native LLVM left no object"
"$NM" "$OBJECT" >"$WORK_DIR/object.nm"
for symbol in user_panic_probe user_after_probe; do
    grep -Eq "[[:space:]]U _?${symbol}\$" "$WORK_DIR/object.nm" ||
        { cat "$WORK_DIR/object.nm" >&2
          fail "object no longer calls $symbol: code after user_panic_probe was deleted"; }
done

[[ -s "$OPT_IR" ]] || fail "PGY_LLVM_DUMP_OPT_IR wrote no optimized module"
# declaration_attrs IR SYMBOL: the attribute group of SYMBOL's declaration.
declaration_attrs() {
    local ir="$1" symbol="$2" line group
    line="$(grep -E "^declare .*@${symbol}\(" "$ir" || true)"
    [[ -n "$line" ]] || fail "$(basename "$ir") has no declaration of @$symbol"
    group="$(printf '%s\n' "$line" | sed -nE 's/.* (#[0-9]+)$/\1/p')"
    [[ -n "$group" ]] || return 0
    grep -E "^attributes ${group} = " "$ir" || fail "attribute group $group is missing"
}
# The runtime's own panic export keeps its facts: this check can see them.
runtime_attrs="$(declaration_attrs "$OPT_IR" pgy_runtime_panic_internal_invariant_export)"
for fact in noreturn cold nounwind; do
    [[ "$runtime_attrs" == *"$fact"* ]] ||
        fail "runtime panic entrypoint lost $fact: ${runtime_attrs:-<none>}"
done
for symbol in user_panic_probe user_after_probe; do
    user_attrs="$(declaration_attrs "$OPT_IR" "$symbol")"
    [[ -z "$user_attrs" ]] ||
        fail "user extern \"c\" @$symbol received runtime attributes: $user_attrs"
done

# --- default LLVM: refusal today, the same rule once it accepts ---
DEFAULT_IR_REL="$WORK_REL/default.ll"
set +e
(cd "$ROOT_DIR" && PGY_SELF_DRIVER_BIN="$DRIVER" \
    "$PGY" "$WORK_REL/user_panic_extern.pgy" --backend=llvm --emit-llvm \
    -o "$DEFAULT_IR_REL") >"$WORK_DIR/default.log" 2>&1
default_rc=$?
set -e
default_state="refuses extern \"c\" declarations"
if [[ "$default_rc" -ne 0 ]]; then
    [[ ! -e "$ROOT_DIR/$DEFAULT_IR_REL" ]] ||
        fail "default LLVM refused the program but left IR behind"
else
    for symbol in user_panic_probe user_after_probe; do
        user_attrs="$(declaration_attrs "$ROOT_DIR/$DEFAULT_IR_REL" "$symbol")"
        [[ -z "$user_attrs" ]] ||
            fail "default LLVM gave user extern \"c\" @$symbol attributes: $user_attrs"
    done
    default_state="gives the user declarations no attributes"
fi

echo "[$LABEL] native LLVM keeps the call after a user extern named like a panic and gives it no runtime attributes; default LLVM $default_state: PASS"
