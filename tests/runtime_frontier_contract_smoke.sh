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
        echo "missing python for runtime frontier contract smoke" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import re
import sys

root = pathlib.Path(sys.argv[1])

c_zone = root / "src" / "codegen" / "transpiler_domain_nominal_emit.h"
c_zone_frontier = root / "src" / "codegen" / "transpiler_zone_decl_emit.h"
c_world = root / "src" / "codegen" / "transpiler_world_select_event_emit.h"
c_projection = root / "src" / "codegen" / "transpiler_domain_role_ability_emit.h"
llvm_domain = root / "src" / "codegen" / "llvm_domain.c"
llvm_projection = root / "src" / "codegen" / "llvm_domain_projection_sync_helpers.h"
abi_smoke = root / "tests" / "abi_pipeline_smoke.sh"
backend_compare = root / "tests" / "compare_backends.sh"
checklist = root / "docs" / "100_beta_readiness_checklist.md"
todo = root / "TODO.md"

for path in [
    c_zone,
    c_zone_frontier,
    c_world,
    c_projection,
    llvm_domain,
    llvm_projection,
    abi_smoke,
    backend_compare,
    checklist,
    todo,
]:
    if not path.exists():
        raise SystemExit(f"missing runtime frontier contract file: {path.relative_to(root)}")

c_zone_text = c_zone.read_text(encoding="utf-8")
c_zone_frontier_text = c_zone_frontier.read_text(encoding="utf-8")
c_world_text = c_world.read_text(encoding="utf-8")
c_zone_contract_text = c_zone_text + "\n" + c_zone_frontier_text
c_projection_text = c_projection.read_text(encoding="utf-8")
llvm_domain_text = llvm_domain.read_text(encoding="utf-8")
llvm_projection_text = llvm_projection.read_text(encoding="utf-8")
abi_text = abi_smoke.read_text(encoding="utf-8")
backend_text = backend_compare.read_text(encoding="utf-8")
checklist_text = checklist.read_text(encoding="utf-8")
todo_text = todo.read_text(encoding="utf-8")

required_c_zone_terms = [
    "_pgy_zone_frontier_pass_limit",
    "while (_pgy_zone_frontier_continue && _pgy_zone_frontier_pass < _pgy_zone_frontier_pass_limit)",
    "_pgy_zone_frontier_continue = true",
    "PGY_PANIC",
    "zone frontier recompute exceeded bounded pass limit",
]
required_c_world_terms = [
    "_pgy_world_frontier_pass_limit",
    "while (_pgy_world_frontier_continue && _pgy_world_frontier_pass < _pgy_world_frontier_pass_limit)",
    "_pgy_world_frontier_continue = true",
    "world frontier recompute exceeded bounded pass limit",
    "_pgy_world_pass_limit",
    "while (_pgy_world_continue && _pgy_world_pass < _pgy_world_pass_limit)",
    "PGY_PANIC",
    "world derived recompute exceeded bounded pass limit",
]
required_c_projection_terms = [
    "_pgy_%s_pass_limit",
    "while (_pgy_%s_continue && _pgy_%s_pass < _pgy_%s_pass_limit)",
    "PGY_PANIC",
    "projection recompute exceeded bounded pass limit",
]
required_llvm_domain_terms = [
    "zone.frontier.pass.addr",
    "zone.frontier.continue.addr",
    "zone.frontier.overflow",
    "world.frontier.pass.addr",
    "world.frontier.continue.addr",
    "world.frontier.overflow",
    "world.derived.overflow",
    'llvm_lookup_or_create_function(ctx, "abort"',
    "LLVMBuildUnreachable",
]
required_llvm_projection_terms = [
    "projection.loop.overflow",
    'llvm_lookup_or_create_function(ctx, "abort"',
    "LLVMBuildUnreachable",
]

def require_terms(label: str, content: str, terms: list[str]) -> None:
    normalized = re.sub(r"\s+", " ", content)
    missing = []
    for term in terms:
        normalized_term = re.sub(r"\s+", " ", term)
        if term not in content and normalized_term not in normalized:
            missing.append(term)
    if missing:
        raise SystemExit(f"{label} missing frontier contract term(s): " + ", ".join(missing))

require_terms("C zone frontier emitter", c_zone_contract_text, required_c_zone_terms)
require_terms("C world frontier emitter", c_world_text, required_c_world_terms)
require_terms("C projection frontier emitter", c_projection_text, required_c_projection_terms)
require_terms("LLVM world/zone frontier emitter", llvm_domain_text, required_llvm_domain_terms)
require_terms("LLVM projection frontier emitter", llvm_projection_text, required_llvm_projection_terms)

required_abi_cases = [
    "world_fixpoint_abi",
    "projection_chain_abi",
    "zone_frontier_abi",
    "world_embedded_projection_abi",
    "world_embedded_method_projection_abi",
    "world_embedded_branch_projection_abi",
    "world_embedded_action_frontier_abi",
    "world_embedded_action_pool_frontier_abi",
    "handoff_projection_frontier_abi",
    "handoff_world_state_frontier_abi",
    "handoff_layer_state_frontier_abi",
]
required_backend_cases = [
    "tests/cases/backend_compare/world_embedded_branch_projection_visibility",
    "tests/cases/backend_compare/world_embedded_action_frontier",
    "tests/cases/backend_compare/world_embedded_action_pool_frontier",
    "tests/cases/backend_compare/handoff_projection_frontier",
    "tests/cases/backend_compare/handoff_world_state_frontier",
    "tests/cases/backend_compare/handoff_layer_state_frontier",
]

require_terms("ABI pipeline frontier case registry", abi_text, required_abi_cases)
require_terms("backend-compare frontier case registry", backend_text, required_backend_cases)

required_docs_terms = [
    "world derived-state bounded recompute",
    "zone lifecycle bounded frontier loop",
    "projection-chain bounded recompute",
    "embedded world-zone action-caused layer/state freshness",
    "full bounded fixpoint / transitive frontier scheduler",
    "remaining authority/failure handoff family",
]
require_terms("beta checklist runtime frontier docs", checklist_text, required_docs_terms)
require_terms("TODO runtime frontier docs", todo_text, required_docs_terms)

for text, label in [(c_zone_text + c_world_text, "C emitter"), (llvm_domain_text, "LLVM emitter")]:
    if re.search(r"frontier.*single[- ]pass", text, flags=re.I):
        raise SystemExit(f"{label} contains single-pass frontier wording")

print("[runtime-frontier-contract] bounded C/LLVM frontier contracts are gated")
PY
