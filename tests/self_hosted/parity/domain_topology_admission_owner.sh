#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[self-host-parity:domain-topology-admission] $*" >&2
    exit 1
}

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "domain-topology-admission" "$PGY" \
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

OWNER="$ROOT_DIR/src/self_hosted/mir_lower/domain_topology_fact_owner.pgy"
DECL_INDEX="$ROOT_DIR/src/self_hosted/mir_lower/program_declaration_index_owner.pgy"
FIELD_INDEX="$ROOT_DIR/src/self_hosted/mir_lower/program_declaration_field_identity_index_owner.pgy"
INPUT_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/mir_json_input_owner.pgy"
MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/mir_lower/domain_topology_admission"
MIR_LOWER="${PGY_SELFHOST_PREBUILT_MIR_LOWER:-$BUILD_DIR/mir_lower.exe}"
VALID="$BUILD_DIR/valid.mir.json"

mkdir -p "$BUILD_DIR"

for term in 'struct MirDomainTopologyFacts' \
    'MirDomainTopologyFactsFromDocument' \
    'MirDomainTopologyRowDeclarationJoinReady' \
    'MirDomainTopologyRelationDeclarationReady'; do
    grep -Fq -- "$term" "$OWNER" || fail "missing typed topology owner term: $term"
done
for term in 'struct MirProgramDeclarationFieldIdentityIndex' \
    'BuildMirProgramDeclarationFieldIdentityIndexFromDeclarationSpans' \
    'MirProgramDeclarationFieldIdentityMatches'; do
    grep -Fq -- "$term" "$FIELD_INDEX" \
        || fail "missing declaration field identity term: $term"
done
[[ "$(wc -l <"$FIELD_INDEX" | tr -d ' ')" -le 300 ]] ||
    fail "declaration field identity owner exceeds 300 lines"
grep -Fq -- 'BuildMirProgramDeclarationFieldIdentityIndexFromDeclarationSpans' \
    "$DECL_INDEX" || fail "program declaration index does not compose field identity spans"
if grep -Eq 'BuildMirDocumentFactIndex|MirDeclArrayBounds|MirDeclArrayObjectFactTable' \
    "$FIELD_INDEX"; then
    fail "field identity index reopened declaration-array discovery"
fi
if grep -Fq -- 'MirDomainTopologyDeclarationFieldKind' "$OWNER"; then
    fail "topology admission retained name-only declaration field lookup"
fi
grep -Fq -- 'MIR domain topology facts are missing or invalid' "$INPUT_OWNER" \
    || fail "MIR input boundary does not fail closed on topology"
if grep -Eq 'AstTree|source_path|ReadFile\(' "$OWNER"; then
    fail "topology admission reopened AST/source recovery"
fi

if [[ -z "${PGY_SELFHOST_PREBUILT_MIR_LOWER:-}" ]]; then
    (cd "$ROOT_DIR" && "$PGY" \
        "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SRC")" \
        --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")" \
        >/dev/null)
fi
[[ -s "$MIR_LOWER" ]] || fail "mir_lower tool was not built"

(cd "$ROOT_DIR" && "$PGY" --test-native-mir-json-oracle \
    "$(pgy_path_for_compiler "$PGY" "$FIXTURE")" \
    2>/dev/null | tr -d '\r' >"$VALID")

(cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
    "${VALID#$ROOT_DIR/}" >"$BUILD_DIR/valid.out" \
    2>"$BUILD_DIR/valid.err") \
    || fail "self-host input owner rejected valid DIR-owned topology"
grep -Fq 'pgy.mir.v1 input verified' "$BUILD_DIR/valid.out" \
    || fail "valid topology admission marker is missing"

(cd "$ROOT_DIR" && "$MIR_LOWER" "${VALID#$ROOT_DIR/}" \
    >"$BUILD_DIR/reconstructed.ast" 2>"$BUILD_DIR/reconstructed.err") \
    || fail "mir_lower rejected relation declaration after topology admission"
for term in 'Relation: TrustedLink' 'SubjectSlot: source: Player' \
    'SubjectSlot: target: Player' 'TObjectSlot: packet: PlayerDto'; do
    grep -Fq -- "$term" "$BUILD_DIR/reconstructed.ast" \
        || fail "relation reconstruction lost: $term"
done

"$PYTHON_BIN" - "$VALID" "$BUILD_DIR" <<'PY'
import copy
import json
import os
import sys

