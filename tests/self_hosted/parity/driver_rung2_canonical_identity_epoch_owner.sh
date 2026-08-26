#!/usr/bin/env bash
# Executable negative ratchet for canonical MIR topology identity epochs.
# The valid carrier below starts from the self-host driver's typed MIR output.
# All declaration field and topology identities in the carrier are then
# atomically replaced by the canonicalizer's epoch.

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
EXPRESSION_IDENTITY_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_identity_epoch_owner.pgy"
EXPRESSION_GRAPH_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/expression_graph_fact_owner.pgy"
LANE_POLICY_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_graph_lane_policy_owner.pgy"
IDENTITY_RESOLUTION_OWNER="$ROOT_DIR/src/self_hosted/semantic/ast_expression_identity_resolution_owner.pgy"
EXECUTION_OWNER="$ROOT_DIR/src/self_hosted/compiler/canonical_mir_execution_owner.pgy"
DRIVER_OWNER="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_owner.pgy"
CARRIER_OWNER="$ROOT_DIR/src/self_hosted/mir/domain_topology_fact_owner.pgy"
DRIVER_SOURCE="$ROOT_DIR/src/self_hosted/compiler/driver_rung2_main.pgy"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/canonical_identity_epoch_gate"
PREBUILT_DRIVER="${PGY_SELFHOST_PREBUILT_DRIVER:-}"
DRIVER_BIN="$BUILD_DIR/driver_c.exe"
RAW_MIR="$BUILD_DIR/raw.mir.json"
VALID_MIR="$BUILD_DIR/valid-simple.mir.json"
CANONICAL_MIR="$BUILD_DIR/canonical.mir.json"
CANONICAL_CARRIER="$BUILD_DIR/canonical-epoch-carrier.mir.json"

mkdir -p "$BUILD_DIR"

for term in \
    'import "canonical_mir_identity_epoch_owner.pgy"' \
    'CanonicalMirIdentityArtifactFromTreeTextOrDie'; do
    grep -Fq -- "$term" "$DRIVER_OWNER" \
        || fail "driver import/consumer closure is missing: $term"
done
for term in \
    'CanonicalMirIdentityEpochArtifactFromTreeText' \
    'CanonicalMirIdentityEpochRebindProgramFacts'; do
    grep -Fq -- "$term" "$EXECUTION_OWNER" \
        || fail "canonical execution owner lost identity epoch consumption: $term"
done
grep -Fq -- 'import "canonical_mir_field_identity_epoch_owner.pgy"' \
    "$IDENTITY_OWNER" \
    || fail "tree/directive epoch owner does not import the field epoch owner"
grep -Fq -- 'CanonicalMirIdentityEpochRemapTopology' "$IDENTITY_OWNER" \
    || fail "tree/directive epoch owner lost topology composition"
grep -Fq -- 'ref constructors: SemanticAstNominalConstructorFacts' \
    "$IDENTITY_OWNER" \
    || fail "canonical rebind no longer consumes the captured constructor facts"
if grep -Fq -- 'SemanticAstNominalConstructorFactsFromArtifact(' \
    "$IDENTITY_OWNER"; then
    fail "canonical rebind reread the artifact after projection retirement"
fi
constructor_line="$(grep -nF -- \
    'SemanticAstNominalConstructorFactsFromArtifact(artifact)' \
    "$EXECUTION_OWNER" | head -1 | cut -d: -f1)"
projection_line="$(grep -nF -- \
    'DriverRung2MirProjectionFromVerifiedFactsObserved(' \
    "$EXECUTION_OWNER" | head -1 | cut -d: -f1)"
rebind_line="$(grep -nF -- \
    'CanonicalMirIdentityEpochRebindProgramFacts(' \
    "$EXECUTION_OWNER" | head -1 | cut -d: -f1)"
