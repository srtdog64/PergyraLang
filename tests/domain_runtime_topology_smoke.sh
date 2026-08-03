#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths

fail() {
    echo "[domain-runtime-topology] $*" >&2
    exit 1
}

require_term() {
    local file="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$file" \
        || fail "missing '$term' in $file"
}

require_term "src/compiler/dir.h" "DIR_DOMAIN_TOPOLOGY_LINK_RELATION"
require_term "src/compiler/dir.h" "DIR_DOMAIN_TOPOLOGY_APPLY_EFFECT"
require_term "src/compiler/mir_domain_topology.h" \
    "MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT"
require_term "src/compiler/propagation_graph_build.c" \
    "case MIR_DOMAIN_TOPOLOGY_APPLY_EFFECT:"
require_term "src/compiler/mir.c" "mir_domain_topology_project_from_dir"
require_term "src/compiler/mir_json_dump.c" \
    "mir_json_emit_domain_topology(out, mir)"
require_term "src/compiler/mir_json_dump_decl.c" "AST_RELATION_DECL"
require_term "src/compiler/mir_json_dump_decl.c" \
    'mir_decl_field_source_syntax_id(field)'
require_term "src/self_hosted/mir_lower/mir_json_input_owner.pgy" \
    "MIR domain topology facts are missing or invalid"
require_term "src/compiler/driver_app.c" \
    "mir_lower_request_bind_dir(&mir_request, dir)"
require_term "src/codegen/transpiler_zone_decl_emit.c" \
    "pgy_codegen_zone_frontier_graph_pass_limit_from_mir"
require_term "src/codegen/llvm_domain_zone_sync.c" \
    "pgy_codegen_zone_frontier_graph_pass_limit_from_mir"

if grep -RInE \
    'propagation_graph_build_from_zone\(|pgy_codegen_zone_frontier_graph_pass_limit\(' \
    "$ROOT_DIR/src" >/dev/null; then
    fail "legacy backend AST zone graph entrypoint remains"
fi

if grep -nE 'ast_zone_(refreshes|maintained_effects|links)' \
    "$ROOT_DIR/src/compiler/propagation_graph_build.c" \
    "$ROOT_DIR/src/codegen/domain_frontier_graph.c" >/dev/null; then
    fail "zone frontier graph reconstructs DIR-owned topology from AST"
fi

if grep -nE 'ast_(zone|relation|effect|node|domain)_[a-z_]+\(|owner_node_id|DIRProgram|DIRDomainTopology' \
    "$ROOT_DIR/src/compiler/mir_json_dump_domain_topology.c" >/dev/null; then
    fail "MIR JSON topology emitter reopened AST/DIR recovery or leaked a DIR-local id"
fi

PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
PGY="$(pgy_select_optional_exe_binary "$PGY")"
pgy_require_runnable_binary_here "domain-runtime-topology" "$PGY" \
    || fail "PGY_BIN is not runnable"

BACKENDS="${PGY_DOMAIN_RUNTIME_TOPOLOGY_BACKENDS:-c llvm}"
ran_c=0
ran_llvm=0
for backend in $BACKENDS; do
    case "$backend" in
        c)
            ((ran_c == 0)) || fail "duplicate C topology backend"
            ran_c=1
            ;;
        llvm)
            ((ran_llvm == 0)) || fail "duplicate LLVM topology backend"
            ran_llvm=1
            ;;
        *)
            fail "unknown topology backend: $backend"
            ;;
    esac
done
((ran_c == 1)) || fail "C topology projection is required"

tmp_dir="$(mktemp -d)"
trap 'rm -rf -- "$tmp_dir"' EXIT

source_path="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
source_arg="$(pgy_path_for_compiler "$PGY" "$source_path")"
state_alias_source_path="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_state_alias/main.pgy"
state_alias_source_arg="$(pgy_path_for_compiler "$PGY" "$state_alias_source_path")"

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "python3/python is required for MIR topology JSON validation"
    fi
fi

mir_json="$tmp_dir/topology.mir.json"
"$PGY" --test-native-mir-json-oracle "$source_arg" \
    >"$mir_json" 2>"$tmp_dir/mir-json.log" \
    || fail "native MIR topology JSON emission failed"

"$PYTHON_BIN" - "$mir_json" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, encoding="utf-8") as stream:
    doc = json.load(stream)

relation = [row for row in doc.get("decls", []) if row.get("name") == "TrustedLink"]
assert len(relation) == 1, relation
relation = relation[0]
assert relation.get("kind") == "relation", relation
assert relation.get("nominal_kind") == "relation", relation
field_kinds = {field.get("name"): field.get("field_kind") for field in relation.get("fields", [])}
assert field_kinds == {
    "source": "subject_slot",
    "target": "subject_slot",
    "packet": "tobject_slot",
}, field_kinds

declarations = {row.get("name"): row for row in doc.get("decls", [])}
field_identities = {}
source_field_ids = set()
for owner_name, declaration in declarations.items():
    assert isinstance(owner_name, str) and owner_name, declaration
    for field in declaration.get("fields", []):
        field_name = field.get("name")
        field_kind = field.get("field_kind")
        source_id = field.get("source_syntax_id")
        assert isinstance(field_name, str) and field_name, field
        assert isinstance(field_kind, str) and field_kind, field
        assert isinstance(source_id, int) and source_id > 0, field
        assert source_id not in source_field_ids, (owner_name, field)
        source_field_ids.add(source_id)
        key = (owner_name, field_name)
        assert key not in field_identities, key
        field_identities[key] = (source_id, field_kind)

