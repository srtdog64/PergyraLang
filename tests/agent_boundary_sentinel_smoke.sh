#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOC_REL="docs/169_agent_boundary_sentinel_library.md"
JSON_REL="docs/169_agent_boundary_sentinel_library.json"
FORTRAN_REL="docs/168_fortran_parallel_evidence.md"
INDEX_REL="docs/INDEX.md"

fail() {
    echo "[agent-boundary-sentinel] $*" >&2
    exit 1
}

require_text() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" ||
        fail "$rel missing term: $term"
}

for rel in "$DOC_REL" "$JSON_REL" "$FORTRAN_REL" "$INDEX_REL"; do
    [[ -f "$ROOT_DIR/$rel" ]] || fail "missing $rel"
done

require_text "$DOC_REL" "not a language feature"
require_text "$DOC_REL" "not a stdlib package"
require_text "$DOC_REL" "not part of the Fortran-derived"
require_text "$DOC_REL" "repository sentinel catalog"
require_text "$DOC_REL" "codebase-maintenance gate"
require_text "$DOC_REL" "language/compiler competitiveness axis"
require_text "$DOC_REL" "codebase gate for future"
require_text "$DOC_REL" "does not teach Pergyra parallel programming"
require_text "$DOC_REL" "pattern -> wrong_boundary -> turn_toward -> owner -> gate"
require_text "$DOC_REL" "Plane Split"
require_text "$DOC_REL" "machine-readable source"
require_text "$DOC_REL" "Adding a sentinel is allowed only when it names a real owner boundary."
require_text "$FORTRAN_REL" "language/compiler capability contract"
require_text "$FORTRAN_REL" "Pergyra competitiveness"
require_text "$FORTRAN_REL" "future CPU"
require_text "$FORTRAN_REL" "NPU"
require_text "$FORTRAN_REL" "There are two separate planes"
require_text "$FORTRAN_REL" "Language plane"
require_text "$FORTRAN_REL" "Repository-authoring plane"
require_text "$INDEX_REL" "169_agent_boundary_sentinel_library.md"
require_text "$INDEX_REL" "LLM/agent boundary sentinel library"

if grep -Fq "Agent Steering Sentinels" "$ROOT_DIR/$FORTRAN_REL"; then
    fail "$FORTRAN_REL must not own the LLM/agent sentinel library"
fi
if grep -Fq "169_agent_boundary_sentinel_library" "$ROOT_DIR/$FORTRAN_REL"; then
    fail "$FORTRAN_REL must stay focused on language data-parallel evidence"
fi

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        fail "missing python for JSON validation"
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR/$JSON_REL" <<'PY'
import json
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as fh:
    data = json.load(fh)

required_top = {
    "schema",
    "owner_doc",
    "plane",
    "not_language_feature",
    "separate_from",
    "status",
    "updated",
    "sentinels",
}
missing_top = required_top - set(data)
if missing_top:
    raise SystemExit("missing top-level fields: " + ", ".join(sorted(missing_top)))
if data["schema"] != "pgy.agent.boundary-sentinels.v1":
    raise SystemExit("unexpected schema")
if data["owner_doc"] != "docs/169_agent_boundary_sentinel_library.md":
    raise SystemExit("owner_doc drift")
if data["plane"] != "repository-authoring-gate":
    raise SystemExit("plane drift")
if data["not_language_feature"] is not True:
    raise SystemExit("not_language_feature drift")
if data["separate_from"] != "docs/168_fortran_parallel_evidence.md":
    raise SystemExit("separate_from drift")
if data["status"] != "repository-gate":
    raise SystemExit("status drift")

sentinels = data["sentinels"]
if not isinstance(sentinels, list) or len(sentinels) < 10:
    raise SystemExit("expected at least ten sentinel rows")

required_item = {
    "id",
    "if_pattern",
    "wrong_boundary",
    "why_wrong",
    "turn_toward",
    "owner",
    "gate_candidate",
}
ids = set()
for item in sentinels:
    missing = required_item - set(item)
    if missing:
        raise SystemExit(f"{item.get('id', '<unknown>')} missing fields: {sorted(missing)}")
    if item["id"] in ids:
        raise SystemExit("duplicate id: " + item["id"])
    ids.add(item["id"])
    for key in required_item:
        value = item[key]
        if not isinstance(value, str) or not value.strip():
            raise SystemExit(f"{item['id']} has empty {key}")

required_ids = {
    "fortran-parallel-plane-confusion",
    "semantic-source-reread",
    "backend-compat-fallback",
    "selfhost-main-artifact-parse",
    "hidden-runtime-materialization",
    "parallel-growable-raw-pointer",
    "lane-as-vector-proof",
    "backend-local-layout",
    "shell-semantic-oracle",
}
missing_ids = required_ids - ids
if missing_ids:
    raise SystemExit("missing sentinel ids: " + ", ".join(sorted(missing_ids)))
PY

echo "[agent-boundary-sentinel] structured boundary sentinels ok"
