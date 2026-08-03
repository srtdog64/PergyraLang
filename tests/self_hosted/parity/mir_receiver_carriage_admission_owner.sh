#!/usr/bin/env bash
# Focused admission ratchet for required routine identity and receiver carriage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:mir-receiver-carriage-admission] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "mir-receiver-carriage-admission" "$PGY" \
    || fail "PGY_BIN is not runnable"

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required"
    fi
fi

OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_index_owner.pgy"
IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/program_routine_receiver_identity_owner.pgy"
MIR_LOWER_SOURCE="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy"
VALUE_FIXTURE="$ROOT_DIR/tests/cases/backend_compare/class_method_self_chain/main.pgy"
IDENTITY_FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
INTENT_FIXTURE="$ROOT_DIR/tests/cases/backend_compare/boilerplate_reduction/main.pgy"
BUILD_DIR="${PGY_SELFHOST_BUILD_DIR:-$ROOT_DIR/.tmp/self_hosted/mir_receiver_carriage_admission}"
MIR_LOWER="$BUILD_DIR/mir_lower.exe"
DRIVER_BIN="${PGY_SELFHOST_PREBUILT_DRIVER:-$BUILD_DIR/driver_c.exe}"

mkdir -p "$BUILD_DIR"

for term in \
    'let source_syntax_ids: Array<String>;' \
    'let receiver_carriages: Array<String>;' \
    'CaptureMirProgramRoutineReceiverIdentity('; do
    grep -Fq -- "$term" "$OWNER" \
        || fail "routine index receiver capture term is missing: $term"
done
for term in \
    'MirProgramRoutineOwnerNominalKind' \
    'MirProgramRoutineReceiverIdentityReady' \
    'MirProgramDeclarationFieldIdentityPositiveDecimal' \
    'row != except_row && source_syntax_ids[row] =='; do
    grep -Fq -- "$term" "$IDENTITY_OWNER" \
        || fail "routine admission owner term is missing: $term"
done
if grep -Eq 'receiver_carriage[[:space:]]*==[[:space:]]*""|owner[[:space:]]*==[[:space:]]*""[[:space:]]*\?' \
    "$OWNER" "$IDENTITY_OWNER"; then
    fail "routine admission reopened a missing-carriage/owner fallback"
fi

if ! (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")" \
    >"$BUILD_DIR/mir_lower.compile.log" 2>&1); then
    cat "$BUILD_DIR/mir_lower.compile.log" >&2
    fail "mir_lower build failed"
fi

if [[ -z "${PGY_SELFHOST_PREBUILT_DRIVER:-}" ]]; then
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$DRIVER_SOURCE")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER_BIN")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1); then
        cat "$BUILD_DIR/driver.compile.log" >&2
        fail "self-host driver build failed"
    fi
else
    DRIVER_BIN="$(pgy_select_optional_exe_binary "$DRIVER_BIN")"
    pgy_require_runnable_binary_here \
        "mir-receiver-carriage-admission:self" "$DRIVER_BIN" \
        || fail "PGY_SELFHOST_PREBUILT_DRIVER is not runnable"
fi

produce_pair() {
    local label="$1" fixture="$2" fixture_rel native_mir self_mir
    fixture_rel="${fixture#$ROOT_DIR/}"
    native_mir="$BUILD_DIR/${label}.native.mir.json"
    self_mir="$BUILD_DIR/${label}.self.mir.json"
    (cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
        "$(pgy_path_for_compiler "$PGY" "$fixture")" 2>/dev/null) \
        | tr -d '\r' >"$native_mir"
    (cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
        "$fixture_rel" >"$self_mir.raw" 2>"$self_mir.err") \
        || { cat "$self_mir.raw" "$self_mir.err" >&2; fail "self MIR production failed: $label"; }
    tr -d '\r' <"$self_mir.raw" >"$self_mir"
    for mir in "$native_mir" "$self_mir"; do
        grep -Fq '"receiver_carriage":' "$mir" \
            || fail "receiver carriage is missing: ${mir#$ROOT_DIR/}"
        (cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
            "${mir#$ROOT_DIR/}" >"$mir.verify.out" 2>"$mir.verify.err") \
            || { cat "$mir.verify.out" "$mir.verify.err" >&2; fail "valid MIR was rejected: ${mir#$ROOT_DIR/}"; }
    done
}

produce_pair "value" "$VALUE_FIXTURE"
produce_pair "identity" "$IDENTITY_FIXTURE"

INTENT_MIR="$BUILD_DIR/intent.native.mir.json"
(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$INTENT_FIXTURE")" 2>/dev/null) \
    | tr -d '\r' >"$INTENT_MIR"
(cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
    "${INTENT_MIR#$ROOT_DIR/}" >"$INTENT_MIR.verify.out" \
    2>"$INTENT_MIR.verify.err") \
    || { cat "$INTENT_MIR.verify.out" "$INTENT_MIR.verify.err" >&2; fail "valid intent MIR was rejected"; }

"$PYTHON_BIN" - "$BUILD_DIR" <<'PY'
import copy
import json
import os
import sys

build_dir = sys.argv[1]
mutable_nominals = {
    "subject", "vessel", "party", "roster", "world", "relation",
    "effect", "role", "zone",
}

def read(name):
    with open(os.path.join(build_dir, name), encoding="utf-8") as stream:
        return json.load(stream)

def expected_rows(doc):
    declarations = {}
    for declaration in doc["decls"]:
        name = declaration["name"]
        assert name not in declarations, name
        declarations[name] = declaration.get("nominal_kind") or declaration["kind"]
    seen = set()
    rows = []
    for routine in doc["routines"]:
        source_id = routine["source_syntax_id"]
        assert isinstance(source_id, int) and source_id > 0
        assert source_id not in seen
        seen.add(source_id)
        kind = routine["kind"]
        carriage = routine["receiver_carriage"]
        if kind == "method":
            nominal = declarations[routine["owner"]]
            expected = "mutable-identity" if nominal in mutable_nominals else "value"
        else:
            expected = "none"
        assert carriage == expected, (routine.get("owner"), routine["name"], carriage, expected)
        rows.append((kind, routine.get("owner", ""), routine["name"], carriage))
    return rows

for label in ("value", "identity"):
    native = read(f"{label}.native.mir.json")
    self_hosted = read(f"{label}.self.mir.json")
    assert expected_rows(native) == expected_rows(self_hosted)

value = read("value.native.mir.json")
identity = read("identity.native.mir.json")
intent_carrier = read("intent.native.mir.json")
mutations = {}

doc = copy.deepcopy(value)
next(row for row in doc["routines"] if row["kind"] == "method").pop("receiver_carriage")
mutations["missing-carriage"] = doc

doc = copy.deepcopy(value)
next(row for row in doc["routines"] if row["kind"] == "method")["receiver_carriage"] = "borrowed"
mutations["unknown-carriage"] = doc

doc = copy.deepcopy(value)
next(row for row in doc["routines"] if row["kind"] == "method")["receiver_carriage"] = "mutable-identity"
mutations["mutable-class"] = doc

doc = copy.deepcopy(identity)
zone_names = {
    declaration["name"] for declaration in doc["decls"]
    if (declaration.get("nominal_kind") or declaration["kind"]) == "zone"
}
zone_method = next(
    row for row in doc["routines"]
    if row["kind"] == "method" and row.get("owner") in zone_names
)
zone_method["receiver_carriage"] = "value"
mutations["value-zone"] = doc

doc = copy.deepcopy(identity)
method = next(row for row in doc["routines"] if row["kind"] == "method")
method["owner"] = "ForeignOwner"
mutations["foreign-owner"] = doc

doc = copy.deepcopy(value)
next(row for row in doc["routines"] if row["kind"] == "method").pop("source_syntax_id")
mutations["missing-source-id"] = doc

doc = copy.deepcopy(value)
next(row for row in doc["routines"] if row["kind"] == "method")["source_syntax_id"] = 0
mutations["zero-source-id"] = doc

doc = copy.deepcopy(value)
doc["routines"][1]["source_syntax_id"] = doc["routines"][0]["source_syntax_id"]
mutations["duplicate-source-id"] = doc

doc = copy.deepcopy(intent_carrier)
intent = next(row for row in doc["routines"] if row["kind"] == "intent")
intent["receiver_carriage"] = "mutable-identity"
mutations["mutable-intent"] = doc

doc = copy.deepcopy(identity)
owner = next(row["owner"] for row in doc["routines"] if row["kind"] == "method")
declaration = next(row for row in doc["decls"] if row["name"] == owner)
doc["decls"].append(copy.deepcopy(declaration))
mutations["ambiguous-owner"] = doc

for name, document in mutations.items():
    with open(os.path.join(build_dir, f"negative-{name}.mir.json"), "w", encoding="utf-8") as stream:
        json.dump(document, stream, separators=(",", ":"))
PY

for mutated in "$BUILD_DIR"/negative-*.mir.json; do
    if (cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
        "${mutated#$ROOT_DIR/}" >"$mutated.out" 2>"$mutated.err"); then
        fail "mutated receiver identity was accepted: ${mutated##*/}"
    fi
done

echo "[self-host-parity:mir-receiver-carriage-admission] PASS"
