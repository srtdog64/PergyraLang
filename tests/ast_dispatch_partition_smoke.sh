#!/usr/bin/env bash
# Regression gate for backend AST surface trust.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PY_BIN=""
if command -v python3 >/dev/null 2>&1; then
    PY_BIN="$(command -v python3)"
elif command -v python >/dev/null 2>&1; then
    PY_BIN="$(command -v python)"
fi

if [[ -z "$PY_BIN" ]]; then
    for token in \
        AST_TYPE AST_CHANNEL_TYPE AST_EVENT_HANDLER_TYPE AST_FUTURE_TYPE \
        AST_WORLD_ACTIVATE AST_WORLD_DEACTIVATE AST_WORLD_MAINTAIN AST_WORLD_STATE \
        AST_ZONE_APPLY AST_ZONE_LINK AST_ZONE_DETACH AST_ZONE_UNLINK \
        AST_ZONE_REFRESH AST_ZONE_AUTHORITY AST_ZONE_STATE; do
        grep -Fq "$token" "$ROOT_DIR/docs/95_ast_dispatch_partition.md" || {
            echo "[ast-dispatch] docs/95_ast_dispatch_partition.md missing $token" >&2
            exit 1
        }
    done
    grep -Fq "llvm_set_error_at_with_hints" "$ROOT_DIR/src/codegen/llvm_expr.c" || {
        echo "[ast-dispatch] llvm_expr.c missing structured backend diagnostic" >&2
        exit 1
    }
    grep -Fq "llvm_set_error_at_with_hints" "$ROOT_DIR/src/codegen/llvm_stmt.c" || {
        echo "[ast-dispatch] llvm_stmt.c missing structured backend diagnostic" >&2
        exit 1
    }
    if grep -Fq "warning: unhandled expression" "$ROOT_DIR/src/codegen/llvm_expr.c" ||
       grep -Fq "warning: unhandled statement" "$ROOT_DIR/src/codegen/llvm_stmt.c"; then
        echo "[ast-dispatch] fallback must not be warning-only" >&2
        exit 1
    fi
    grep -Fq "not silent expression fallback" "$ROOT_DIR/src/codegen/llvm_expr.c" || {
        echo "[ast-dispatch] LLVM domain safety-net missing explicit reject text" >&2
        exit 1
    }
    if grep -R "lc_registry_" "$ROOT_DIR/src/parser" >/dev/null 2>&1 ||
       grep -R "lifecycle_state.h" "$ROOT_DIR/src/parser" >/dev/null 2>&1; then
        echo "[ast-dispatch] parser must not populate semantic lifecycle registry" >&2
        exit 1
    fi
    echo "[ast-dispatch] OK: source partition contract is gated (literal fallback)"
    exit 0
fi

"$PY_BIN" - "$ROOT_DIR" <<'PY'
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
ast_h = "\n".join(
    path.read_text(encoding="utf-8")
    for path in [
        root / "src/parser/ast.h",
        root / "src/parser/ast_types.h",
    ]
)
llvm_expr = (root / "src/codegen/llvm_expr.c").read_text(encoding="utf-8")
llvm_stmt = (root / "src/codegen/llvm_stmt.c").read_text(encoding="utf-8")
doc = (root / "docs/95_ast_dispatch_partition.md").read_text(encoding="utf-8")
parser_text = "\n".join(
    path.read_text(encoding="utf-8")
    for path in (root / "src/parser").glob("*.c")
)

ast_types = set(re.findall(r"\b(AST_[A-Z_]+)\b", ast_h))
stmt_cases = set(re.findall(r"\bcase\s+(AST_[A-Z_]+)\s*:", llvm_stmt))
expr_cases = set(re.findall(r"\bcase\s+(AST_[A-Z_]+)\s*:", llvm_expr))

type_annotation = {
    "AST_TYPE",
    "AST_CHANNEL_TYPE",
    "AST_EVENT_HANDLER_TYPE",
    "AST_FUTURE_TYPE",
}

metadata_only = {
    "AST_DOMAIN_SLOT",
    "AST_ROLE_SLOT",
    "AST_SYSTEMIC_SLOT",
    "AST_PARTY_METHOD",
    "AST_PARTY_SHARED",
    "AST_REQUIRE_FIELD",
    "AST_OVERRIDE_FUNC",
    "AST_WORLD_SYSTEMIC",
    "AST_WORLD_ZONE",
    "AST_ZONE_LAYER_SLOT",
    "AST_ZONE_MAINTAIN_EFFECT",
    "AST_ZONE_MAINTAIN_RELATION",
    "AST_ZONE_MAINTAIN_STATE",
    "AST_INTENT_INVOLVES",
    "AST_INTENT_STEP",
    "AST_INTENT_VALUE",
    "AST_MATCH_CASE",
}

