#!/usr/bin/env bash
# Proves the canonical TextMate grammar is an exact projection of registry
# rows owned for syntax highlighting, while editors/vscode stays a thin client.

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[vscode-language-graph] python3/python is required" >&2
    exit 1
fi

PYTHONDONTWRITEBYTECODE=1 "$PYTHON_BIN" -B - "$ROOT_DIR" <<'PY'
from __future__ import annotations

import json
from pathlib import Path
import re
import sys


root = Path(sys.argv[1])
registry_path = root / "src/lexer/language_keyword_registry.def"
grammar_path = root / "editor/vscode-pergyra/syntaxes/pergyra.tmLanguage.json"
canonical_package_path = root / "editor/vscode-pergyra/package.json"
thin_root = root / "editors/vscode"
thin_package_path = thin_root / "package.json"


def fail(message: str) -> None:
    raise SystemExit(f"[vscode-language-graph] {message}")


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as exc:
        fail(f"invalid JSON at {path.relative_to(root).as_posix()}: {exc}")


def macro_bodies(source: str) -> list[str]:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    source = re.sub(r"//.*?$", "", source, flags=re.MULTILINE)
    bodies: list[str] = []
    cursor = 0
    macro = "PGY_LANGUAGE_KEYWORD"
    while True:
        start = source.find(macro, cursor)
        if start < 0:
            return bodies
        opening = source.find("(", start + len(macro))
        if opening < 0:
            fail("keyword registry macro has no opening parenthesis")
        depth = 1
        quoted = False
        escaped = False
        index = opening + 1
        while index < len(source) and depth > 0:
            char = source[index]
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
            elif char == '"':
                quoted = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
            index += 1
        if depth != 0:
            fail("keyword registry has an unterminated macro row")
        bodies.append(source[opening + 1 : index - 1])
        cursor = index


def split_fields(body: str) -> list[str]:
    fields: list[str] = []
    start = 0
    depth = 0
    quoted = False
    escaped = False
    for index, char in enumerate(body):
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
        elif char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "," and depth == 0:
            fields.append(body[start:index].strip())
            start = index + 1
    fields.append(body[start:].strip())
    return fields


try:
    registry_source = registry_path.read_text(encoding="utf-8")
except (OSError, UnicodeDecodeError) as exc:
    fail(f"cannot read keyword registry: {exc}")

registry_rows = []
for row_index, body in enumerate(macro_bodies(registry_source), start=1):
    fields = split_fields(body)
    if len(fields) != 8:
        fail(f"registry row {row_index} has {len(fields)} fields, expected 8")
    spelling_match = re.fullmatch(r'"([a-z][a-z0-9_]*)"', fields[0])
    if spelling_match is None:
        fail(f"registry row {row_index} has invalid spelling {fields[0]!r}")
    registry_rows.append((spelling_match.group(1), fields[7]))

if len(registry_rows) != 145:
    fail(f"registry closure is incomplete: {len(registry_rows)} rows, expected 145")
registry_spellings = [spelling for spelling, _ in registry_rows]
if len(registry_spellings) != len(set(registry_spellings)):
    duplicates = sorted(
        spelling for spelling in set(registry_spellings)
        if registry_spellings.count(spelling) > 1
    )
    fail(f"registry contains duplicate spellings: {', '.join(duplicates)}")

highlighted = {
    spelling
    for spelling, tooling_flags in registry_rows
    if "PGY_KEYWORD_TOOLING_HIGHLIGHT" in tooling_flags
}

grammar = load_json(grammar_path)
repository = grammar.get("repository")
if not isinstance(repository, dict):
    fail("TextMate grammar repository is missing")

