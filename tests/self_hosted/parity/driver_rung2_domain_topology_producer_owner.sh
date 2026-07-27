#!/usr/bin/env bash
# Owns the first non-empty self DIR topology producer rung. The fixture must
# reach typed self MIR and survive exact declaration-field admission without
# native topology JSON grafting or canonical identity repair. Runtime layer
# materialization is a later substitution rung.

domain_topology_row_owner="$ROOT_DIR/src/self_hosted/dir/domain_topology_row_owner.pgy"
domain_topology_mir_owner="$ROOT_DIR/src/self_hosted/mir/domain_topology_fact_owner.pgy"
constructor_signature_owner="$ROOT_DIR/src/self_hosted/semantic/ast_expression_environment_owner.pgy"
constructor_argument_owner="$ROOT_DIR/src/self_hosted/semantic/nominal_constructor_argument_policy_owner.pgy"
constructor_layout_owner="$ROOT_DIR/src/self_hosted/codegen/emission/nominal_struct_emit_owner.pgy"
constructor_call_owner="$ROOT_DIR/src/self_hosted/codegen/emission/expr_semantic_call_emit_owner.pgy"

for term in 'struct SelfDirDomainTopologyRows' \
    'SelfDirDomainTopologyRowsFromArtifact' \
    'SelfDirFieldSourceSyntaxId' \
    'SelfDirProjectionFieldSourceSyntaxId' \
    'topology_kind == "refresh"' \
    'topology_kind == "publish"' \
    'topology_kind != "bind"' \
    'SelfDirDomainTopologyRowsReady'; do
    grep -Fq -- "$term" "$domain_topology_row_owner" || {
        echo "[self-host-parity:driver-rung2] missing self DIR topology term: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
grep -Fq 'SemanticAstNominalConstructorFieldIsArgument' \
    "$constructor_signature_owner" || {
    echo "[self-host-parity:driver-rung2] zone constructor signature policy call drifted" >&2
    return 1 2>/dev/null || exit 1
}
for term in 'SemanticAstNominalConstructorFieldIsArgument' \
    'NominalFieldKindSubjectSlot()' 'NominalFieldKindObjectSlot()' \
    'NominalFieldKindTObjectSlot()' 'NominalFieldKindBindingSlot()'; do
    grep -Fq -- "$term" "$constructor_argument_owner" || {
        echo "[self-host-parity:driver-rung2] zone constructor input policy drifted: $term" >&2
        return 1 2>/dev/null || exit 1
    }
done
grep -Fq '=constructor_fields:' "$constructor_layout_owner" || {
    echo "[self-host-parity:driver-rung2] constructor field inventory projection drifted" >&2
    return 1 2>/dev/null || exit 1
}
grep -Fq '"constructor_fields"' "$constructor_call_owner" || {
    echo "[self-host-parity:driver-rung2] struct call reopened storage-field arity" >&2
    return 1 2>/dev/null || exit 1
}
if grep -Eq 'TypedAstArenaProvenanceText|native_oracle|ordinal|source_syntax_id[[:space:]]*\+' \
    "$domain_topology_row_owner"; then
    echo "[self-host-parity:driver-rung2] topology producer reopened provenance/oracle/ordinal identity" >&2
    return 1 2>/dev/null || exit 1
fi

pgy_selfhost_verify_driver_rung2_domain_topology_producer() {
    local backend="$1" base="$2" native_mir_json="$3"
    local self_mir_json="$4" driver_bin="$5"
    [[ "$base" == "zone_layer_projection_runtime" ]] || return 0
    if ! grep -Fq '"domain_graph_id":14937235025281185444' \
        "$native_mir_json"; then
        echo "[self-host-parity:driver-rung2] native topology anchor drifted before self producer comparison" >&2
        return 1
    fi

    local python_bin="${PYTHON_BIN:-python3}"
    local negative="$BUILD_DIR/zone_layer_projection_runtime_${backend}.foreign-field-id.mir.json"
    "$python_bin" - "$self_mir_json" "$negative" <<'PY'
import json, pathlib, sys

source = pathlib.Path(sys.argv[1])
negative = pathlib.Path(sys.argv[2])
doc = json.loads(source.read_text(encoding="utf-8"))
topology = doc["domain_topology"]
assert topology["domain_graph_id"] == 14937235025281185444, topology
rows = topology["rows"]
assert len(rows) == 3, rows
assert [row["kind"] for row in rows] == ["refresh", "publish", "link-relation"]

decls = {decl["name"]: decl for decl in doc["decls"]}
fields = {
    owner: {field["name"]: field for field in decls[owner]["fields"]}
    for owner in ("Poisoned", "TrustedLink", "BattleZone")
}

expected = [
    ("Poisoned", "view", "object_slot", "bearer", "subject_slot"),
    ("TrustedLink", "packet", "tobject_slot", "target", "subject_slot"),
]
for row, (owner, projection, projection_kind, source_name, source_kind) in zip(rows[:2], expected):
    assert row["owner_name"] == owner
    assert row["owner_source_syntax_id"] > 0 and row["source_syntax_id"] > 0
    assert fields[owner][projection]["field_kind"] == projection_kind
    assert row["projection_slot_source_syntax_id"] == fields[owner][projection]["source_syntax_id"]
    assert row["source_slot_source_syntax_id"] == fields[owner][source_name]["source_syntax_id"]
    assert fields[owner][source_name]["field_kind"] == source_kind

link = rows[2]
assert link["owner_name"] == "BattleZone"
assert link["owner_source_syntax_id"] > 0 and link["source_syntax_id"] > 0
for key, name, kind in (
    ("layer_slot", "trust", "relation_slot"),
    ("left_slot", "player", "subject_slot"),
    ("right_slot", "enemy", "subject_slot"),
):
    assert link[f"{key}_name"] == name
    assert link[f"{key}_source_syntax_id"] == fields["BattleZone"][name]["source_syntax_id"]
    assert fields["BattleZone"][name]["field_kind"] == kind

link["left_slot_source_syntax_id"] = fields["BattleZone"]["enemy"]["source_syntax_id"]
negative.write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")
PY

    local negative_out="${negative}.out" negative_err="${negative}.err"
    if (cd "$ROOT_DIR" && "$driver_bin" --mir-json \
        "$(pgy_selfhost_path_relative_to_root "$negative")" \
        >"$negative_out" 2>"$negative_err"); then
        echo "[self-host-parity:driver-rung2] $backend topology admitted player name with enemy field ID" >&2
        return 1
    fi
    grep -Fq 'MIR domain topology facts are missing or invalid' \
        "$negative_out" "$negative_err" || {
        echo "[self-host-parity:driver-rung2] $backend exact-field topology negative drifted" >&2
        cat "$negative_out" "$negative_err" >&2
        return 1
    }

    local legacy_mir="$BUILD_DIR/zone_layer_projection_runtime_${backend}.semicolon-legacy.mir.json"
    local legacy_err="${legacy_mir}.err"
    if ! (cd "$ROOT_DIR" && "$driver_bin" --emit-mir-json-verified \
        "tests/self_hosted/parity/fixture/domain_topology_semicolon_legacy.pgy" \
        >"$legacy_mir.raw" 2>"$legacy_err"); then
        echo "[self-host-parity:driver-rung2] $backend semicolon topology syntax failed" >&2
        cat "$legacy_mir.raw" "$legacy_err" >&2
        return 1
    fi
    tr -d '\r' <"$legacy_mir.raw" >"$legacy_mir"
    rm -f "$legacy_mir.raw"
    "$python_bin" - "$legacy_mir" <<'PY'
import json, pathlib, sys
doc = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
rows = doc["domain_topology"]["rows"]
assert [row["kind"] for row in rows] == ["refresh", "publish", "link-relation"]
PY

    local arity_source="tests/self_hosted/parity/fixture/zone_layer_constructor_arity_rejected.pgy"
    local arity_out="$BUILD_DIR/zone_layer_projection_runtime_${backend}.layer-arity.out"
    local arity_err="$BUILD_DIR/zone_layer_projection_runtime_${backend}.layer-arity.err"
    if (cd "$ROOT_DIR" && "$driver_bin" --emit-mir-json-verified \
        "$arity_source" >"$arity_out" 2>"$arity_err"); then
        echo "[self-host-parity:driver-rung2] $backend zone layer storage reentered constructor arity" >&2
        return 1
    fi
    grep -Fq 'call_arity_mismatch' "$arity_out" "$arity_err" || {
        echo "[self-host-parity:driver-rung2] $backend zone layer constructor negative drifted" >&2
        cat "$arity_out" "$arity_err" >&2
        return 1
    }

    local kind_negative_source=""
    for kind_negative_source in \
        "tests/self_hosted/parity/fixture/domain_topology_refresh_tobject_rejected.pgy" \
        "tests/self_hosted/parity/fixture/domain_topology_publish_object_rejected.pgy"; do
        local kind_negative_name
        kind_negative_name="$(basename "$kind_negative_source" .pgy)"
        local kind_negative_out="$BUILD_DIR/${kind_negative_name}_${backend}.out"
        local kind_negative_err="$BUILD_DIR/${kind_negative_name}_${backend}.err"
        if (cd "$ROOT_DIR" && "$driver_bin" --emit-mir-json-verified \
            "$kind_negative_source" >"$kind_negative_out" 2>"$kind_negative_err"); then
            echo "[self-host-parity:driver-rung2] $backend admitted projection directive with the wrong slot kind: $kind_negative_name" >&2
            return 1
        fi
        grep -Fq 'self-host DIR projection topology field identity is missing' \
            "$kind_negative_out" "$kind_negative_err" || {
            echo "[self-host-parity:driver-rung2] $backend projection-kind negative drifted: $kind_negative_name" >&2
            cat "$kind_negative_out" "$kind_negative_err" >&2
            return 1
        }
    done

}