top_level_skip = {
    "AST_FUNC_DECL",
    "AST_CLASS_DECL",
    "AST_ABILITY_DECL",
    "AST_ROLE_DECL",
    "AST_PARTY_DECL",
    "AST_ROSTER_DECL",
    "AST_WORLD_DECL",
    "AST_RELATION_DECL",
    "AST_EFFECT_DECL",
    "AST_ZONE_DECL",
    "AST_EVENT_DECL",
    "AST_INTENT_DECL",
    "AST_IMPORT_DECL",
    "AST_NAMESPACE_DECL",
    "AST_TYPE_ALIAS",
    "AST_LIFECYCLE_DECL",
    "AST_USE_DECL",
    "AST_INCLUDE_STMT",
    "AST_IMPL_ABILITY",
    "AST_EXTERN_BLOCK",
}

domain_runtime_safety_net = {
    "AST_WORLD_ACTIVATE",
    "AST_WORLD_DEACTIVATE",
    "AST_WORLD_MAINTAIN",
    "AST_WORLD_STATE",
    "AST_ZONE_APPLY",
    "AST_ZONE_LINK",
    "AST_ZONE_DETACH",
    "AST_ZONE_UNLINK",
    "AST_ZONE_REFRESH",
    "AST_ZONE_AUTHORITY",
    "AST_ZONE_STATE",
}

root_only = {"AST_PROGRAM"}
expected_partition = (
    type_annotation
    | metadata_only
    | top_level_skip
    | domain_runtime_safety_net
    | root_only
)

errors = []

missing_from_ast = sorted(expected_partition - ast_types)
if missing_from_ast:
    errors.append(f"partition references unknown AST types: {missing_from_ast}")

unexpected_absorbed = sorted((type_annotation | metadata_only | root_only) & (stmt_cases | expr_cases))
if unexpected_absorbed:
    errors.append(
        "type/metadata/root AST nodes must not be absorbed by LLVM stmt/expr cases: "
        + ", ".join(unexpected_absorbed)
    )

missing_stmt_skip = sorted(top_level_skip - stmt_cases)
if missing_stmt_skip:
    errors.append(
        "top-level declaration safety skip missing from llvm_emit_statement: "
        + ", ".join(missing_stmt_skip)
    )

top_level_expr_absorb = sorted(top_level_skip & expr_cases)
if top_level_expr_absorb:
    errors.append(
        "top-level declarations must not be expression-emitted: "
        + ", ".join(top_level_expr_absorb)
    )

missing_domain_stmt = sorted(domain_runtime_safety_net - stmt_cases)
missing_domain_expr = sorted(domain_runtime_safety_net - expr_cases)
if missing_domain_stmt:
    errors.append(
        "domain runtime safety-net missing from llvm_emit_statement: "
        + ", ".join(missing_domain_stmt)
    )
if missing_domain_expr:
    errors.append(
        "domain runtime diagnostic terminus missing from llvm_emit_expression: "
        + ", ".join(missing_domain_expr)
    )

for token in sorted(expected_partition):
    if token not in doc:
        errors.append(f"docs/95_ast_dispatch_partition.md does not mention {token}")

if "llvm_set_error_at_with_hints" not in llvm_expr or "llvm_set_error_at_with_hints" not in llvm_stmt:
    errors.append("LLVM stmt/expr defaults must emit structured backend diagnostics")

if "warning: unhandled expression" in llvm_expr or "warning: unhandled statement" in llvm_stmt:
    errors.append("LLVM stmt/expr fallback must not be warning-only")

if "not silent expression fallback" not in llvm_expr:
    errors.append("LLVM domain safety-net must explicitly reject silent expression fallback")

if "lc_registry_" in parser_text or "lifecycle_state.h" in parser_text:
    errors.append("parser must not populate semantic lifecycle registry")

if errors:
    print("[ast-dispatch] FAIL", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print(
    "[ast-dispatch] OK: "
    f"{len(ast_types)} AST types, "
    f"{len(top_level_skip)} top-level skip nodes, "
    f"{len(domain_runtime_safety_net)} domain safety-net nodes"
)
PY