valid_path, output_dir = sys.argv[1:]
with open(valid_path, encoding="utf-8") as stream:
    base = json.load(stream)

mutations = {}

rows = base["domain_topology"]["rows"]
apply_index = next(
    index for index, row in enumerate(rows)
    if row.get("kind") == "apply-effect"
    and row.get("owner_name") == "BattleZone"
)
link_index = next(
    index for index, row in enumerate(rows)
    if row.get("kind") == "link-relation"
    and row.get("owner_name") == "BattleZone"
)

doc = copy.deepcopy(base)
doc.pop("domain_topology")
mutations["missing-topology"] = doc

doc = copy.deepcopy(base)
doc["decls"] = [row for row in doc["decls"] if row.get("name") != "TrustedLink"]
mutations["missing-relation-owner"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][link_index]["kind"] = "unknown"
mutations["unknown-kind"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][link_index]["source_syntax_id"] = doc["domain_topology"]["rows"][0]["source_syntax_id"]
mutations["duplicate-directive-id"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][apply_index]["layer_slot_source_syntax_id"] = 0
mutations["missing-required-slot-id"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][link_index]["target_slot_source_syntax_id"] = 999
mutations["stray-unused-slot-id"] = doc

doc = copy.deepcopy(base)
for row in doc["decls"]:
    if row.get("name") == "TrustedLink":
        row["kind"] = "class"
mutations["relation-kind-drift"] = doc

declarations = {row.get("name"): row for row in base["decls"]}
battle = declarations.get("BattleZone")
assert isinstance(battle, dict), battle
battle_fields = {field.get("name"): field for field in battle.get("fields", [])}
player = battle_fields.get("player")
enemy = battle_fields.get("enemy")
assert isinstance(player, dict) and isinstance(enemy, dict), battle_fields
player_id = player.get("source_syntax_id")
enemy_id = enemy.get("source_syntax_id")
assert isinstance(player_id, int) and player_id > 0, player
assert isinstance(enemy_id, int) and enemy_id > 0, enemy
assert player_id != enemy_id, (player_id, enemy_id)
poison = battle_fields.get("poison")
trust = battle_fields.get("trust")
assert isinstance(poison, dict) and isinstance(trust, dict), battle_fields
poison_id = poison.get("source_syntax_id")
trust_id = trust.get("source_syntax_id")
assert poison.get("field_kind") == "effect_slot", poison
assert trust.get("field_kind") == "relation_slot", trust
assert isinstance(poison_id, int) and poison_id > 0, poison
assert isinstance(trust_id, int) and trust_id > 0, trust
assert poison_id != trust_id, (poison_id, trust_id)
apply_rows = [
    row for row in base["domain_topology"]["rows"]
    if row.get("kind") == "apply-effect"
    and row.get("owner_name") == "BattleZone"
]
assert len(apply_rows) == 1, apply_rows
assert apply_rows[0]["layer_slot_name"] == "poison", apply_rows[0]
assert apply_rows[0]["layer_slot_source_syntax_id"] == poison_id, apply_rows[0]
assert apply_rows[0]["target_slot_name"] == "player", apply_rows[0]
assert apply_rows[0]["target_slot_source_syntax_id"] == player_id, apply_rows[0]
link_rows = [
    row for row in base["domain_topology"]["rows"]
    if row.get("kind") == "link-relation" and row.get("owner_name") == "BattleZone"
]
assert len(link_rows) == 1, link_rows
assert link_rows[0]["left_slot_name"] == "player", link_rows[0]
assert link_rows[0]["left_slot_source_syntax_id"] == player_id, link_rows[0]

doc = copy.deepcopy(base)
link = next(
    row for row in doc["domain_topology"]["rows"]
    if row.get("kind") == "link-relation" and row.get("owner_name") == "BattleZone"
)
link["left_slot_name"] = "player"
link["left_slot_source_syntax_id"] = enemy_id
mutations["forged-player-name-enemy-id"] = doc

doc = copy.deepcopy(base)
apply = next(
    row for row in doc["domain_topology"]["rows"]
    if row.get("kind") == "apply-effect"
    and row.get("owner_name") == "BattleZone"
)
apply["layer_slot_name"] = "poison"
apply["layer_slot_source_syntax_id"] = trust_id
mutations["forged-poison-name-trust-id"] = doc

