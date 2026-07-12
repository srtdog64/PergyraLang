#!/usr/bin/env bash
# Validates the whole-compiler SoT owner outline and binds every registry row to
# live owner, consumer, gate, and Coq declaration symbols.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REGISTRY="$ROOT_DIR/docs/semantics/sot_owner_spine_registry.md"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[sot-owner-spine] Python is required for the registry gate" >&2
    exit 1
fi

"$PYTHON_BIN" - "$ROOT_DIR" "$REGISTRY" <<'PY'
from __future__ import annotations

import copy
import pathlib
import sys
from collections import Counter

root = pathlib.Path(sys.argv[1])
registry_path = pathlib.Path(sys.argv[2])
coq_path = root / "docs/semantics/proofs/SoTAuthority.v"
begin = "<!-- BEGIN sot-owner-spine-registry -->"
end = "<!-- END sot-owner-spine-registry -->"
statuses = {"ACTIVE", "BRIDGE", "CLOSED"}
expected_status_counts = Counter({"ACTIVE": 9, "BRIDGE": 6, "CLOSED": 7})
expected_pairs = {
    "source.module_graph": ("SFSourceModuleGraph", "SOModuleLoader"),
    "lexer.token_stream": ("SFTokenStream", "SOLexer"),
    "parser.syntax_provenance": ("SFSyntaxProvenanceTree", "SOParserAst"),
    "semantic.symbol_type_graph": ("SFSemanticSymbolTypeGraph", "SOSemanticAnalyzer"),
    "hir.typed_control_flow": ("SFHirTypedControlFlow", "SOHir"),
    "dir.domain_graph": ("SFDirDomainGraph", "SODir"),
    "rir.resource_transition_graph": ("SFRirResourceTransitionGraph", "SORir"),
    "mir.execution_graph": ("SFMirExecutionGraph", "SOMir"),
    "air.evidence_graph": ("SFAirEvidenceGraph", "SOAir"),
    "abi.layout_rows": ("SFAbiLayoutRows", "SOMirAbi"),
    "target.capability_profile": ("SFTargetCapabilityProfile", "SOTargetCapability"),
    "projection.verified_plan": ("SFProjectionPlan", "SOProjectionPlanner"),
    "diagnostic.catalog": ("SFDiagnosticCatalog", "SODiagnosticCatalog"),
    "artifact.zone": ("SFBackendArtifact", "SOArtifactZone"),
    "compatibility.evolution": ("SFCompatibilityEvolution", "SOCompatibilityEvolution"),
    "selfhost.initializer_expression_shape": ("SFInitializerExpressionShape", "SOSemanticLocalBinding"),
    "selfhost.collection_mutation_statement": ("SFCollectionMutationStatement", "SOSemanticStatement"),
    "selfhost.enum_declaration_rows": ("SFEnumDeclarationRows", "SOSemanticEnum"),
    "selfhost.nominal_declaration_rows": ("SFNominalDeclarationRows", "SOSemanticNominalConstructor"),
    "selfhost.role_declaration_rows": ("SFRoleDeclarationRows", "SOSemanticRole"),
    "selfhost.expression_runtime_usage_surface": ("SFExpressionRuntimeUsageSurface", "SOSemanticExpressionSurface"),
    "selfhost.type_runtime_usage_surface": ("SFTypeRuntimeUsageSurface", "SOSemanticTypeSurface"),
}


def fail(message: str) -> None:
    raise ValueError(message)


def evidence_ref(value: str, field: str) -> tuple[pathlib.Path, str]:
    if "#" not in value:
        fail(f"{field} must use path#required-text: {value}")
    rel, needle = value.split("#", 1)
    if not rel or not needle:
        fail(f"{field} has an empty path or required text: {value}")
    path = root / rel
    if not path.is_file():
        fail(f"{field} path does not exist: {rel}")
    if needle not in path.read_text(encoding="utf-8"):
        fail(f"{field} required text is absent from {rel}: {needle}")
    return path, needle


def parse_rows(text: str) -> list[dict[str, str]]:
    if begin not in text or end not in text:
        fail("registry markers are missing")
    body = text.split(begin, 1)[1].split(end, 1)[0]
    fields = (
        "owner_id",
        "fact_class",
        "stable_handle",
        "coq_fact",
        "coq_owner",
        "authority_path",
        "producer_term",
        "last_consumers",
        "forbidden_fallbacks",
        "enforcement_gate",
        "status",
        "open_reason",
    )
    rows: list[dict[str, str]] = []
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("```"):
            continue
        values = [part.strip() for part in line.split("|")]
        if len(values) != len(fields):
            fail(f"registry row must have {len(fields)} fields: {line}")
        rows.append(dict(zip(fields, values)))
    if not rows:
        fail("registry has no owner rows")
    return rows