[[ -n "$constructor_line" && -n "$projection_line" && -n "$rebind_line" && \
    "$constructor_line" -lt "$projection_line" && \
    "$projection_line" -lt "$rebind_line" ]] \
    || fail "constructor facts must be captured before projection and consumed by rebind"
for term in \
    'admitted.domain_topology.layer_slot_source_syntax_ids[row]' \
    'admitted.domain_topology.target_slot_source_syntax_ids[row]' \
    'admitted.domain_topology.participant_slot_source_syntax_ids[row]'; do
    grep -Fq -- "$term" "$IDENTITY_OWNER" \
        || fail "canonical topology remap lost apply-dependent identity: $term"
done
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
    "$IDENTITY_OWNER" "$FIELD_IDENTITY_OWNER" \
    "$EXPRESSION_IDENTITY_OWNER"; then
    fail "canonical identity owner reopened numeric equality/offset remapping"
fi
for term in \
    'MirExpressionIdentityEpochFromArtifactWithSignatures' \
    'header.param_source_syntax_ids[param]' \
    'canonical_param_id' \
    'MirExpressionIdentityEpochLookup'; do
    grep -Fq -- "$term" "$EXPRESSION_IDENTITY_OWNER" \
        || fail "expression identity epoch lost exact routine/parameter join: $term"
done
for term in \
    'MirExpressionIdentityEpochRebindGraphForArtifact('; do
    grep -Fq -- "$term" "$EXPRESSION_GRAPH_OWNER" \
        || fail "MIR expression graph bypassed canonical identity rebinding: $term"
done
grep -Fq -- 'SemanticAstExpressionGraphLaneCarriesPersistedIdentity' \
    "$LANE_POLICY_OWNER" \
    || fail "producer-only identity carriage policy is missing"
grep -Fq -- 'surface_carries_identity' "$IDENTITY_RESOLUTION_OWNER" \
    || fail "semantic identity admission bypassed lane carriage policy"
grep -Fq -- 'graph.topology_row_count != ArrayLength(graph.topology.kinds)' \
    "$CARRIER_OWNER" \
    || fail "DIR-to-MIR topology carrier does not consume the typed row arrays"
if grep -Fq -- 'canonical MIR bridge cannot reconstruct non-empty domain topology' \
    "$DRIVER_OWNER"; then
    fail "canonicalizer retained the non-empty topology rejection"
fi

if [[ -n "$PREBUILT_DRIVER" ]]; then
    DRIVER_BIN="$(pgy_select_optional_exe_binary "$PREBUILT_DRIVER")"
    pgy_require_runnable_binary_here \
        "canonical-identity-epoch" "$DRIVER_BIN" \
        || fail "PGY_SELFHOST_PREBUILT_DRIVER is not runnable"
else
    if ! (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$DRIVER_SOURCE")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$DRIVER_BIN")" \
        >"$BUILD_DIR/driver.compile.log" 2>&1); then
        cat "$BUILD_DIR/driver.compile.log" >&2
        fail "driver build failed"
    fi
fi

(cd "$ROOT_DIR" && "$DRIVER_BIN" --emit-mir-json-verified \
    "${FIXTURE#$ROOT_DIR/}" \
    2>"$BUILD_DIR/raw.err" | tr -d '\r' >"$RAW_MIR")
[[ -s "$RAW_MIR" ]] || fail "self-host topology MIR was not produced"

"$PYTHON_BIN" - "$RAW_MIR" "$VALID_MIR" <<'PY'
import json
import pathlib
import sys

source, target = map(pathlib.Path, sys.argv[1:])
doc = json.loads(source.read_text(encoding="utf-8"))

# Move the self-produced carrier into a second, internally consistent source
# epoch. The canonicalizer must join by exact owner/name/field-kind identity,
# not by accidental numeric equality with the parser's current arena IDs.
owner_ids = {}
field_ids = {}
field_ids_by_source_id = {}
directive_ids = {}
for decl in doc["decls"]:
    for field in decl["fields"]:
        old_field_id = field["source_syntax_id"]
        field["source_syntax_id"] = old_field_id + 200000
        field_ids[(decl["name"], field["name"], old_field_id)] = \
            field["source_syntax_id"]
        field_ids_by_source_id[old_field_id] = field["source_syntax_id"]
