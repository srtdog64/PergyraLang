#!/usr/bin/env bash
set -euo pipefail

# TObject is a detached immutable publication value. It is neither a zone
# constructor input nor a fresh projection source. This gate keeps native C,
# native LLVM, self source production, and admitted self MIR on that boundary.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
export PATH

fail() {
    echo "[self-host-tobject-boundary] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "self-host-tobject-boundary" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON_BIN:-python3}"
command -v "$PYTHON_BIN" >/dev/null 2>&1 || fail "python is required"
BUILD_DIR="${PGY_SELFHOST_TOBJECT_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/tobject_boundary}"
DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
POSITIVE="tests/self_hosted/parity/fixture/zone_projection_constructor_source_order.pgy"
BINDING_POSITIVE="tests/self_hosted/parity/fixture/binding_slot_constructor_source_order.pgy"
SELF_POSITIVE="tests/self_hosted/parity/fixture/intent_callable_execution.pgy"
CONSTRUCTOR_NEGATIVE="$BUILD_DIR/zone_projection_constructor_input_rejected.pgy"
SOURCE_NEGATIVE="tests/self_hosted/parity/fixture/domain_topology_tobject_source_rejected.pgy"
INITIALIZER_NEGATIVE="tests/self_hosted/parity/fixture/domain_projection_initializer_rejected.pgy"
DOMAIN_CONSTRUCTOR_NEGATIVE="tests/self_hosted/parity/fixture/domain_projection_constructor_input_rejected.pgy"
mkdir -p "$BUILD_DIR"

if [[ -n "$DRIVER" ]]; then
    DRIVER="$(pgy_select_optional_exe_binary "$DRIVER")"
    pgy_require_runnable_binary_here "self-host-tobject-boundary" "$DRIVER" \
        || fail "prebuilt driver is not runnable"
else
    DRIVER="$BUILD_DIR/driver_rung2.exe"
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/driver.compile.log" >&2; fail "driver build failed"; }
fi

"$PYTHON_BIN" - "$ROOT_DIR/$SELF_POSITIVE" "$CONSTRUCTOR_NEGATIVE" <<'PY'
from pathlib import Path
import sys

source = Path(sys.argv[1]).read_text(encoding="utf-8")
old = "let payment = PaymentZone(Clone(buyer));"
new = 'let payment = PaymentZone(Clone(buyer), BuyerView("forged", 99));'
assert source.count(old) == 1
Path(sys.argv[2]).write_text(source.replace(old, new), encoding="utf-8")
PY

for backend in c llvm; do
    native_exe="$BUILD_DIR/native.$backend.exe"
    (cd "$ROOT_DIR" && "$PGY" "$POSITIVE" --backend="$backend" \
        -o "$native_exe" >"$BUILD_DIR/native.$backend.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/native.$backend.compile.log" >&2; fail "native $backend positive failed"; }
    "$native_exe" | tr -d '\r' >"$BUILD_DIR/native.$backend.run"

    if (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$CONSTRUCTOR_NEGATIVE")" \
        --backend="$backend" \
        -o "$BUILD_DIR/invalid-constructor.$backend.exe" \
        >"$BUILD_DIR/invalid-constructor.$backend.out" \
        2>"$BUILD_DIR/invalid-constructor.$backend.err"); then
        fail "native $backend admitted projection storage as a zone constructor input"
    fi
    grep -Fq 'accepts at most 1 positional field argument(s), got 2' \
        "$BUILD_DIR/invalid-constructor.$backend.out" \
        "$BUILD_DIR/invalid-constructor.$backend.err" \
        || fail "native $backend constructor diagnostic drifted"

    if (cd "$ROOT_DIR" && "$PGY" "$SOURCE_NEGATIVE" --backend="$backend" \
        -o "$BUILD_DIR/invalid-source.$backend.exe" \
        >"$BUILD_DIR/invalid-source.$backend.out" \
        2>"$BUILD_DIR/invalid-source.$backend.err"); then
        fail "native $backend admitted a detached tobject as projection source"
    fi
    grep -Fq "cannot be a tobject slot" \
        "$BUILD_DIR/invalid-source.$backend.out" \
        "$BUILD_DIR/invalid-source.$backend.err" \
        || fail "native $backend tobject-source diagnostic drifted"
done

for backend in c llvm; do
    binding_exe="$BUILD_DIR/binding.$backend.exe"
    (cd "$ROOT_DIR" && "$PGY" "$BINDING_POSITIVE" --backend="$backend" \
        -o "$binding_exe" >"$BUILD_DIR/binding.$backend.compile.log" 2>&1) \
        || { cat "$BUILD_DIR/binding.$backend.compile.log" >&2; fail "native $backend binding positive failed"; }
    "$binding_exe" | tr -d '\r' >"$BUILD_DIR/binding.$backend.run"

    if (cd "$ROOT_DIR" && "$PGY" "$DOMAIN_CONSTRUCTOR_NEGATIVE" \
        --backend="$backend" -o "$BUILD_DIR/invalid-domain-constructor.$backend.exe" \
        >"$BUILD_DIR/invalid-domain-constructor.$backend.out" \
        2>"$BUILD_DIR/invalid-domain-constructor.$backend.err"); then
        fail "native $backend admitted relation/effect projection constructor input"
    fi
    grep -Fq 'positional field argument(s), got' \
        "$BUILD_DIR/invalid-domain-constructor.$backend.out" \
        "$BUILD_DIR/invalid-domain-constructor.$backend.err" \
        || fail "native $backend domain constructor diagnostic drifted"
done

printf '%s\n' 'door=5' 'key=9' 'view=5' >"$BUILD_DIR/binding.expected.run"
cmp -s "$BUILD_DIR/binding.expected.run" "$BUILD_DIR/binding.c.run" \
    || { cat "$BUILD_DIR/binding.c.run" >&2; fail "native C binding output drifted"; }
cmp -s "$BUILD_DIR/binding.c.run" "$BUILD_DIR/binding.llvm.run" \
    || fail "native C/LLVM binding constructor source-order differs"

for backend in c llvm; do
    if (cd "$ROOT_DIR" && "$PGY" "$INITIALIZER_NEGATIVE" --backend="$backend" \
        -o "$BUILD_DIR/invalid-initializer.$backend.exe" \
        >"$BUILD_DIR/invalid-initializer.$backend.out" \
        2>"$BUILD_DIR/invalid-initializer.$backend.err"); then
        fail "native $backend admitted an unowned projection initializer"
    fi
    grep -Fq 'cannot declare an initializer' \
        "$BUILD_DIR/invalid-initializer.$backend.out" \
        "$BUILD_DIR/invalid-initializer.$backend.err" \
        || fail "native $backend projection-initializer diagnostic drifted"
done

printf '%s\n' 'alpha=7' 'beta=9' 'view=7' 'receipt=9' \
    >"$BUILD_DIR/expected.run"
cmp -s "$BUILD_DIR/expected.run" "$BUILD_DIR/native.c.run" \
    || { cat "$BUILD_DIR/native.c.run" >&2; fail "native C source-order output drifted"; }
cmp -s "$BUILD_DIR/native.c.run" "$BUILD_DIR/native.llvm.run" \
    || fail "native C/LLVM zone constructor source-order differs"

SELF_MIR="$BUILD_DIR/self.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$SELF_POSITIVE" \
    >"$SELF_MIR" 2>"$BUILD_DIR/self.mir.err") \
    || { cat "$SELF_MIR" "$BUILD_DIR/self.mir.err" >&2; fail "self positive MIR failed"; }