textmate_words: list[str] = []
for owner in ("keywords", "domain-keywords", "intent-keywords"):
    owner_value = repository.get(owner)
    if not isinstance(owner_value, dict):
        fail(f"TextMate repository owner is missing: {owner}")
    patterns = owner_value.get("patterns")
    if not isinstance(patterns, list):
        fail(f"TextMate owner {owner} has no pattern list")
    for pattern_index, pattern in enumerate(patterns):
        if not isinstance(pattern, dict) or not isinstance(pattern.get("match"), str):
            fail(f"TextMate owner {owner} pattern {pattern_index} has no match")
        match = re.fullmatch(r"\\b\(([^()]*)\)\\b", pattern["match"])
        if match is None:
            fail(f"TextMate owner {owner} pattern {pattern_index} is not a word list")
        words = match.group(1).split("|")
        if not words or any(re.fullmatch(r"[a-z][a-z0-9_]*", word) is None for word in words):
            fail(f"TextMate owner {owner} pattern {pattern_index} has a non-lowercase word")
        textmate_words.extend(words)

duplicate_textmate_words = sorted(
    word for word in set(textmate_words) if textmate_words.count(word) > 1
)
if duplicate_textmate_words:
    fail(f"TextMate keyword patterns contain duplicates: {', '.join(duplicate_textmate_words)}")

textmate_set = set(textmate_words)
for unowned in ("domain", "sync"):
    if unowned in textmate_set or unowned in highlighted:
        fail(f"parser-unowned word remains highlighted: {unowned}")

missing_in_textmate = sorted(highlighted - textmate_set)
unowned_in_textmate = sorted(textmate_set - highlighted)
if missing_in_textmate or unowned_in_textmate:
    details = []
    if missing_in_textmate:
        details.append("missing in TextMate: " + ", ".join(missing_in_textmate))
    if unowned_in_textmate:
        details.append("not registry-highlight-owned: " + ", ".join(unowned_in_textmate))
    fail("highlight projection drift; " + "; ".join(details))

canonical_package = load_json(canonical_package_path)
canonical_grammars = canonical_package.get("contributes", {}).get("grammars")
if not isinstance(canonical_grammars, list) or len(canonical_grammars) != 1:
    fail("canonical VS Code package must contribute exactly one grammar")
declared_path = canonical_grammars[0].get("path")
if declared_path != "./syntaxes/pergyra.tmLanguage.json":
    fail(f"canonical VS Code grammar path drifted: {declared_path!r}")

ignored_parts = {"node_modules", "out", "build", ".tmp", ".git"}
grammar_candidates = []
for base in (root / "editor", root / "editors"):
    for path in base.rglob("*"):
        if not path.is_file() or ignored_parts.intersection(path.parts):
            continue
        lower_name = path.name.lower()
        if "tmlanguage" in lower_name or (
            path.parent.name.lower() in {"grammar", "grammars", "syntax", "syntaxes"}
            and path.suffix.lower() in {".json", ".yaml", ".yml", ".plist"}
        ):
            grammar_candidates.append(path.resolve())
expected_grammar = grammar_path.resolve()
if grammar_candidates != [expected_grammar]:
    rendered = ", ".join(
        path.relative_to(root).as_posix() for path in sorted(grammar_candidates)
    )
    fail(f"full grammar must exist only at the canonical path; found: {rendered}")

thin_package = load_json(thin_package_path)
thin_contributes = thin_package.get("contributes", {})
if not isinstance(thin_contributes, dict):
    fail("thin VS Code client has a malformed contributes object")
for forbidden_key in ("grammars", "grammar", "syntaxes", "syntax", "tmLanguage"):
    if forbidden_key in thin_contributes:
        fail(f"thin VS Code client owns forbidden contribution: {forbidden_key}")
thin_manifest_text = json.dumps(thin_package, sort_keys=True).lower()
for forbidden_term in ('"grammars"', '"syntaxes"', "tmlanguage"):
    if forbidden_term in thin_manifest_text:
        fail(f"thin VS Code package manifest mentions grammar ownership: {forbidden_term}")
for forbidden_dir in ("grammar", "grammars", "syntax", "syntaxes"):
    if (thin_root / forbidden_dir).exists():
        fail(f"thin VS Code client contains forbidden {forbidden_dir}/ directory")

print(
    "[vscode-language-graph] ok "
    f"({len(registry_rows)} registry rows; {len(highlighted)} highlighted spellings; "
    "one canonical grammar; thin client has no grammar contribution)"
)
PY
