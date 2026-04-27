#!/usr/bin/env bash
# Regression gate for LLVM declaration-side MIR inventory usage.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

if [[ -z "$PY_BIN" ]]; then
    echo "[mir-decl-inventory] FAIL: python is required" >&2
    exit 1
fi

"$PY_BIN" - "$ROOT_DIR" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
internal_path = root / "src" / "codegen" / "llvm_internal.h"
inventory_internal_path = root / "src" / "codegen" / "llvm_inventory_internal.h"
pipeline_path = root / "src" / "codegen" / "llvm_pipeline.c"
domain_path = root / "src" / "codegen" / "llvm_domain.c"
backend_doc_path = root / "src" / "codegen" / "llvm_backend.h"
transpiler_header_path = root / "src" / "codegen" / "transpiler.h"
transpiler_path = root / "src" / "codegen" / "transpiler.c"
checklist_path = root / "docs" / "100_beta_readiness_checklist.md"
todo_path = root / "TODO.md"

for path in (
    internal_path,
    inventory_internal_path,
    pipeline_path,
    domain_path,
    backend_doc_path,
    transpiler_header_path,
    transpiler_path,
    checklist_path,
    todo_path,
):
    if not path.exists():
        raise SystemExit(f"[mir-decl-inventory] missing required file: {path.relative_to(root)}")

internal = internal_path.read_text(encoding="utf-8")
inventory_internal = inventory_internal_path.read_text(encoding="utf-8")
pipeline = pipeline_path.read_text(encoding="utf-8")
domain = domain_path.read_text(encoding="utf-8")
backend_doc = backend_doc_path.read_text(encoding="utf-8")
transpiler_header = transpiler_header_path.read_text(encoding="utf-8")
transpiler = transpiler_path.read_text(encoding="utf-8")
checklist = checklist_path.read_text(encoding="utf-8")
todo = todo_path.read_text(encoding="utf-8")

errors = []

required_internal_terms = [
    "llvm_active_inventory",
    "llvm_active_routine_inventory",
    "llvm_mir_routine_inventory_from_program",
    "llvm_find_decl_header_in_context",
    "llvm_find_host_decl_header_in_context",
    "llvm_host_decl_method_metadata",
    "llvm_find_host_method_metadata_in_context",
    "llvm_routine_inventory_get",
    "llvm_find_decl_in_active_inventory",
    "llvm_find_host_decl_in_active_inventory",
    "llvm_find_host_method_decl_in_context",
    "llvm_find_host_decl_methods_in_context",
    "llvm_active_nominal_inventory",
    "llvm_active_domain_inventory",
    "mir_find_decl_header(ctx->mir, name)",
    "llvm_is_host_decl_type",
]
for term in required_internal_terms:
    if term not in inventory_internal:
        errors.append(f"llvm_inventory_internal.h missing declaration inventory helper: {term}")

required_pipeline_terms = [
    "llvm_active_nominal_inventory(ctx, &nominal_nodes, &nominal_count)",
    "llvm_find_host_decl_methods_in_context(ctx, cls_name, &methods, &method_count)",
    "MIR-only LLVM path missing routine for class method",
    "MIR-only LLVM path missing routine for function",
    "declaration inventory is still AST-carried inside MIRProgram",
]
for term in required_pipeline_terms:
    if term not in pipeline:
        errors.append(f"llvm_pipeline.c missing MIR declaration inventory guard: {term}")

if "llvm_active_domain_inventory(ctx, &inventory)" not in domain:
    errors.append("llvm_domain.c must consume domain declarations through llvm_active_domain_inventory")

for term in [
    "declaration / top-level inventory is carried through MIRProgram",
    "dedicated declaration IR layer",
]:
    if term not in backend_doc:
        errors.append(f"llvm_backend.h must document remaining declaration inventory debt: {term}")

required_transpiler_terms = [
    "transpiler_active_inventory",
    "transpiler_active_externs",
    "transpiler_active_executables",
    "transpiler_active_synthetic_executable_func",
    "transpiler_active_has_main_function",
    "transpiler_active_has_top_level_exec",
]
for term in required_transpiler_terms:
    if term not in transpiler_header:
        errors.append(f"transpiler.h missing C backend active inventory helper: {term}")

for term in [
    "transpiler_active_inventory(ctx, AST_ABILITY_DECL, &abilities, &ability_count)",
    "transpiler_active_inventory(ctx, AST_CLASS_DECL, &types, &type_count)",
    "transpiler_active_inventory(ctx, AST_FUNC_DECL, &functions, &function_count)",
    "transpiler_active_inventory(ctx, AST_INTENT_DECL, &intents, &intent_count)",
    "transpiler_active_synthetic_executable_func(ctx)",
    "transpiler_active_has_main_function(ctx)",
    "transpiler_active_has_top_level_exec(ctx)",
]:
    if term not in transpiler:
        errors.append(f"transpiler.c must consume active executable metadata helper: {term}")

