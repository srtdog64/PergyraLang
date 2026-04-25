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
        echo "missing python for parser/lexer diagnostic smoke" >&2
        exit 1
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

diag_header = root / "src" / "semantic" / "diag_codes.h"
diag_doc = root / "docs" / "72_diagnostic_codes.md"
driver = root / "src" / "compiler" / "driver_app.c"
header = diag_header.read_text(encoding="utf-8", errors="ignore")
doc = diag_doc.read_text(encoding="utf-8", errors="ignore")
driver_text = driver.read_text(encoding="utf-8", errors="ignore")
for literal in ("PGY_PARSE_SYNTAX", "PGY_LEX_INVALID_TOKEN"):
    if literal not in header:
        violations.append(f"diag_codes.h missing {literal}")
    if literal not in doc:
        violations.append(f"docs/72_diagnostic_codes.md missing {literal}")

for literal in ("PGY_CODE_PARSE_SYNTAX", "PGY_CODE_LEX_INVALID_TOKEN",
                "PGY_CAUSE_PARSE_UNEXPECTED_TOKEN", "PGY_CAUSE_LEX_INVALID_TOKEN",
                "PGY_FIX_CHECK_SYNTAX", "PGY_FIX_REMOVE_OR_ESCAPE_CHARACTER"):
    if literal not in driver_text:
        violations.append(f"driver_app.c missing JSON routing macro {literal}")

for literal in ("driver_diag_code_from_message", "driver_diag_cause_from_code",
                "driver_diag_fix_from_code", '"lex"', '"parse"'):
    if literal not in driver_text:
        violations.append(f"driver_app.c missing parser/lexer JSON routing term {literal}")

if violations:
    raise SystemExit("parser/lexer diagnostic smoke violations:\n" + "\n".join(violations))

print("[parser-lexer-diagnostic] stage codes and Code/Reason/Fix routing ok")
PY