for row in doc["domain_topology"]["rows"]:
    old_owner_id = row["owner_source_syntax_id"]
    if row["owner_name"] not in owner_ids:
        owner_ids[row["owner_name"]] = old_owner_id + 100000
    row["owner_source_syntax_id"] = owner_ids[row["owner_name"]]
    old_directive_id = row["source_syntax_id"]
    row["source_syntax_id"] += 300000
    directive_ids[(row["owner_name"], old_directive_id)] = \
        row["source_syntax_id"]
    for prefix in (
        "projection_slot", "source_slot", "layer_slot", "target_slot",
        "left_slot", "right_slot", "participant_slot",
    ):
        name = row[prefix + "_name"]
        old_id = row[prefix + "_source_syntax_id"]
        if name is not None:
            row[prefix + "_source_syntax_id"] = field_ids[
                (row["owner_name"], name, old_id)
            ]

# Runtime assignment rows are part of the same producer identity epoch. Keep
# them exact with the transformed declaration/topology carrier so the positive
# fixture reaches canonicalization instead of failing machine admission first.
runtime = doc["domain_runtime_assignments"]
for role in runtime["participant_roles"]:
    old_field_id = role["field_syntax_id"]
    role["owner_syntax_id"] = owner_ids[role["owner_name"]]
    role["field_syntax_id"] = field_ids[
        (role["owner_name"], role["field_name"], old_field_id)
    ]
for member in runtime["projection_members"]:
    owner = member["owner_name"]
    member["owner_syntax_id"] = owner_ids[owner]
    member["directive_syntax_id"] = directive_ids[
        (owner, member["directive_syntax_id"])
    ]
    member["projection_slot_syntax_id"] = field_ids[
        (owner, member["projection_slot_name"],
         member["projection_slot_syntax_id"])
    ]
    member["source_slot_syntax_id"] = field_ids[
        (owner, member["source_slot_name"], member["source_slot_syntax_id"])
    ]
    member["target_field_syntax_id"] = field_ids_by_source_id[
        member["target_field_syntax_id"]
    ]
    for segment in member["source_path_segments"]:
        segment["field_syntax_id"] = field_ids_by_source_id[
            segment["field_syntax_id"]
        ]

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

def topology_owner_map(doc):
    owners = {}
    for row in doc["domain_topology"]["rows"]:
        owner = row["owner_name"]
        source_id = row["owner_source_syntax_id"]
        if owner in owners:
            assert owners[owner] == source_id, (owner, owners[owner], source_id)
        owners[owner] = source_id
    return owners

raw_fields = declaration_field_map(raw)
canonical_fields = declaration_field_map(canonical)
raw_owners = topology_owner_map(raw)
canonical_owners = topology_owner_map(canonical)
raw_topology = raw["domain_topology"]
canonical_topology = canonical["domain_topology"]
assert raw_topology["domain_graph_id"] == canonical_topology["domain_graph_id"]
assert len(raw_topology["rows"]) == len(canonical_topology["rows"]) == 4

# A hosted-domain callable is a direct declaration child in the source parser.
# Its canonical reconstruction must preserve that exact tree position instead
# of inserting a synthetic Methods node and then compensating numerically.
raw_show = [
    row for row in raw["routines"]
    if row.get("owner") == "BattleZone"
    and row.get("name") == "Show"
    and row.get("kind") == "method"
]
canonical_show = [
    row for row in canonical["routines"]
    if row.get("owner") == "BattleZone"
    and row.get("name") == "Show"
    and row.get("kind") == "method"
]
assert len(raw_show) == len(canonical_show) == 1
assert raw_show[0]["source_syntax_id"] == 27, raw_show
assert canonical_show[0]["source_syntax_id"] == \
    raw_show[0]["source_syntax_id"], (raw_show, canonical_show)

