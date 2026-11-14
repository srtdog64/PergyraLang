# Sourced by tobject_boundary_execution_owner.sh after native/source admission.
# Owns admitted self-MIR parity and valid-ID wrong-kind rejection only.

BINDING_MIR="$BUILD_DIR/binding.self.mir.json"
(cd "$ROOT_DIR" && "$DRIVER" --emit-mir-json-verified "$BINDING_POSITIVE" \
    >"$BINDING_MIR" 2>"$BUILD_DIR/binding.self.mir.err") \
    || { cat "$BINDING_MIR" "$BUILD_DIR/binding.self.mir.err" >&2; fail "self binding positive MIR failed"; }
grep -Fq '"field_kind":"binding_slot"' "$BINDING_MIR" \
    || fail "self binding slot identity was not preserved"

BINDING_FROM_MIR_C="$BUILD_DIR/binding.from-mir.c"
BINDING_DIRECT_C="$BUILD_DIR/binding.direct.c"
BINDING_SELF_EXE="$BUILD_DIR/binding.self.exe"
(cd "$ROOT_DIR" && "$DRIVER" --mir-json "${BINDING_MIR#"$ROOT_DIR/"}" \
    >"$BINDING_FROM_MIR_C" 2>"$BUILD_DIR/binding.from-mir.err") \
    || { cat "$BUILD_DIR/binding.from-mir.err" >&2; fail "admitted binding MIR C emission failed"; }
(cd "$ROOT_DIR" && "$DRIVER" "$BINDING_POSITIVE" --emit-c-verified \
    >"$BINDING_DIRECT_C" 2>"$BUILD_DIR/binding.direct.err") \
    || { cat "$BUILD_DIR/binding.direct.err" >&2; fail "direct binding source C emission failed"; }
cmp -s "$BINDING_FROM_MIR_C" "$BINDING_DIRECT_C" \
    || fail "direct binding source entrypoint bypassed admitted MIR"
for anchor in \
    'LockZone lock = (LockZone){ .door =' \
    '.key = ((Key){ .id = (9) })' \
    '(*self).view.hp = (*self).door.hp;'; do
    grep -Fq -- "$anchor" "$BINDING_DIRECT_C" \
        || fail "self binding C lost admitted-role anchor: $anchor"
done
"$CC_BIN" -x c -std=c11 -fwrapv -fno-strict-aliasing \
    -I"$ROOT_DIR/src" -I"$ROOT_DIR/src/runtime" -pthread \
    "$BINDING_DIRECT_C" -o "$BINDING_SELF_EXE"
"$BINDING_SELF_EXE" | tr -d '\r' >"$BUILD_DIR/binding.self.run"
cmp -s "$BUILD_DIR/binding.expected.run" "$BUILD_DIR/binding.self.run" \
    || { cat "$BUILD_DIR/binding.self.run" >&2; fail "self binding runtime output drifted"; }
cmp -s "$BUILD_DIR/binding.self.run" "$BUILD_DIR/binding.c.run" \
    || fail "self/native C binding execution differs"
cmp -s "$BUILD_DIR/binding.self.run" "$BUILD_DIR/binding.llvm.run" \
    || fail "self/native LLVM binding execution differs"

"$PYTHON_BIN" - "$BINDING_MIR" "$BUILD_DIR" <<'PY'
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

zone = next(decl for decl in base["decls"] if decl["name"] == "LockZone")
fields = {field["name"]: field for field in zone["fields"]}
assert fields["door"]["field_kind"] == "binding_slot"
assert fields["key"]["field_kind"] == "binding_slot"
assert fields["view"]["field_kind"] == "object_slot"
assert len({field["source_syntax_id"] for field in fields.values()}) == 3

for name, field_name, replacement_kind in (
    ("binding-role-to-object-slot", "door", "object_slot"),
    ("binding-role-to-tobject-slot", "key", "tobject_slot"),
):
    document = copy.deepcopy(base)
    mutated_zone = next(
        decl for decl in document["decls"] if decl["name"] == "LockZone"
    )
    mutated_field = next(
        field for field in mutated_zone["fields"] if field["name"] == field_name
    )
    original_id = mutated_field["source_syntax_id"]
    assert isinstance(original_id, int) and original_id > 0
    mutated_field["field_kind"] = replacement_kind
    assert mutated_field["source_syntax_id"] == original_id
    (Path(sys.argv[2]) / f"negative-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
PY

for negative in "$BUILD_DIR"/negative-binding-role-to-*.mir.json; do
    name="$(basename "$negative" .mir.json)"
    if (cd "$ROOT_DIR" && "$DRIVER" --mir-json \
        "${negative#"$ROOT_DIR/"}" >"$negative.out" 2>"$negative.err"); then
        fail "$name was accepted as caller-supplied domain admission"
    fi
    if grep -Eq '^#include|^typedef|LockZone lock' \
        "$negative.out" "$negative.err"; then
        fail "$name emitted partial C before rejection"
    fi
    for diagnostic in \
        'Code: call_arity_mismatch' \
        '- func: LockZone' \
        '- expected: at_most_1' \
        '- actual: 2'; do
        grep -Fq -- "$diagnostic" "$negative.out" "$negative.err" \
            || fail "$name escaped the nominal constructor policy owner: $diagnostic"
    done
done

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
