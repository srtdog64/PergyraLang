#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        echo "missing python for projection diagnostic contract smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
semantic_test = root / "src" / "tests" / "semantic" / "test_semantic_projection_diagnostics.cases.h"
diagnostic_sources = [
    root / "src" / "semantic" / "type_checker_decls_domain_helpers.c",
    root / "src" / "semantic" / "type_checker_domain_projection.c",
]
projection_source = root / "src" / "semantic" / "type_checker_domain_projection.c"
proof_doc = root / "docs" / "semantics" / "02_relation_effect_projection.md"

for path in [semantic_test, *diagnostic_sources, proof_doc]:
    if not path.exists():
        raise SystemExit(f"missing required projection diagnostic contract file: {path.relative_to(root)}")

test_text = semantic_test.read_text(encoding="utf-8")
source_text = "\n".join(path.read_text(encoding="utf-8") for path in diagnostic_sources)
projection_text = projection_source.read_text(encoding="utf-8")
proof_text = proof_doc.read_text(encoding="utf-8")

required_tests = {
    "missing source field": [
        "zone refresh reports missing source field with structured diagnostic",
        "target field 'mana' is missing from source slot 'player'",
        "projection target 'playerView' expects field 'mana'",
        "add field 'mana' to source declaration 'Player'",
    ],
    "ambiguous source path": [
        "zone refresh reports ambiguous nested source field with structured diagnostic",
        "target field 'hp' is ambiguous in source slot 'player'",
        "multiple projection source paths match field 'hp'",
        "automatic projection cannot choose one path safely",
    ],
    "wrong projection kind": [
        "zone refresh reports wrong projection kind with structured diagnostic",
        "target slot 'playerPacket' uses the wrong projection kind",
        "refresh requires target slot 'playerPacket' to use object declaration",
        "actual target type 'PlayerPacket' uses a different projection kind",
    ],
    "duplicate field map": [
        "zone refresh reports duplicate field map with structured diagnostic",
        "projection map duplicates target field 'hp'",
        "each projection target field may be filled from exactly one source field",
        "keep a single mapping for 'hp'",
    ],
}

missing_test_terms = []
for case, terms in required_tests.items():
    for term in terms:
        if term not in test_text:
            missing_test_terms.append(f"{case}: {term}")
if missing_test_terms:
    raise SystemExit(
        "projection semantic regression missing required term(s): "
        + "; ".join(missing_test_terms)
    )

required_source_terms = [
    "Contract source:",
    "Reason:",
    "Fix:",
    "target slot '%s' uses the wrong projection kind",
    "projection map duplicates target field '%s'",
    "target field '%s' is ambiguous in source slot '%s'",
    "target field '%s' maps from missing source field '%s' in slot '%s'",
    "target field '%s' is missing from source slot '%s'",
    "projection consumer path is target slot '%s' <- source slot '%s'",
    "target projection declaration is '%s'",
]
missing_source_terms = [term for term in required_source_terms if term not in source_text]
if missing_source_terms:
    raise SystemExit(
        "projection diagnostic implementation missing required term(s): "
        + ", ".join(missing_source_terms)
    )

projection_error_count = projection_text.count("semantic_error_with_hints")
projection_contract_source_count = projection_text.count('"Contract source:\\n"')
if projection_error_count != projection_contract_source_count:
    raise SystemExit(
        "projection diagnostic implementation must give every projection "
        f"contract error a Contract source block: errors={projection_error_count}, "
        f"contract_source={projection_contract_source_count}"
    )

newly_gated_paths = [
    "source slot '%s' cannot be a tobject slot",
    "source slot '%s' is not a valid projection source",
    "projection map refers to unknown target field '%s'",
    "target field '%s' maps from missing source field '%s'",
    "target field '%s' cannot accept source path '%s'",
]
missing_new_paths = [term for term in newly_gated_paths if term not in projection_text]
if missing_new_paths:
    raise SystemExit(
        "projection diagnostic implementation missing newly gated path(s): "
        + ", ".join(missing_new_paths)
    )

required_proof_terms = [
    "## Theorem: Projection Diagnostic Completeness",
    "missing source field",
    "ambiguous path",
    "wrong projection kind",
    "duplicate field map",
]
missing_proof_terms = [term for term in required_proof_terms if term not in proof_text]
if missing_proof_terms:
    raise SystemExit(
        "projection proof doc missing required term(s): "
        + ", ".join(missing_proof_terms)
    )

print("[projection-diagnostic-contract] projection diagnostic contract is smoke-gated")
PY
