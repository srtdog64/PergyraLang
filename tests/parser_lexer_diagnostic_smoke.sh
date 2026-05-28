#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

require_literal() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "[parser-lexer-diagnostic] $rel missing: $term" >&2
        exit 1
    }
}

run_literal_contract_smoke() {
    require_literal "src/parser/parser.c" "PGY_CODE_PARSE_SYNTAX"
    require_literal "src/parser/parser.c" "PGY_CAUSE_PARSE_UNEXPECTED_TOKEN"
    require_literal "src/parser/parser.c" "PGY_FIX_CHECK_SYNTAX"
    require_literal "src/parser/parser.c" "Lexer saved_lexer = *parser->lexer"
    require_literal "src/parser/parser.c" "*parser->lexer = saved_lexer"
    require_literal "src/lexer/lexer.c" "PGY_CODE_LEX_INVALID_TOKEN"
    require_literal "src/lexer/lexer.c" "PGY_CAUSE_LEX_INVALID_TOKEN"
    require_literal "src/lexer/lexer.c" "PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER"
    require_literal "src/semantic/diag_codes.h" "PGY_PARSE_SYNTAX"
    require_literal "src/semantic/diag_codes.h" "PGY_LEX_INVALID_TOKEN"
    require_literal "docs/72_diagnostic_codes.md" "PGY_PARSE_SYNTAX"
    require_literal "docs/72_diagnostic_codes.md" "PGY_LEX_INVALID_TOKEN"
    require_literal "src/compiler/driver_diag.c" "driver_diag_code_from_message"
    require_literal "src/compiler/driver_diag.c" "driver_diag_cause_from_code"
    require_literal "src/compiler/driver_diag.c" "driver_diag_fix_from_code"
    require_literal "src/parser/parser_expr_postfix.c" "Optional chaining '?.' is reserved but not implemented."
    require_literal "src/parser/parser_expr_postfix.c" "optional member provenance is not frozen"
    require_literal "src/parser/parser_expr_postfix.c" "Slicing 'xs[..]' is reserved but not implemented."
    require_literal "src/parser/parser_expr_postfix.c" "public slice ABI and ownership policy are not frozen"
    require_literal "src/parser/parser_expr.c" "Object/map literal syntax '{ ... }' is reserved but not implemented."
    require_literal "src/parser/parser_expr.c" "object/map literal ABI, field ownership, and collection key policy"
    require_literal "src/parser/parser_expr.c" "Spread/rest syntax '...' is reserved but not implemented."
    require_literal "src/parser/parser_expr.c" "spread/rest needs call ABI, ownership, and collection lowering policy"
    require_literal "src/parser/parser_type.c" "Generic parameter placeholder '_' is reserved but not implemented."
    require_literal "src/parser/parser_type.c" "generic parameter elision needs DAG-owned ambiguity diagnostics"
    require_literal "src/parser/parser_type.c" "Generic/type argument elision '_' is reserved but not implemented."
    require_literal "src/parser/parser_type.c" "type-argument elision must be backed by DAG evidence"
    require_literal "src/parser/parser_statement_dispatch.c" "Attribute syntax '@...' is reserved but not implemented."
    require_literal "src/parser/parser_statement_dispatch.c" "attribute metadata ownership is not frozen"
    require_literal "src/parser/parser_decl.c" "Default value arguments are reserved but not implemented."
    require_literal "src/parser/parser_decl.c" "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"
    require_literal "src/parser/parser_async.c" "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"
    require_literal "src/parser/parser_expr_lambda.c" "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"
    echo "[parser-lexer-diagnostic] stage codes and routing are source-gated (literal fallback)"
}

if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    else
        run_literal_contract_smoke
        exit 0
    fi
fi

"$PYTHON_BIN" - "$ROOT_DIR" <<'PY'
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
checks = [
    (
        root / "src" / "parser" / "parser.c",
        "PGY_CODE_PARSE_SYNTAX",
        "PGY_CAUSE_PARSE_UNEXPECTED_TOKEN",
        "PGY_FIX_CHECK_SYNTAX",
    ),
    (
        root / "src" / "lexer" / "lexer.c",
        "PGY_CODE_LEX_INVALID_TOKEN",
        "PGY_CAUSE_LEX_INVALID_TOKEN",
        "PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER",
    ),
]

violations = []
for path, code, cause, fix in checks:
    text = path.read_text(encoding="utf-8", errors="ignore")
    for token in (code, cause, fix):
        if token not in text:
            violations.append(f"{path.relative_to(root)} missing {token}")
    if "Code:" not in text or "Reason:" not in text or "Fix:" not in text:
        violations.append(f"{path.relative_to(root)} must emit Code/Reason/Fix")