routine_raw_hits = []
for path in [
    root / "src" / "codegen" / "llvm_pipeline.c",
    root / "src" / "codegen" / "llvm_domain.c",
    root / "src" / "codegen" / "llvm_intent.c",
]:
    rel = path.relative_to(root).as_posix()
    text_for_routine = path.read_text(encoding="utf-8")
    if re.search(r"\bctx->mir->routine_count\b|\bctx->mir->routines\b|\bmir->routine_count\b|\bmir->routines\b", text_for_routine):
        routine_raw_hits.append(rel)

if routine_raw_hits:
    errors.append(
        "LLVM routine inventory must go through llvm_active_routine_inventory outside the helper owner: "
        + ", ".join(sorted(routine_raw_hits))
    )

if "decl_header->ast == decl" in inventory_internal:
    errors.append(
        "llvm_host_decl_methods must be MIRDeclHeader metadata-first; do not require decl_header->ast == decl"
    )

for term in [
    "llvm_mir_decl_method_name",
    "llvm_mir_decl_method_param_count",
    "llvm_mir_decl_method_param",
    "llvm_mir_decl_method_return_type",
    "llvm_mir_decl_method_is_action_like",
]:
    if term not in inventory_internal:
        errors.append(f"llvm_inventory_internal.h missing MIR method signature helper: {term}")

llvm_register = (root / "src" / "codegen" / "llvm_register.c").read_text(encoding="utf-8")
for term in [
    "llvm_mir_decl_method_param_count(method_meta, method)",
    "llvm_mir_decl_method_return_type(method_meta, method)",
    "llvm_mir_decl_method_is_action_like(method_meta, method)",
]:
    if term not in llvm_register:
        errors.append(f"llvm_register.c must consume MIR method signature metadata: {term}")

required_mir_terms = [
    "MIRDeclMethod",
    "method_metadata",
    "method_metadata_count",
    "mir_decl_header_set_methods",
    "mir_link_decl_method_routines",
    "params",
    "param_count",
    "return_type",
    "has_routine",
    "routine_index",
]
mir_header = (root / "src" / "compiler" / "mir.h").read_text(encoding="utf-8")
mir_public = (root / "src" / "compiler" / "mir_lower_public_api.h").read_text(encoding="utf-8")
mir_decl_headers = (root / "src" / "compiler" / "mir_decl_headers.h").read_text(encoding="utf-8")
for term in required_mir_terms:
    if term not in mir_header and term not in mir_public and term not in mir_decl_headers:
        errors.append(f"MIR declaration method metadata missing term: {term}")

metadata_branch = inventory_internal.split("decl = decl_header->ast;", 1)[0]
if "method->data.func_decl.name != NULL" in metadata_branch:
    errors.append(
        "LLVM host method lookup must compare MIRDeclMethod.name before AST func_decl name"
    )

for term in [
    "MIR Declaration Debt Removal",
    "AST-carried declaration inventory",
    "dedicated declaration metadata view",
    "make mir-declaration-inventory-test-smoke",
]:
    if term not in checklist:
        errors.append(f"beta checklist must track MIR declaration inventory term: {term}")

if "declaration-side MIR-only debt" not in todo:
    errors.append("TODO must keep declaration-side MIR-only debt visible")

domain_arrays = {
    "functions",
    "intents",
    "abilities",
    "roles",
    "parties",
    "rosters",
    "worlds",
    "relations",
    "effects",
    "zones",
    "events",
    "types",
}

allowed_raw_files = {
    "src/codegen/llvm_internal.h",
    "src/codegen/llvm_inventory_internal.h",
    "src/codegen/transpiler.h",
}

raw_hits = []
for path in list((root / "src" / "codegen").glob("llvm*.[ch]")) + [
    root / "src" / "codegen" / "transpiler.c",
    root / "src" / "codegen" / "transpiler.h",
]:
    rel = path.relative_to(root).as_posix()
    text = path.read_text(encoding="utf-8")
    if rel in allowed_raw_files:
        continue
    for name in domain_arrays:
        if re.search(rf"\bctx->mir->{name}\b|\bmir->{name}\b", text):
            raw_hits.append(f"{rel}: raw MIR declaration array access: {name}")

if raw_hits:
    errors.append(
        "raw MIR declaration inventory array access outside allowed owner files:\n  "
        + "\n  ".join(sorted(raw_hits))
    )

if errors:
    print("[mir-decl-inventory] FAIL", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("[mir-decl-inventory] OK: C/LLVM declaration inventory use is helper-gated")
PY