for negative in "$CONSTRUCTOR_NEGATIVE" "$SOURCE_NEGATIVE" \
    "$INITIALIZER_NEGATIVE" "$DOMAIN_CONSTRUCTOR_NEGATIVE"; do
    name="$(basename "$negative" .pgy)"
    driver_negative="$negative"
    if [[ "$negative" == "$CONSTRUCTOR_NEGATIVE" ]]; then
        driver_negative="${negative#"$ROOT_DIR/"}"
    fi
    if (cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$driver_negative" \
        >"$BUILD_DIR/$name.self.out" 2>"$BUILD_DIR/$name.self.err"); then
        fail "self source producer admitted $name"
    fi
done


BINDING_MIR="$BUILD_DIR/binding.self.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$BINDING_POSITIVE" \
    >"$BINDING_MIR" 2>"$BUILD_DIR/binding.self.mir.err") \
    || { cat "$BINDING_MIR" "$BUILD_DIR/binding.self.mir.err" >&2; fail "self binding positive MIR failed"; }
grep -Fq '"field_kind":"binding_slot"' "$BINDING_MIR" \
    || fail "self binding slot identity was not preserved"

"$PYTHON_BIN" - "$SELF_MIR" "$BUILD_DIR/tobject-source.mir.json" <<'PY'
import json
from pathlib import Path
import sys

source = Path(sys.argv[1])
for line in source.read_text(encoding="utf-8").splitlines():
    if line.lstrip().startswith('{"schema":"pgy.mir.v1"'):
        doc = json.loads(line)
        break
else:
    raise SystemExit("missing pgy.mir.v1 document")

zone = next(decl for decl in doc["decls"] if decl["name"] == "PaymentZone")
receipt = next(field for field in zone["fields"] if field["name"] == "buyerPacket")
assert receipt["field_kind"] == "tobject_slot"
refresh = next(row for row in doc["domain_topology"]["rows"] if row["kind"] == "refresh")
refresh["source_slot_name"] = receipt["name"]
refresh["source_slot_source_syntax_id"] = receipt["source_syntax_id"]
Path(sys.argv[2]).write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")
PY

MUTATED="$BUILD_DIR/tobject-source.mir.json"
if (cd "$ROOT_DIR" && "$DRIVER" --mir-json "${MUTATED#"$ROOT_DIR/"}" \
    >"$MUTATED.out" 2>"$MUTATED.err"); then
    fail "self MIR admission accepted a valid-ID tobject projection source"
fi
grep -Fq 'MIR domain topology facts are missing or invalid' \
    "$MUTATED.out" "$MUTATED.err" \
    || fail "self MIR tobject-source diagnostic drifted"
if grep -Eq '^#include|^typedef' "$MUTATED.out" "$MUTATED.err"; then
    fail "self MIR emitted partial C before rejecting tobject source"
fi

echo "[self-host-tobject-boundary] detached receipt + binding/projection constructor boundaries: PASS"
