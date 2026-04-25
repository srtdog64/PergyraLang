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
        echo "missing python for beta readiness checklist smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
if not checklist_path.exists():
    raise SystemExit("missing docs/100_beta_readiness_checklist.md")

text = checklist_path.read_text(encoding="utf-8")

required_sections = [
    "## 0. Formal Semantics / Proof Obligations",
    "## 0b. Function CFG / Body Dataflow Closure",
    "## 0c. Core Language Semantic Closure",
    "## 0d. Runtime Panic And Secure Authority Invariants",
    "## 0e. User-Facing Beta Quality Gates",
]

required_terms = [
    "Intent closure:",
    "Zone/world/authority/handoff closure:",
    "Runtime panic/unwinding policy that must be frozen:",
    "Secure slot / authority invariant obligations:",
    "Diagnostic quality gate:",
    "Cross-platform support matrix:",
    "Stdlib beta freeze:",
    "Tooling beta conformance:",
    "Package/module resolver beta surface:",
    "Test quality gate:",
    "Performance gate:",
    "Observability/tracing schema gate:",
    "Docs freeze:",
    "Memory/concurrency model gate:",
    "String/unicode policy:",
]

missing_sections = [section for section in required_sections if section not in text]
if missing_sections:
    raise SystemExit("beta checklist missing section(s): " + ", ".join(missing_sections))

missing_terms = [term for term in required_terms if term not in text]
if missing_terms:
    raise SystemExit("beta checklist missing gate term(s): " + ", ".join(missing_terms))

if "현재 공식 beta readiness는 약 50%" not in text:
    raise SystemExit("beta readiness checklist must keep strict 50% readiness wording")

print("beta readiness checklist smoke: ok")
PY