slot_prefixes = (
    "projection_slot", "source_slot", "layer_slot", "target_slot",
    "left_slot", "right_slot", "participant_slot",
)
for raw_row, canonical_row in zip(
    raw_topology["rows"], canonical_topology["rows"]
):
    assert raw_row["owner_name"] == canonical_row["owner_name"]
    assert raw_row["kind"] == canonical_row["kind"]
    assert raw_row["owner_source_syntax_id"] == raw_owners[raw_row["owner_name"]]
    assert canonical_row["owner_source_syntax_id"] == canonical_owners[canonical_row["owner_name"]]
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

# Preserve the valid self-host input envelope, but replace every declaration
# field and topology identity with the just-produced canonical epoch.
carrier = copy.deepcopy(raw)
for decl in carrier["decls"]:
    for field in decl["fields"]:
        exact = (decl["name"], field["name"], field["field_kind"])
        field["source_syntax_id"] = canonical_fields[exact]
carrier["domain_topology"] = copy.deepcopy(canonical_topology)
carrier["domain_runtime_assignments"] = copy.deepcopy(
    canonical["domain_runtime_assignments"]
)
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

apply = next(
    row for row in canonical_topology["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "apply-effect"
)
raw_poison = raw_fields[("BattleZone", "poison", "effect_slot")]
canonical_poison = canonical_fields[("BattleZone", "poison", "effect_slot")]
canonical_trust = canonical_fields[("BattleZone", "trust", "relation_slot")]
assert apply["layer_slot_name"] == "poison"
assert apply["layer_slot_source_syntax_id"] == canonical_poison
assert apply["target_slot_name"] == "player"
assert apply["target_slot_source_syntax_id"] == canonical_player
assert apply["participant_slot_name"] is None
assert apply["participant_slot_source_syntax_id"] == 0
assert raw_poison != canonical_poison != canonical_trust

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

stale_apply = copy.deepcopy(carrier)
stale_apply_row = next(
    row for row in stale_apply["domain_topology"]["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "apply-effect"
)
stale_apply_row["layer_slot_source_syntax_id"] = raw_poison
(output_dir / "stale-raw-poison-id.mir.json").write_text(
    json.dumps(stale_apply, separators=(",", ":")), encoding="utf-8"
)

wrong_kind_apply = copy.deepcopy(carrier)
wrong_kind_apply_row = next(
    row for row in wrong_kind_apply["domain_topology"]["rows"]
    if row["owner_name"] == "BattleZone" and row["kind"] == "apply-effect"
)
wrong_kind_apply_row["layer_slot_name"] = "poison"
wrong_kind_apply_row["layer_slot_source_syntax_id"] = canonical_trust
(output_dir / "poison-name-canonical-trust-id.mir.json").write_text(
    json.dumps(wrong_kind_apply, separators=(",", ":")), encoding="utf-8"
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
    "$BUILD_DIR/player-name-canonical-enemy-id.mir.json" \
    "$BUILD_DIR/stale-raw-poison-id.mir.json" \
    "$BUILD_DIR/poison-name-canonical-trust-id.mir.json"; do
    if (cd "$ROOT_DIR" && "$DRIVER_BIN" --canonicalize-mir-json \
        "${mutation#$ROOT_DIR/}" >"$mutation.out" 2>"$mutation.err"); then
        fail "canonical identity mutation was accepted: $(basename "$mutation")"
    fi
    grep -Fq 'MIR domain topology facts are missing or invalid' \
        "$mutation.out" "$mutation.err" \
        || fail "identity mutation missed the topology boundary: $(basename "$mutation")"
done

echo "[self-host-parity:canonical-identity-epoch] exact hosted-method tree ID, apply/link epoch remap, and stale/wrong-kind field-ID negatives ok"