def path_list(value: str, field: str) -> list[pathlib.Path]:
    entries = [item.strip() for item in value.split(",") if item.strip()]
    if not entries:
        fail(f"{field} must not be empty")
    paths = [root / entry for entry in entries]
    missing = [str(path.relative_to(root)) for path in paths if not path.is_file()]
    if missing:
        fail(f"{field} paths do not exist: {', '.join(missing)}")
    return paths


def validate(rows: list[dict[str, str]]) -> None:
    if {row["owner_id"] for row in rows} != set(expected_pairs):
        fail("owner id set drifted from the accepted 22-row compiler spine")
    if len(rows) != len(expected_pairs):
        fail("registry contains duplicate owner ids")

    coq_text = coq_path.read_text(encoding="utf-8")
    seen_facts: set[str] = set()
    for row in rows:
        owner_id = row["owner_id"]
        expected_fact, expected_owner = expected_pairs[owner_id]
        if (row["coq_fact"], row["coq_owner"]) != (expected_fact, expected_owner):
            fail(f"{owner_id}: Coq owner pair drifted")
        if row["coq_fact"] in seen_facts:
            fail(f"duplicate fact authority row: {row['coq_fact']}")
        seen_facts.add(row["coq_fact"])
        if row["coq_fact"] not in coq_text or row["coq_owner"] not in coq_text:
            fail(f"{owner_id}: Coq fact/owner constructor is missing")
        if row["status"] not in statuses:
            fail(f"{owner_id}: invalid status {row['status']}")
        if not row["stable_handle"] or not row["fact_class"]:
            fail(f"{owner_id}: stable handle and fact class are required")

        authority_path = root / row["authority_path"]
        if not authority_path.is_file():
            fail(f"{owner_id}: authority path does not exist: {row['authority_path']}")
        if row["producer_term"] not in authority_path.read_text(encoding="utf-8"):
            fail(f"{owner_id}: producer term missing from authority path")
        path_list(row["last_consumers"], f"{owner_id}: last_consumers")
        gate_path, _ = evidence_ref(row["enforcement_gate"], f"{owner_id}: enforcement_gate")

        forbidden = [
            item.strip() for item in row["forbidden_fallbacks"].split(",")
            if item.strip()
        ]
        if not forbidden:
            fail(f"{owner_id}: forbidden fallback set must not be empty")
        if row["status"] == "CLOSED":
            if row["open_reason"] != "none":
                fail(f"{owner_id}: CLOSED row must have open_reason=none")
            gate_text = gate_path.read_text(encoding="utf-8")
            for token in forbidden:
                if token not in gate_text:
                    fail(f"{owner_id}: CLOSED gate does not name fallback {token}")
        elif row["open_reason"] == "none" or not row["open_reason"]:
            fail(f"{owner_id}: open row must name its remaining reason")

    counts = Counter(row["status"] for row in rows)
    if counts != expected_status_counts:
        fail(f"status counts drifted: {dict(counts)}")


text = registry_path.read_text(encoding="utf-8")
rows = parse_rows(text)
validate(rows)


def must_reject(mutator, label: str) -> None:
    candidate = copy.deepcopy(rows)
    mutator(candidate)
    try:
        validate(candidate)
    except ValueError:
        return
    fail(f"self-test mutation was accepted: {label}")


must_reject(lambda rs: rs.pop(), "missing spine row")
must_reject(lambda rs: rs.append(copy.deepcopy(rs[0])), "duplicate owner id")
must_reject(lambda rs: rs[0].__setitem__("status", "UNKNOWN"), "invalid status")
must_reject(
    lambda rs: rs[0].__setitem__("authority_path", "missing/owner.c"),
    "missing authority path",
)
must_reject(
    lambda rs: rs[0].__setitem__("coq_owner", "SOMissingOwner"),
    "missing Coq owner",
)
must_reject(
    lambda rs: rs[-1].__setitem__("open_reason", "fallback still live"),
    "closed row with open reason",
)
must_reject(
    lambda rs: rs[-1].__setitem__("forbidden_fallbacks", "MissingFallbackToken"),
    "closed gate missing fallback token",
)

print(
    "[sot-owner-spine] 22 owner rows locked "
    "(CLOSED=7 BRIDGE=6 ACTIVE=9); mutations rejected"
)
PY
