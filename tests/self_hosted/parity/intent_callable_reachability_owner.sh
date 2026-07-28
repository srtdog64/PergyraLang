#!/usr/bin/env bash
set -euo pipefail

# REACHABLE, not SUBSTITUTING: this gate proves that intent declaration,
# participant, action-default step, and predecessor facts reach the production
# self-host DIR graph. Native/self must publish the same graph anchor. The
# separate intent_callable_execution gate owns the bounded MIR execution rung.

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

assert_graph_id_parity() {
    local self_json="$1"
    local native_log="$2"
    local label="$3"
    "$PYTHON_BIN" - "$self_json" "$native_log" "$label" <<'PY'
import json
from pathlib import Path
import sys

def mir_document(path: str):
    for line in Path(path).read_text(encoding="utf-8").splitlines():
        if line.lstrip().startswith('{"schema":"pgy.mir.v1"'):
            return json.loads(line)
    raise SystemExit(f"{path}: missing pgy.mir.v1 document")

self_doc = mir_document(sys.argv[1])
native_doc = mir_document(sys.argv[2])
label = sys.argv[3]
self_id = self_doc.get("domain_topology", {}).get("domain_graph_id")
native_id = native_doc.get("domain_topology", {}).get("domain_graph_id")
if not isinstance(self_id, int) or self_id <= 0:
    raise SystemExit(f"{label}: self domain graph identity is missing")
if self_id != native_id:
    raise SystemExit(
        f"{label}: native/self domain graph drift: native={native_id} self={self_id}"
    )
print(f"{label}: domain_graph_id={self_id}")
PY
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
if ! (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE" \
        >"$positive_out" 2>"$positive_err"); then
    cat "$positive_out" "$positive_err" >&2
    fail "intent participant/step facts did not reach self MIR"
fi
for fact in '"schema":"pgy.mir.v1"' \
    '"callable_kind":"action"' \
    '"requires":[{"base":"Payable"' \
    '"within":"PaymentZone"' \
    '"causes":"Charged"' \
    '"authorized_by":["self"]' \
    '"field_kind":"tobject_slot"' \
    '"kind":"publish"'; do
    grep -Fq -- "$fact" "$positive_out" \
        || fail "positive self MIR lost fact: $fact"
done

native_mir="$BUILD_DIR/native.mir.log"
native_dir="$BUILD_DIR/native.dir.log"
(cd "$ROOT_DIR" && "$PGY" --mir-json "$FIXTURE" >"$native_mir" 2>&1) \
    || { cat "$native_mir" >&2; fail "native MIR oracle failed"; }
(cd "$ROOT_DIR" && "$PGY" --dir "$FIXTURE" >"$native_dir" 2>&1) \
    || { cat "$native_dir" >&2; fail "native DIR oracle failed"; }
for fact in 'nodes: 14' 'edges: 30' 'intents: 1' \
    'intent-participant from=8 label=payment target=PaymentZone' \
    'intent-participant from=8 label=buyer target=Buyer' \
    'intent-step-zone from=8 label=Promote target=PaymentZone' \
    'intent-step-causes from=8 label=Promote target=Charged' \
    'intent-step-who from=8 label=Promote target=buyer' \
    'intent-step-requires from=8 label=Promote target=Payable' \
    'intent-step-authorized-by from=8 label=Promote target=buyer'; do
    grep -Fq -- "$fact" "$native_dir" \
        || fail "native DIR intent oracle drifted: $fact"
done
assert_graph_id_parity "$positive_out" "$native_mir" "single-step"
grep -Fq '"domain_graph_id":14937234969446610600' "$positive_out" \
    || fail "single-step exact 14-node/30-edge graph anchor drifted"

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

step = """    step Promote {
        on: buyer.Promote();
        expect: true;
    }
"""
if source.count(step) != 1:
    raise SystemExit("intent callable fixture step drifted")
second = step + """    step PromoteAgain {
        using: payment;
        on: buyer.Promote();
        expect: true;
    }
"""
(build / "two-step.pgy").write_text(
    source.replace(step, second), encoding="utf-8", newline="\n"
)
(build / "wrong-using.pgy").write_text(
    source.replace(step, second.replace("using: payment;", "using: buyer;")),
    encoding="utf-8",
    newline="\n",
)
PY

two_step_source="${BUILD_DIR#"$ROOT_DIR"/}/two-step.pgy"
two_step_self="$BUILD_DIR/two-step.self.mir.json"
two_step_native="$BUILD_DIR/two-step.native.mir.log"
two_step_dir="$BUILD_DIR/two-step.native.dir.log"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$two_step_source" \
    >"$two_step_self" 2>"$BUILD_DIR/two-step.self.err") \
    || { cat "$two_step_self" "$BUILD_DIR/two-step.self.err" >&2; fail "two-step self DIR failed"; }
(cd "$ROOT_DIR" && "$PGY" --mir-json "$two_step_source" \
    >"$two_step_native" 2>&1) \
    || { cat "$two_step_native" >&2; fail "two-step native MIR failed"; }
(cd "$ROOT_DIR" && "$PGY" --dir "$two_step_source" \
    >"$two_step_dir" 2>&1) \
    || { cat "$two_step_dir" >&2; fail "two-step native DIR failed"; }
grep -Fq 'intent-step-depends-on' "$two_step_dir" \
    || fail "two-step predecessor edge is missing"
grep -Fq 'step[01] PromoteAgain' "$two_step_dir" \
    || fail "two-step native intent row is missing"
assert_graph_id_parity "$two_step_self" "$two_step_native" "two-step"

wrong_using_source="${BUILD_DIR#"$ROOT_DIR"/}/wrong-using.pgy"
wrong_using_out="$BUILD_DIR/wrong-using.out"
wrong_using_err="$BUILD_DIR/wrong-using.err"
if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified \
        "$wrong_using_source" >"$wrong_using_out" 2>"$wrong_using_err"); then
    fail "wrong-zone using participant was accepted"
fi
grep -Fq 'self-host DIR intent step using binding is unresolved' \
    "$wrong_using_out" "$wrong_using_err" \
    || { cat "$wrong_using_out" "$wrong_using_err" >&2; fail "wrong using diagnostic drifted"; }
if grep -Fq '"schema":"pgy.mir.v1"' "$wrong_using_out" "$wrong_using_err"; then
    fail "wrong using negative emitted a partial MIR artifact"
fi

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

echo "[self-host-intent-callable] semantic + exact intent DIR reachability: PASS"
