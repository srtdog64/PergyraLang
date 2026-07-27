#!/usr/bin/env bash
# Executable negative ratchet for canonical MIR topology identity epochs.
# The valid carrier below keeps the native input envelope only because the
# canonical JSON projection is not itself the MIR input schema. All declaration
# field and topology identities in that carrier come from the canonicalizer.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:canonical-identity-epoch] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "canonical-identity-epoch" "$PGY" \
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

IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/compiler/canonical_mir_identity_epoch_owner.pgy"
FIELD_IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/compiler/canonical_mir_field_identity_epoch_owner.pgy"
DRIVER_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
CARRIER_OWNER="$ROOT_DIR/src/self_hosted/mir/domain_topology_fact_owner.pgy"
DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/canonical_identity_epoch_gate"
DRIVER_BIN="$BUILD_DIR/driver_c.exe"
RAW_MIR="$BUILD_DIR/raw.mir.json"
VALID_MIR="$BUILD_DIR/valid-simple.mir.json"
CANONICAL_MIR="$BUILD_DIR/canonical.mir.json"
CANONICAL_CARRIER="$BUILD_DIR/canonical-epoch-carrier.mir.json"

mkdir -p "$BUILD_DIR"

for term in \
    'import "canonical_mir_identity_epoch_owner.pgy"' \
    'CanonicalMirIdentityEpochArtifactFromTreeText' \
    'CanonicalMirIdentityEpochRebindProgramFacts'; do
    grep -Fq -- "$term" "$DRIVER_OWNER" \
        || fail "driver import/consumer closure is missing: $term"
done
grep -Fq -- 'import "canonical_mir_field_identity_epoch_owner.pgy"' \
    "$IDENTITY_OWNER" \
    || fail "tree/directive epoch owner does not import the field epoch owner"
grep -Fq -- 'CanonicalMirIdentityEpochRemapTopology' "$IDENTITY_OWNER" \
    || fail "tree/directive epoch owner lost topology composition"
for term in \
    'CanonicalMirIdentityEpochSourceFieldKind' \
    'CanonicalMirIdentityEpochCanonicalFieldId' \
    'CanonicalMirIdentityEpochBindDeclarationFields' \
    'declarations.field_names[field_index] == field_name' \
    'declarations.field_kinds[field_index] == field_kind' \
    'index.field_names[i] == field_name' \
    'index.field_source_syntax_ids[i] == source_syntax_id'; do
    grep -Fq -- "$term" "$FIELD_IDENTITY_OWNER" \
        || fail "exact owner/name/field-kind join closure is missing: $term"
done
if grep -Eq 'source_syntax_id[[:space:]]*[+-]|canonical_id[[:space:]]*==[[:space:]]*source_syntax_id' \
    "$IDENTITY_OWNER" "$FIELD_IDENTITY_OWNER"; then
    fail "canonical identity owner reopened numeric equality/offset remapping"
fi
grep -Fq -- 'graph.topology_row_count != ArrayLength(graph.topology.kinds)' \
    "$CARRIER_OWNER" \
    || fail "DIR-to-MIR topology carrier does not consume the typed row arrays"
if grep -Fq -- 'canonical MIR bridge cannot reconstruct non-empty domain topology' \
    "$DRIVER_OWNER"; then
    fail "canonicalizer retained the non-empty topology rejection"
fi

if ! (cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$DRIVER_SOURCE")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER_BIN")" \
    >"$BUILD_DIR/driver.compile.log" 2>&1); then
    cat "$BUILD_DIR/driver.compile.log" >&2
    fail "driver build failed"
fi

(cd "$ROOT_DIR" && "$PGY" --mir-json \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
    2>"$BUILD_DIR/raw.err" | tr -d '\r' >"$RAW_MIR")
[[ -s "$RAW_MIR" ]] || fail "native topology MIR was not produced"

"$PYTHON_BIN" - "$RAW_MIR" "$VALID_MIR" <<'PY'
import json
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
doc = json.loads(source.read_text(encoding="utf-8"))
main = next(row for row in doc["routines"] if row["name"] == "Main")
main["blocks"] = [{
    "id": 0,
    "reachable": True,
    "instructions": [{
        "id": 0,
        "kind": "return",
        "name": "return",
        "result": None,
        "arg0": None,
        "arg1": None,
        "slot_anchor": None,
        "abi_type_name": None,
        "abi_layout_id": 0,
        "abi_layout_required": False,
        "abi_layout": None,
        "machine_layer": None,
        "machine_contact_kind": None,
        "expr0": None,
        "expr0_graph": None,
        "expr1": None,
        "expr1_graph": None,
        "speculation": None,
        "source_type": None,
        "match_patterns": [],
        "match_variant": None,
        "match_bindings": [],
        "match_binding_types": [],
        "destructure_element_type": None,
        "destructure_bindings": [],
        "uses": [],
        "ast": None,
    }],
}]
target.write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")
PY

if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --canonicalize-mir-json \
    "${VALID_MIR#$ROOT_DIR/}" 2>"$BUILD_DIR/canonical.err" \
    | tr -d '\r' >"$CANONICAL_MIR"); then
    cat "$BUILD_DIR/canonical.err" >&2
    fail "valid non-empty topology canonicalization failed"
fi