topology = doc.get("domain_topology")
assert isinstance(topology, dict), topology
assert isinstance(topology.get("domain_graph_id"), int)
assert topology["domain_graph_id"] > 0
rows = topology.get("rows")
assert isinstance(rows, list) and len(rows) == 4, rows
assert [(row.get("kind"), row.get("owner_name")) for row in rows] == [
    ("refresh", "Poisoned"),
    ("publish", "TrustedLink"),
    ("apply-effect", "BattleZone"),
    ("link-relation", "BattleZone"),
]
assert (rows[0]["projection_slot_name"], rows[0]["source_slot_name"]) == ("view", "bearer")
assert (rows[1]["projection_slot_name"], rows[1]["source_slot_name"]) == ("packet", "target")
assert (rows[2]["layer_slot_name"], rows[2]["target_slot_name"]) == ("poison", "player")
assert rows[2]["participant_slot_name"] is None
assert rows[2]["participant_slot_source_syntax_id"] == 0
assert (rows[3]["layer_slot_name"], rows[3]["left_slot_name"], rows[3]["right_slot_name"]) == ("trust", "player", "enemy")

name_fields = [
    "projection_slot", "source_slot", "layer_slot", "target_slot",
    "left_slot", "right_slot", "participant_slot",
]
source_ids = set()
for row in rows:
    assert row["owner_source_syntax_id"] > 0
    assert row["source_syntax_id"] > 0
    assert row["source_syntax_id"] not in source_ids
    source_ids.add(row["source_syntax_id"])
    for field in name_fields:
        name = row[f"{field}_name"]
        source_id = row[f"{field}_source_syntax_id"]
        assert (name is None and source_id == 0) or (
            isinstance(name, str) and name and isinstance(source_id, int) and source_id > 0
        ), (field, name, source_id)
        if name is not None:
            exact = field_identities.get((row["owner_name"], name))
            assert exact is not None, (row["owner_name"], field, name)
            assert exact[0] == source_id, (row["owner_name"], field, name, source_id, exact)
PY

state_alias_mir_json="$tmp_dir/topology-state-alias.mir.json"
"$PGY" --test-native-mir-json-oracle "$state_alias_source_arg" \
    >"$state_alias_mir_json" \
    2>"$tmp_dir/mir-json-state-alias.log" \
    || fail "native apply-state alias topology JSON emission failed"

"$PYTHON_BIN" - "$state_alias_mir_json" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as stream:
    doc = json.load(stream)

rows = doc["domain_topology"]["rows"]
apply = [row for row in rows if row.get("kind") == "apply-effect"]
assert len(apply) == 1, apply
apply = apply[0]
assert apply["owner_name"] == "BattleZone", apply
assert apply["layer_slot_name"] == "poison", apply
assert apply["layer_slot_source_syntax_id"] > 0, apply
assert apply["target_slot_name"] == "player", apply
assert apply["target_slot_source_syntax_id"] > 0, apply
PY

run_projection() {
    local backend="$1"
    local output="$tmp_dir/topology.${backend}"
    local output_arg
    local log="$tmp_dir/${backend}.log"
    output_arg="$(pgy_path_for_compiler "$PGY" "$output")"

    if [[ "$backend" == "c" ]]; then
        if ! PGY_DUMP_PROPAGATION=1 "$PGY" "$source_arg" \
            --emit-c -o "$output_arg" >"$log" 2>&1; then
            cat "$log" >&2
            fail "C topology projection failed"
        fi
    else
        if ! PGY_DUMP_PROPAGATION=1 "$PGY" "$source_arg" \
            --emit-llvm -o "$output_arg" >"$log" 2>&1; then
            cat "$log" >&2
            fail "LLVM topology projection failed"
        fi
    fi

    grep -Fq \
        '[propagation-graph] BattleZone: nodes=3 edges=2 acyclic depth=2 pass_limit=2' \
        "$log" || fail "$backend topology summary drifted"
    grep -Fq 'dep: trust <- player' "$log" \
        || fail "$backend topology lost player dependency"
    grep -Fq 'dep: trust <- enemy' "$log" \
        || fail "$backend topology lost enemy dependency"
    grep -E '^\[propagation-graph\]|^  propagation order:|^  dep:' \
        "$log" >"$tmp_dir/${backend}.trace"
}

run_projection c
if ((ran_llvm == 1)); then
    run_projection llvm
    cmp -s "$tmp_dir/c.trace" "$tmp_dir/llvm.trace" \
        || fail "C and LLVM consume different domain topology"
fi

grep -Fq 'size_t _pgy_zone_frontier_pass_limit = 3;' \
    "$tmp_dir/topology.c" \
    || fail "C output did not preserve the count floor above the graph depth"

if ((ran_llvm == 1)); then
    echo "[domain-runtime-topology] DIR -> MIR -> C/LLVM zone frontier topology and declaration field identity are exact; AST bypasses are absent"
else
    echo "[domain-runtime-topology] DIR -> MIR -> C zone frontier topology and declaration field identity are exact; LLVM is owned by an LLVM-enabled gate"
fi