parser_core = (root / "src" / "parser" / "parser.c").read_text(encoding="utf-8", errors="ignore")
for literal in ("Lexer saved_lexer = *parser->lexer", "*parser->lexer = saved_lexer"):
    if literal not in parser_core:
        violations.append("parser_peek_next must restore full Lexer state, including error state")

diag_header = root / "src" / "semantic" / "diag_codes.h"
diag_doc = root / "docs" / "72_diagnostic_codes.md"
driver_sources = [
    root / "src" / "compiler" / "driver_app.c",
    root / "src" / "compiler" / "driver_diag.c",
]
header = diag_header.read_text(encoding="utf-8", errors="ignore")
doc = diag_doc.read_text(encoding="utf-8", errors="ignore")
driver_text = "\n".join(path.read_text(encoding="utf-8", errors="ignore") for path in driver_sources)
for literal in ("PGY_PARSE_SYNTAX", "PGY_LEX_INVALID_TOKEN"):
    if literal not in header:
        violations.append(f"diag_codes.h missing {literal}")
    if literal not in doc:
        violations.append(f"docs/72_diagnostic_codes.md missing {literal}")

for literal in ("PGY_CODE_PARSE_SYNTAX", "PGY_CODE_LEX_INVALID_TOKEN",
                "PGY_CAUSE_PARSE_UNEXPECTED_TOKEN", "PGY_CAUSE_LEX_INVALID_TOKEN",
                "PGY_FIX_CHECK_SYNTAX", "PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER"):
    if literal not in driver_text:
        violations.append(f"driver diagnostic owners missing JSON routing macro {literal}")

for literal in ("driver_diag_code_from_message", "driver_diag_cause_from_code",
                "driver_diag_fix_from_code", '"lex"', '"parse"'):
    if literal not in driver_text:
        violations.append(f"driver diagnostic owners missing parser/lexer JSON routing term {literal}")

parser_expr = (root / "src" / "parser" / "parser_expr.c").read_text(encoding="utf-8", errors="ignore")
parser_expr_postfix = (root / "src" / "parser" / "parser_expr_postfix.c").read_text(encoding="utf-8", errors="ignore")
parser_type = (root / "src" / "parser" / "parser_type.c").read_text(encoding="utf-8", errors="ignore")
parser_stmt = (root / "src" / "parser" / "parser_statement_dispatch.c").read_text(encoding="utf-8", errors="ignore")
parser_decl = (root / "src" / "parser" / "parser_decl.c").read_text(encoding="utf-8", errors="ignore")
parser_async = (root / "src" / "parser" / "parser_async.c").read_text(encoding="utf-8", errors="ignore")
parser_lambda = (root / "src" / "parser" / "parser_expr_lambda.c").read_text(encoding="utf-8", errors="ignore")
reserved_expr_terms = (
    "Optional chaining '?.' is reserved but not implemented.",
    "optional member provenance is not frozen",
    "Slicing 'xs[..]' is reserved but not implemented.",
    "public slice ABI and ownership policy are not frozen",
    "Object/map literal syntax '{ ... }' is reserved but not implemented.",
    "object/map literal ABI, field ownership, and collection key policy",
    "Spread/rest syntax '...' is reserved but not implemented.",
    "spread/rest needs call ABI, ownership, and collection lowering policy",
)
for literal in reserved_expr_terms:
    if literal not in parser_expr and literal not in parser_expr_postfix:
        violations.append(f"parser expression owners missing reserved-syntax Reason/Fix term {literal}")

reserved_other_terms = (
    (parser_type, "parser_type.c", "Generic parameter placeholder '_' is reserved but not implemented."),
    (parser_type, "parser_type.c", "generic parameter elision needs DAG-owned ambiguity diagnostics"),
    (parser_type, "parser_type.c", "Generic/type argument elision '_' is reserved but not implemented."),
    (parser_type, "parser_type.c", "type-argument elision must be backed by DAG evidence"),
    (parser_stmt, "parser_statement_dispatch.c", "Attribute syntax '@...' is reserved but not implemented."),
    (parser_stmt, "parser_statement_dispatch.c", "attribute metadata ownership is not frozen"),
    (parser_decl, "parser_decl.c", "Default value arguments are reserved but not implemented."),
    (parser_decl, "parser_decl.c", "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"),
    (parser_async, "parser_async.c", "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"),
    (parser_lambda, "parser_expr_lambda.c", "value defaults need call ABI, overload/dispatch, and named-argument interaction policy"),
)
for text, label, literal in reserved_other_terms:
    if literal not in text:
        violations.append(f"{label} missing reserved-syntax Reason/Fix term {literal}")

if violations:
    raise SystemExit("parser/lexer diagnostic smoke violations:\n" + "\n".join(violations))

print("[parser-lexer-diagnostic] stage codes and Code/Reason/Fix routing ok")
PY