"$PYTHON_BIN" - "$VALID_MIR" "$CANONICAL_MIR" \
    "$CANONICAL_CARRIER" "$BUILD_DIR" <<'PY'
import copy
import json
import pathlib
import sys

raw_path, canonical_path, carrier_path, output_dir = map(
    pathlib.Path, sys.argv[1:]
)
raw = json.loads(raw_path.read_text(encoding="utf-8"))
canonical = json.loads(canonical_path.read_text(encoding="utf-8"))

def declaration_field_map(doc):
    return {
        (decl["name"], field["name"], field["field_kind"]):
            field["source_syntax_id"]
        for decl in doc["decls"]
        for field in decl["fields"]
    }

raw_fields = declaration_field_map(raw)
canonical_fields = declaration_field_map(canonical)
raw_topology = raw["domain_topology"]
canonical_topology = canonical["domain_topology"]
assert raw_topology["domain_graph_id"] == canonical_topology["domain_graph_id"]
assert len(raw_topology["rows"]) == len(canonical_topology["rows"]) == 3

slot_prefixes = (
    "projection_slot", "source_slot", "layer_slot", "target_slot",
    "left_slot", "right_slot", "participant_slot",
)
for raw_row, canonical_row in zip(
    raw_topology["rows"], canonical_topology["rows"]
):
    assert raw_row["owner_name"] == canonical_row["owner_name"]
    assert raw_row["kind"] == canonical_row["kind"]
    assert raw_row["owner_source_syntax_id"] != canonical_row["owner_source_syntax_id"]
    assert raw_row["source_syntax_id"] != canonical_row["source_syntax_id"]
    for prefix in slot_prefixes:
        name_key = prefix + "_name"
        id_key = prefix + "_source_syntax_id"
        assert raw_row[name_key] == canonical_row[name_key]
        if raw_row[name_key] is None:
            assert raw_row[id_key] == canonical_row[id_key] == 0
            continue
        candidates = [
            key for key, value in raw_fields.items()
            if key[0] == raw_row["owner_name"]
            and key[1] == raw_row[name_key]
            and value == raw_row[id_key]
        ]
        assert len(candidates) == 1, (raw_row, prefix, candidates)
        exact = candidates[0]
        assert canonical_row[id_key] == canonical_fields[exact]
        assert canonical_row[id_key] != raw_row[id_key]

# Preserve the valid native input envelope, but replace every declaration
# field and topology identity with the just-produced canonical epoch.
carrier = copy.deepcopy(raw)
for decl in carrier["decls"]:
    for field in decl["fields"]:
        exact = (decl["name"], field["name"], field["field_kind"])
        field["source_syntax_id"] = canonical_fields[exact]
carrier["domain_topology"] = copy.deepcopy(canonical_topology)
carrier_path.write_text(
    json.dumps(carrier, separators=(",", ":")), encoding="utf-8"
)

link = next(
    row for row in canonical_topology["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "link-relation"
)
raw_player = raw_fields[("BattleZone", "player", "subject_slot")]
canonical_player = canonical_fields[("BattleZone", "player", "subject_slot")]
canonical_enemy = canonical_fields[("BattleZone", "enemy", "subject_slot")]
assert link["left_slot_name"] == "player"
assert link["left_slot_source_syntax_id"] == canonical_player
assert raw_player != canonical_player != canonical_enemy

stale = copy.deepcopy(carrier)
stale_link = next(
    row for row in stale["domain_topology"]["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "link-relation"
)
stale_link["left_slot_source_syntax_id"] = raw_player
(output_dir / "stale-raw-player-id.mir.json").write_text(
    json.dumps(stale, separators=(",", ":")), encoding="utf-8"
)

foreign = copy.deepcopy(carrier)
foreign_link = next(
    row for row in foreign["domain_topology"]["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "link-relation"
)
foreign_link["left_slot_name"] = "player"
foreign_link["left_slot_source_syntax_id"] = canonical_enemy
(output_dir / "player-name-canonical-enemy-id.mir.json").write_text(
    json.dumps(foreign, separators=(",", ":")), encoding="utf-8"
)
PY

# Prove the canonical-epoch carrier is otherwise admissible. This prevents the
# two negative cases from passing merely because another input fact is invalid.
if ! (cd "$ROOT_DIR" && "$DRIVER_BIN" --canonicalize-mir-json \
    "${CANONICAL_CARRIER#$ROOT_DIR/}" >"$BUILD_DIR/carrier.out" \
    2>"$BUILD_DIR/carrier.err"); then
    cat "$BUILD_DIR/carrier.out" "$BUILD_DIR/carrier.err" >&2
    fail "unmodified canonical-epoch carrier was rejected"
fi

for mutation in \
    "$BUILD_DIR/stale-raw-player-id.mir.json" \
    "$BUILD_DIR/player-name-canonical-enemy-id.mir.json"; do
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" --canonicalize-mir-json \
        "${mutation#$ROOT_DIR/}" >"$mutation.out" 2>"$mutation.err"); then
        fail "canonical identity mutation was accepted: $(basename "$mutation")"
    fi
    grep -Fq 'MIR domain topology facts are missing or invalid' \
        "$mutation.out" "$mutation.err" \
        || fail "identity mutation missed the topology boundary: $(basename "$mutation")"
done

echo "[self-host-parity:canonical-identity-epoch] exact epoch remap and stale/foreign field-ID negatives ok"
