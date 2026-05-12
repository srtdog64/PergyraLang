#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PYTHON_BIN="${PYTHON_BIN:-}"

require_literal() {
    local rel="$1"
    local term="$2"
    grep -Fq -- "$term" "$ROOT_DIR/$rel" || {
        echo "[diagnostic-registry] $rel missing: $term" >&2
        exit 1
    }
}

run_literal_contract_smoke() {
    require_literal "src/semantic/diag_codes.h" "PGY_CODE_"
    require_literal "src/semantic/diag_codes.h" "PGY_CAUSE_"
    require_literal "src/semantic/diag_codes.h" "PGY_FIX_"
    require_literal "docs/72_diagnostic_codes.md" "PGY_PARSE_SYNTAX"
    require_literal "docs/72_diagnostic_codes.md" "PGY_LEX_INVALID_TOKEN"
    require_literal "src/semantic/type_checker_diag.c" "semantic_error_with_hints"
    require_literal "src/semantic/type_checker_diag.c" "semantic_warning_with_hints"
    echo "[diagnostic-registry] macros and docs are source-gated (literal fallback)"
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
import re
import sys

root = pathlib.Path(sys.argv[1])
diag_header = root / "src" / "semantic" / "diag_codes.h"
diag_doc = root / "docs" / "72_diagnostic_codes.md"
semantic_root = root / "src" / "semantic"
callsite_skip = {
    pathlib.Path("src/semantic/diag_codes.h"),
    pathlib.Path("src/semantic/type_checker.h"),
    pathlib.Path("src/semantic/type_checker_diag.c"),
}

header_text = diag_header.read_text(encoding="utf-8")
doc_text = diag_doc.read_text(encoding="utf-8")

defines = dict(re.findall(
    r"^\s*#define\s+(PGY_(?:CODE|CAUSE|FIX)_[A-Z0-9_]+)\s+\"([^\"]+)\"",
    header_text,
    flags=re.M,
))

if not defines:
    raise SystemExit("diag_codes.h exposes no PGY_CODE/CAUSE/FIX macros")

missing_codes = [
    literal for name, literal in defines.items()
    if name.startswith("PGY_CODE_") and literal not in doc_text
]
if missing_codes:
    raise SystemExit(
        "docs/72_diagnostic_codes.md missing diagnostic code(s): "
        + ", ".join(sorted(missing_codes))
    )


def find_matching_paren(text: str, open_index: int) -> int:
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    for i in range(open_index, len(text)):
        ch = text[i]
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "'":
            in_char = True
            continue
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return i
    return -1


def split_top_level_args(arg_text: str) -> list[str]:
    args = []
    start = 0
    depth = 0
    in_string = False
    in_char = False
    escaped = False
    for i, ch in enumerate(arg_text):
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if in_char:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            continue
        if ch == '"':
            in_string = True
            continue
        if ch == "'":
            in_char = True
            continue
        if ch in "([{":
            depth += 1
            continue
        if ch in ")]}":
            depth -= 1
            continue
        if ch == "," and depth == 0:
            args.append(arg_text[start:i].strip())
            start = i + 1
    args.append(arg_text[start:].strip())
    return args


def strip_c_comments(text: str) -> str:
    out = []
    i = 0
    in_string = False
    in_char = False
    escaped = False
    while i < len(text):
        ch = text[i]
        nxt = text[i + 1] if i + 1 < len(text) else ""
        if in_string:
            out.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            i += 1
            continue
        if in_char:
            out.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == "'":
                in_char = False
            i += 1
            continue
        if ch == '"':
            in_string = True
            out.append(ch)
            i += 1
            continue
        if ch == "'":
            in_char = True
            out.append(ch)
            i += 1
            continue
        if ch == "/" and nxt == "/":
            while i < len(text) and text[i] != "\n":
                i += 1
            out.append("\n")
            continue
        if ch == "/" and nxt == "*":
            out.extend("  ")
            i += 2
            while i + 1 < len(text) and not (text[i] == "*" and text[i + 1] == "/"):
                out.append("\n" if text[i] == "\n" else " ")
                i += 1
            if i + 1 < len(text):
                out.extend("  ")
                i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


violations = []
call_names = ("semantic_error_with_hints", "semantic_warning_with_hints")
for path in sorted(semantic_root.rglob("*")):
    if path.suffix not in {".c", ".h"}:
        continue
    rel = path.relative_to(root)
    if rel in callsite_skip:
        continue
    text = strip_c_comments(path.read_text(encoding="utf-8", errors="ignore"))
    for call_name in call_names:
        pos = 0
        needle = call_name + "("
        while True:
            idx = text.find(needle, pos)
            if idx < 0:
                break
            open_idx = idx + len(call_name)
            close_idx = find_matching_paren(text, open_idx)
            if close_idx < 0:
                violations.append(f"{rel}: unterminated {call_name} call")
                break
            args = split_top_level_args(text[open_idx + 1:close_idx])
            for arg_index, prefix, label in (
                (1, "PGY_CODE_", "code"),
                (2, "PGY_CAUSE_", "cause_ir"),
                (3, "PGY_FIX_", "fix_source"),
            ):
                if len(args) <= arg_index:
                    continue
                value = args[arg_index].strip()
                if value in {"NULL", "0"}:
                    continue
                if not value.startswith(prefix):
                    violations.append(
                        f"{rel}: {call_name} {label} must use {prefix} macro, got {value[:80]}"
                    )
            pos = close_idx + 1

if violations:
    raise SystemExit(
        "diagnostic registry violations:\n" + "\n".join(violations[:80])
    )

print("[diagnostic-registry] macros and call sites ok")
PY
