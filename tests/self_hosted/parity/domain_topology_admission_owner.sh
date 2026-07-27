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
INPUT_OWNER="$ROOT_DIR/src/self_hosted/mir_lower/mir_json_input_owner.pgy"
MIR_LOWER_SRC="$ROOT_DIR/src/self_hosted/mir_lower/main.pgy"
FIXTURE="$ROOT_DIR/tests/cases/backend_compare/zone_layer_projection_runtime/main.pgy"
BUILD_DIR="$ROOT_DIR/.tmp/self_hosted/mir_lower/domain_topology_admission"
MIR_LOWER="$BUILD_DIR/mir_lower.exe"
VALID="$BUILD_DIR/valid.mir.json"

mkdir -p "$BUILD_DIR"

for term in 'struct MirDomainTopologyFacts' \
    'MirDomainTopologyFactsFromDocument' \
    'MirDomainTopologyRowDeclarationJoinReady' \
    'MirDomainTopologyRelationDeclarationReady'; do
    grep -Fq -- "$term" "$OWNER" || fail "missing typed topology owner term: $term"
done
grep -Fq -- 'MIR domain topology facts are missing or invalid' "$INPUT_OWNER" \
    || fail "MIR input boundary does not fail closed on topology"
if grep -Eq 'AstTree|source_path|ReadFile\(' "$OWNER"; then
    fail "topology admission reopened AST/source recovery"
fi

(cd "$ROOT_DIR" && "$PGY" \
    "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER_SRC")" \
    --backend=c -o "$(pgy_path_for_compiler "$PGY" "$MIR_LOWER")" \
    >/dev/null)
[[ -s "$MIR_LOWER" ]] || fail "mir_lower tool was not built"

(cd "$ROOT_DIR" && "$PGY" --mir-json \
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

doc = copy.deepcopy(base)
doc.pop("domain_topology")
mutations["missing-topology"] = doc

doc = copy.deepcopy(base)
doc["decls"] = [row for row in doc["decls"] if row.get("name") != "TrustedLink"]
mutations["missing-relation-owner"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][2]["kind"] = "unknown"
mutations["unknown-kind"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][2]["source_syntax_id"] = doc["domain_topology"]["rows"][0]["source_syntax_id"]
mutations["duplicate-directive-id"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][2]["layer_slot_source_syntax_id"] = 0
mutations["missing-required-slot-id"] = doc

doc = copy.deepcopy(base)
doc["domain_topology"]["rows"][2]["target_slot_source_syntax_id"] = 999
mutations["stray-unused-slot-id"] = doc

doc = copy.deepcopy(base)
for row in doc["decls"]:
    if row.get("name") == "TrustedLink":
        row["kind"] = "class"
mutations["relation-kind-drift"] = doc

for name, payload in mutations.items():
    path = os.path.join(output_dir, name + ".mir.json")
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(payload, stream, separators=(",", ":"))
        stream.write("\n")
PY

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

echo "[self-host-parity:domain-topology-admission] relation + 3-row topology admission and mutations are fail-closed"
