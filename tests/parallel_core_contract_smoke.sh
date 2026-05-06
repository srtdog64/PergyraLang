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
        echo "[parallel-core-contract] requires python3 or python for JSON contract validation" >&2
        exit 1
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1])


def read(rel):
    return (root / rel).read_text(encoding="utf-8")


def require(condition, message):
    if not condition:
        raise SystemExit(f"[parallel-core-contract] {message}")


def require_text(rel, needles):
    text = read(rel)
    for needle in needles:
        require(needle in text, f"{rel} missing: {needle}")
    return text


def reject_text(rel, needles):
    text = read(rel)
    for needle in needles:
        require(needle not in text, f"{rel} must not contain: {needle}")
    return text


taxonomy = require_text(
    "docs/99_language_module_taxonomy.md",
    [
        "`parallel` as the core execution primitive",
        "`parallel`: core execution primitive",
        "`parallel`만 core primitive다.",
        "user-facing language order is `parallel -> spawn -> async/await -> select/channel`.",
    ],
)
require("fiber/coroutine" in taxonomy, "taxonomy must keep fiber/coroutine below core")

manifest = json.loads(read("docs/language_module_manifest.json"))
modules = {entry["name"]: entry for entry in manifest["modules"]}
core = modules.get("pgy.core")
execution = modules.get("pgy.execution")
scheduler = modules.get("pgy.runtime.scheduler")

require(core is not None, "manifest missing pgy.core")
require(execution is not None, "manifest missing pgy.execution")
require(scheduler is not None, "manifest missing pgy.runtime.scheduler")
require("parallel" in core.get("surfaces", []), "pgy.core must include parallel")
require(execution.get("layer") == "execution-family", "pgy.execution must stay execution-family")
for surface in ["parallel", "spawn", "async", "await", "select", "channel", "cancellation"]:
    require(surface in execution.get("surfaces", []), f"pgy.execution missing {surface}")
require(scheduler.get("layer") == "runtime-mechanism", "scheduler must stay runtime-mechanism")
require(scheduler.get("beta_blocker") is False, "scheduler mechanism must not be a beta blocker")

cases = json.loads(read("docs/language_module_cases.json"))
case_by_path = {entry["path"]: entry for entry in cases["cases"]}
parallel_case = case_by_path.get("tests/cases/backend_compare/parallel_channel_sum/main.pgy")
require(parallel_case is not None, "module cases missing parallel_channel_sum")
for module in ["pgy.core", "pgy.execution", "pgy.foundation"]:
    require(module in parallel_case.get("modules", []), f"parallel_channel_sum missing module {module}")

compare = require_text(
    "tests/compare_backends.sh",
    [
        "PGY_BACKEND_COMPARE_RUN_TIMEOUT_SECONDS",
        "tests/cases/backend_compare/parallel_channel_sum",
        "tests/cases/backend_compare/parallel_channel_dual",
        "tests/cases/backend_compare/triple_paradigm",
    ],
)
require(compare.count("parallel_channel") >= 2, "backend compare must keep multiple parallel cases")

require_text(
    "src/codegen/transpiler_mir_stmt_emit.h",
    [
        "stmt->type == AST_PARALLEL_BLOCK",
        "resource ops are observability hooks, not semantic",
        "The residual statement must still lower the",
        "strcmp(resource_inst->name, \"IO\") == 0",
        "They do not emit the concrete builtin or",
    ],
)

require_text(
    "src/codegen/thread_pool_usage.c",
    [
        "pgy_mir_instruction_uses_thread_pool",
        "pgy_mir_program_uses_thread_pool",
        "mir_routine_inventory_from_program",
        "inventory_uses_thread_pool_surface",
        "ast_uses_thread_pool_surface",
        "inst->expr0",
        "inst->expr1",
    ],
)
reject_text(
    "src/codegen/thread_pool_usage.c",
    [
        "source_terminator_condition",
        "source_terminator_value",
        "source_statements",
        "source_statement_count",
        "Compatibility fallback for source-only MIR blocks",
    ],
)

require_text(
    "src/parser/ast_thread_pool_analysis.c",
    [
        "ast_uses_thread_pool_surface",
        "AST_AWAIT_EXPR",
        "AST_TASK_GROUP",
    ],
)

require_text(
    "src/codegen/transpiler_thread_pool.c",
    [
        "pgy_mir_program_uses_thread_pool(ctx->mir)",
    ],
)
reject_text(
    "src/codegen/transpiler_thread_pool.c",
    [
        "pgy_ast_uses_thread_pool(",
    ],
)

require_text(
    "src/codegen/llvm_pipeline.c",
    [
        "pgy_mir_program_uses_thread_pool(ctx->mir)",
    ],
)
reject_text(
    "src/codegen/llvm_pipeline.c",
    [
        "pgy_ast_uses_thread_pool(",
    ],
)

require_text(
    "tests/module_smoke.sh",
    [
        "parallel_ref_slot_conflict",
        "Parallel context slot conflict on 'left'",
    ],
)

require_text(
    "src/tests/semantic/test_semantic_parallel_context.cases.h",
    [
        "parallel-rejected: write-write slot conflict",
        "parallel-safe: read-write slot race warns",
        "parallel-rejected: SecureSlot access is capability-serialized",
        "parallel-rejected: DeviceSlot operations stay serialized",
        "parallel-rejected: derived secure-effect helper calls stay serialized",
        "parallel-rejected: token-capability helper calls stay serialized",
    ],
)

require_text(
    "src/tests/semantic/test_semantic_parallel_family.cases.h",
    [
        "async-suspension: await outside async context triggers error",
        "async-suspension: await inside async context passes",
        "parallel-family: spawn expression returns Future<T>",
        "select-readiness: non-channel case is rejected",
    ],
)

require_text(
    "docs/semantics/05_parallel_execution.md",
    [
        "`parallel` as the core execution primitive.",
        "Parallel Conflict Soundness",
        "Execution Backend Parity",
        "tests/parallel_core_contract_smoke.sh",
    ],
)

print("[parallel-core-contract] ok")
PY