doc = copy.deepcopy(base)
field = next(
    field for row in doc["decls"] if row.get("name") == "BattleZone"
    for field in row.get("fields", []) if field.get("name") == "player"
)
field.pop("source_syntax_id")
mutations["missing-declaration-field-id"] = doc

doc = copy.deepcopy(base)
field = next(
    field for row in doc["decls"] if row.get("name") == "BattleZone"
    for field in row.get("fields", []) if field.get("name") == "player"
)
field["source_syntax_id"] = 0
mutations["non-positive-declaration-field-id"] = doc

doc = copy.deepcopy(base)
field = next(
    field for row in doc["decls"] if row.get("name") == "BattleZone"
    for field in row.get("fields", []) if field.get("name") == "player"
)
field["source_syntax_id"] = enemy_id
mutations["duplicate-declaration-field-id"] = doc

doc = copy.deepcopy(base)
field = next(
    field for row in doc["decls"] if row.get("name") == "BattleZone"
    for field in row.get("fields", []) if field.get("name") == "player"
)
field["field_kind"] = "object_slot"
mutations["declaration-field-kind-drift"] = doc

all_field_ids = [
    field.get("source_syntax_id")
    for declaration in base["decls"]
    for field in declaration.get("fields", [])
    if isinstance(field.get("source_syntax_id"), int)
]
maximum_field_id = max(all_field_ids)

doc = copy.deepcopy(base)
owner = next(row for row in doc["decls"] if row.get("name") == "BattleZone")
first = copy.deepcopy(owner["fields"][0])
first["name"] = "duplicate_id_probe"
owner["fields"].append(first)
mutations["nonadjacent-duplicate-declaration-field-id"] = doc

doc = copy.deepcopy(base)
owner = next(row for row in doc["decls"] if row.get("name") == "BattleZone")
first = copy.deepcopy(owner["fields"][0])
first["source_syntax_id"] = maximum_field_id + 1
owner["fields"].append(first)
mutations["nonadjacent-duplicate-declaration-field-name"] = doc

doc = copy.deepcopy(base)
duplicate_declaration = copy.deepcopy(doc["decls"][0])
duplicate_declaration["fields"] = []
doc["decls"].append(duplicate_declaration)
mutations["nonadjacent-duplicate-declaration-name"] = doc

accepted = copy.deepcopy(base)
accepted["decls"].append({
    "kind": "struct",
    "nominal_kind": "struct",
    "name": "FieldIdentityCrossOwnerProbe",
    "fields": [
        {
            "name": "player",
            "source_syntax_id": maximum_field_id + 3,
            "field_kind": "field",
            "type": "Int",
        },
        {
            "name": "descending_id_probe",
            "source_syntax_id": maximum_field_id + 2,
            "field_kind": "field",
            "type": "Int",
        },
    ],
})
with open(
    os.path.join(output_dir, "cross-owner-same-name-descending-id.accepted.json"),
    "w", encoding="utf-8", newline="\n"
) as stream:
    json.dump(accepted, stream, separators=(",", ":"))
    stream.write("\n")

for name, payload in mutations.items():
    path = os.path.join(output_dir, name + ".mir.json")
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(payload, stream, separators=(",", ":"))
        stream.write("\n")
PY

ACCEPTED="$BUILD_DIR/cross-owner-same-name-descending-id.accepted.json"
(cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
    "${ACCEPTED#$ROOT_DIR/}" >"$ACCEPTED.out" 2>"$ACCEPTED.err") \
    || fail "field identity owner rejected cross-owner name or descending unique id"
grep -Fq 'pgy.mir.v1 input verified' "$ACCEPTED.out" \
    || fail "field identity positive admission marker is missing"

for bad in "$BUILD_DIR"/*.mir.json; do
    [[ "$bad" != "$VALID" ]] || continue
    if (cd "$ROOT_DIR" && "$MIR_LOWER" --verify-input \
        "${bad#$ROOT_DIR/}") >"$bad.out" 2>"$bad.err"; then
        fail "self-host input owner admitted mutation: $(basename "$bad")"
    fi
    grep -Fq 'MIR domain topology facts are missing or invalid' \
        "$bad.out" "$bad.err" \
        || fail "mutation did not fail at topology boundary: $(basename "$bad")"
done

echo "[self-host-parity:domain-topology-admission] exact declaration field identity joins and topology mutations are fail-closed"
