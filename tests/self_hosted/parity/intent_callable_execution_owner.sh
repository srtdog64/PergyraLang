#!/usr/bin/env bash
set -euo pipefail

# REACHABLE executable rung: admitted self MIR must reconstruct and execute the
# successful Checkout path through the production self-host C entrypoint.  The
# full rollback/effect-observability runtime remains open, so this gate does not
# claim whole-intent SUBSTITUTING status.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-intent-execution] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-intent-execution" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
CC_BIN="${CC:-gcc}"
command -v "$CC_BIN" >/dev/null 2>&1 || fail "C compiler is required"

FIXTURE_REL="tests/self_hosted/parity/fixture/intent_callable_execution.pgy"
BUILD_DIR="${PGY_SELFHOST_INTENT_EXECUTION_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/intent_callable_execution}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-intent-execution" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

SELF_MIR="$BUILD_DIR/self.mir.json"
SELF_FROM_MIR_C="$BUILD_DIR/self.from-mir.c"
SELF_DIRECT_C="$BUILD_DIR/self.direct.c"
SELF_EXE="$BUILD_DIR/self.exe"
NATIVE_C_EXE="$BUILD_DIR/native.c.exe"
NATIVE_LLVM_EXE="$BUILD_DIR/native.llvm.exe"

(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$FIXTURE_REL" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self MIR production failed"; }
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${SELF_MIR#"$ROOT_DIR/"}" \
    >"$SELF_FROM_MIR_C" 2>"$BUILD_DIR/self.from-mir.err") \
    || { cat "$BUILD_DIR/self.from-mir.err" >&2; fail "admitted MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$FIXTURE_REL" --emit-c-verified \
    >"$SELF_DIRECT_C" 2>"$BUILD_DIR/self.direct.err") \
    || { cat "$BUILD_DIR/self.direct.err" >&2; fail "direct source C emission failed"; }
cmp -s "$SELF_FROM_MIR_C" "$SELF_DIRECT_C" \
    || fail "direct source entrypoint bypassed admitted intent MIR"

for anchor in \
    'bool Checkout(PaymentZone *payment, Buyer *buyer)' \
    '(*payment).buyer = (*buyer);' \
    'Buyer_Promote(&((*payment).buyer));' \
    '(*buyer) = (*payment).buyer;' \
    'PaymentZone_sync(payment);'; do
    grep -Fq -- "$anchor" "$SELF_DIRECT_C" \
        || fail "self C lost intent execution anchor: $anchor"
done
if grep -Fq 'Clone(' "$SELF_DIRECT_C"; then
    fail "Clone survived as an undeclared C call"
fi

"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$SELF_DIRECT_C" -o "$SELF_EXE"
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=c -o "$NATIVE_C_EXE" \
    >"$BUILD_DIR/native.c.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.c.compile.log" >&2; fail "native C compile failed"; }
(cd "$ROOT_DIR" && "$PGY" "$FIXTURE_REL" --backend=llvm -o "$NATIVE_LLVM_EXE" \
    >"$BUILD_DIR/native.llvm.compile.log" 2>&1) \
    || { cat "$BUILD_DIR/native.llvm.compile.log" >&2; fail "native LLVM compile failed"; }

"$SELF_EXE" | tr -d '\r' >"$BUILD_DIR/self.run"
"$NATIVE_C_EXE" | tr -d '\r' >"$BUILD_DIR/native.c.run"
"$NATIVE_LLVM_EXE" | tr -d '\r' >"$BUILD_DIR/native.llvm.run"
printf '%s\n' \
    'buyer.total=3' \
    'payment.total=3' \
    'payment.ready=true' \
    'payment.label=Mina' \
    'world.ready=true' >"$BUILD_DIR/expected.run"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/self.run" \
    || { cat "$BUILD_DIR/self.run" >&2; fail "self runtime output drifted"; }
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.c.run" \
    || fail "self/native C intent execution differs"
cmp -s "$BUILD_DIR/self.run" "$BUILD_DIR/native.llvm.run" \
    || fail "self/native LLVM intent execution differs"

"$PYTHON_BIN" - "$SELF_MIR" "$BUILD_DIR" <<'PY'
import copy
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
for line in source.read_text(encoding="utf-8").splitlines():
    if line.lstrip().startswith('{"schema":"pgy.mir.v1"'):
        base = json.loads(line)
        break
else:
    raise SystemExit("missing pgy.mir.v1 document")

intent = next(row for row in base["routines"] if row["kind"] == "intent")
block0 = intent["blocks"][0]["instructions"]

def one(name, kind=None):
    rows = [row for row in block0
            if row.get("name") == name and (kind is None or row.get("kind") == kind)]
    assert len(rows) == 1, (name, rows)
    return rows[0]

mutations = {}
doc = copy.deepcopy(base)
next(row for row in doc["routines"] if row["kind"] == "intent")["kind"] = "function"
mutations["kind-fallback"] = doc

doc = copy.deepcopy(base)
row = next(row for row in next(r for r in doc["routines"] if r["kind"] == "intent")["blocks"][0]["instructions"] if row.get("name") == "CommitIntent")
row["arg0"] = "Foreign"
mutations["commit-identity"] = doc

doc = copy.deepcopy(base)
rows = next(r for r in doc["routines"] if r["kind"] == "intent")["blocks"][0]["instructions"]
next(row for row in rows if row.get("name") == "IntentBinding" and row.get("arg0") == "buyer")["arg1"] = "String"
mutations["binding-type"] = doc

doc = copy.deepcopy(base)
rows = next(r for r in doc["routines"] if r["kind"] == "intent")["blocks"][0]["instructions"]
next(row for row in rows if row.get("name") == "IntentZoneAlias")["arg0"] = "buyer"
mutations["zone-alias"] = doc

doc = copy.deepcopy(base)
rows = next(r for r in doc["routines"] if r["kind"] == "intent")["blocks"][0]["instructions"]
next(row for row in rows if row.get("name") == "Authorize")["arg0"] = "payment"
mutations["authorize-cross-carrier"] = doc

doc = copy.deepcopy(base)
rollback = next(r for r in doc["routines"] if r["kind"] == "intent")["blocks"][2]["instructions"]
next(row for row in rollback if row.get("name") == "AbortIntent")["arg0"] = "Foreign"
mutations["rollback-identity"] = doc

for name, document in mutations.items():
    (Path(sys.argv[2]) / f"negative-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
PY

for negative in "$BUILD_DIR"/negative-*.mir.json; do
    name="$(basename "$negative" .mir.json)"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${negative#"$ROOT_DIR/"}" >"$negative.out" 2>"$negative.err"); then
        fail "$name was accepted"
    fi
    if grep -Eq '^#include|^typedef|bool Checkout\(' \
        "$negative.out" "$negative.err"; then
        fail "$name emitted a partial C artifact before rejection"
    fi
done

echo "[self-host-intent-execution] exact successful action path + MIR negatives: PASS"
