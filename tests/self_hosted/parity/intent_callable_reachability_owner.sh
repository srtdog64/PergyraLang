#!/usr/bin/env bash
set -euo pipefail

# REACHABLE, not SUBSTITUTING: this gate proves that intent declaration and
# observability ABI facts pass semantic call admission. The positive stops at
# the next honest boundary, DIR authority lowering.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
BUILD_DIR="${PGY_SELFHOST_INTENT_CALLABLE_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_callable_reachability}"
FIXTURE="tests/self_hosted/parity/fixture/intent_callable_reachability.pgy"
PYTHON_BIN="${PYTHON_BIN:-python3}"

fail() {
    echo "[self-host-intent-callable] $*" >&2
    exit 1
}

mkdir -p "$BUILD_DIR"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-callable" "$PGY" \
    || fail "PGY_BIN is not runnable"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-callable" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

positive_out="$BUILD_DIR/positive.out"
positive_err="$BUILD_DIR/positive.err"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE" \
        >"$positive_out" 2>"$positive_err"); then
    fail "intent authority/DIR lowering unexpectedly completed; advance this gate"
fi
grep -Fq "self-host DIR authority shape is unsupported" \
    "$positive_out" "$positive_err" \
    || { cat "$positive_out" "$positive_err" >&2; fail "positive did not reach DIR authority boundary"; }
if grep -Fq "undefined_function" "$positive_out" "$positive_err" ||
        grep -Fq "semantic call target rows are incomplete" \
            "$positive_out" "$positive_err"; then
    fail "positive regressed behind intent semantic call admission"
fi

"$PYTHON_BIN" - "$ROOT_DIR/$FIXTURE" "$BUILD_DIR" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
build = Path(sys.argv[2])
needle = "Checkout(payment, buyer);"
if source.count(needle) != 1:
    raise SystemExit("intent callable fixture call site drifted")
variants = {
    "arity": "Checkout(payment);",
    "type": "Checkout(payment, 1);",
    "missing": "MissingCheckout(payment, buyer);",
}
for name, replacement in variants.items():
    (build / f"{name}.pgy").write_text(
        source.replace(needle, replacement), encoding="utf-8", newline="\n"
    )
PY

while IFS='|' read -r case_name diagnostic fact; do
    case_out="$BUILD_DIR/$case_name.out"
    case_err="$BUILD_DIR/$case_name.err"
    case_source="${BUILD_DIR#"$ROOT_DIR"/}/$case_name.pgy"
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
            "$case_source" >"$case_out" 2>"$case_err"); then
        fail "$case_name negative was accepted"
    fi
    grep -Fq "Code: $diagnostic" "$case_out" "$case_err" \
        || { cat "$case_out" "$case_err" >&2; fail "$case_name diagnostic drifted"; }
    grep -Fq -- "- func: $fact" "$case_out" "$case_err" \
        || { cat "$case_out" "$case_err" >&2; fail "$case_name identity drifted"; }
    if grep -Fq '"schema":"pgy.mir.v1"' "$case_out" "$case_err"; then
        fail "$case_name negative emitted a partial MIR artifact"
    fi
done <<'CASES'
arity|call_arity_mismatch|Checkout
type|call_arg_type_mismatch|Checkout
missing|undefined_function|MissingCheckout
CASES

echo "[self-host-intent-callable] declaration + observability semantic reachability: PASS"
