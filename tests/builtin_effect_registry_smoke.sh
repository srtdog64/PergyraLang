#!/usr/bin/env bash
# Fixed ambient-call policy only, not whole-language effect completeness.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"
PYTHON_BIN="${PYTHON_BIN:-python3}"
"$PYTHON_BIN" scripts/render_builtin_effect_registry.py --check
"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
from pathlib import Path
import re
import sys

root = Path(sys.argv[1])
registry = (root / "src/semantic/builtin_effect_registry.def").read_text(encoding="utf-8")
names = set(re.findall(r'PGY_BUILTIN_EFFECT\("([^"]+)"', registry))
consumers = [root / "src/semantic" / name for name in (
    "type_checker_builtins_stdlib_body.c", "type_checker_builtins_stdlib_scalar.c",
    "type_checker_builtins_nominal.c", "type_checker_builtins_stdlib_collections.c",
    "type_checker_builtins_stdlib_map.c")]
for consumer in consumers:
    text = consumer.read_text(encoding="utf-8")
    assert not re.search(r'semantic_record_effect\(ctx, EFFECT_(?:NONDETERMINISTIC|IO)\)', text), consumer
    for name in re.findall(r'semantic_record_builtin_effect\(ctx, (?:expr|call), "([^"]+)"\)', text):
        assert name in names, name
groups = (
    ("type_checker_builtins_stdlib_scalar.c", r'\{ "([A-Za-z0-9]+)", stdlib_scalar_check_', set()),
    ("type_checker_builtins_stdlib_collections.c", r'\{ "([A-Za-z0-9]+)", STDLIB_COLLECTION_', {"ArrayMap", "ArrayFilter"}),
    ("type_checker_builtins_stdlib_map.c", r'\{ "([A-Za-z0-9]+)", STDLIB_MAP_BUILTIN_', set()),
)
for filename, pattern, invoked_callbacks in groups:
    text = (root / "src/semantic" / filename).read_text(encoding="utf-8")
    dispatch = set(re.findall(pattern, text))
    assert dispatch and invoked_callbacks <= dispatch, filename
    assert not (dispatch - invoked_callbacks - names), (filename, dispatch - invoked_callbacks - names)
    assert not (invoked_callbacks & names), "callback effect classified as fixed"
    assert "semantic_record_builtin_effect(ctx, expr," in text, filename
projection = (root / "src/self_hosted/semantic/builtin_effect_projection_owner.pgy").read_text(encoding="utf-8")
assert "return -1;" in projection
assert "capability" not in projection.lower()
graph = (root / "src/self_hosted/semantic/ast_capability_call_graph_owner.pgy").read_text(encoding="utf-8")
assert "SemanticBuiltinFixedEffectMask(name)" in graph
assert "state.unknown_effects[callable_index] = true" in graph
facts = (root / "src/self_hosted/semantic/ast_capability_fact_owner.pgy").read_text(encoding="utf-8")
assert '"declared_effect_missing"' in facts and '"call_effect_fact_unavailable"' in facts
PY
echo 'fixed builtin effect registry smoke: ok'
