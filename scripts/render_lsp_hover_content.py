#!/usr/bin/env python3
"""Render and verify the self-host LSP hover presentation projection."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re


LANGUAGE_MACRO = "LANGUAGE_WORD_HOVER"
BUILTIN_MACRO = "BUILTIN_HOVER"
REGISTRY_MACRO = "PGY_LANGUAGE_KEYWORD"
EXPECTED_LANGUAGE_COUNT = 25
EXPECTED_BUILTINS = {"Err", "Log", "LogBanner", "LogBlock", "LogRaw", "Ok", "Unwrap"}


@dataclass(frozen=True)
class HoverRow:
    word: str
    markdown: str
    language_word: bool


def _without_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//.*?$", "", source, flags=re.MULTILINE)


def _macro_bodies(source: str, macro: str) -> list[str]:
    bodies: list[str] = []
    cursor = 0
    while True:
        start = source.find(macro, cursor)
        if start < 0:
            return bodies
        opening = source.find("(", start + len(macro))
        if opening < 0:
            raise ValueError(f"{macro} without an opening parenthesis")
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
            raise ValueError(f"unterminated {macro} row")
        bodies.append(source[opening + 1 : index - 1])
        cursor = index


def _split_fields(body: str) -> list[str]:
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


def _string_field(field: str, label: str) -> str:
    try:
        value = json.loads(field)
    except json.JSONDecodeError as exc:
        raise ValueError(f"{label} is not a plain string literal: {field!r}") from exc
    if not isinstance(value, str):
        raise ValueError(f"{label} is not a string")
    return value


def load_hover_rows(path: Path) -> list[HoverRow]:
    source = _without_comments(path.read_text(encoding="utf-8"))
    rows: list[HoverRow] = []
    for macro, language_word in (
        (LANGUAGE_MACRO, True),
        (BUILTIN_MACRO, False),
    ):
        for row_index, body in enumerate(_macro_bodies(source, macro), start=1):
            fields = _split_fields(body)
            if len(fields) != 2:
                raise ValueError(
                    f"{macro} row {row_index} has {len(fields)} fields; expected 2"
                )
            word = _string_field(fields[0], f"{macro} row {row_index} word")
            markdown = _string_field(fields[1], f"{macro} row {row_index} markdown")
            expected_pattern = r"[a-z][a-z0-9_]*" if language_word else r"[A-Z][A-Za-z0-9]*"
            if re.fullmatch(expected_pattern, word) is None:
                raise ValueError(f"{macro} row {row_index} has invalid word {word!r}")
            if not markdown.startswith("**") or len(markdown) < 5:
                raise ValueError(f"{macro} row {row_index} has empty/non-markdown prose")
            rows.append(HoverRow(word, markdown, language_word))

    language_rows = [row for row in rows if row.language_word]
    builtin_rows = [row for row in rows if not row.language_word]
    if len(language_rows) != EXPECTED_LANGUAGE_COUNT:
        raise ValueError(
            f"hover presentation has {len(language_rows)} language rows; "
            f"expected {EXPECTED_LANGUAGE_COUNT}"
        )
    builtin_words = {row.word for row in builtin_rows}
    if builtin_words != EXPECTED_BUILTINS or len(builtin_rows) != len(EXPECTED_BUILTINS):
        raise ValueError(
            "builtin hover rows must be exactly: " + ", ".join(sorted(EXPECTED_BUILTINS))
        )
    words = [row.word for row in rows]
    if len(words) != len(set(words)):
        raise ValueError("hover presentation contains duplicate words")
    if [row.word for row in language_rows] != sorted(row.word for row in language_rows):
        raise ValueError("language hover rows are not bytewise sorted")
    if [row.word for row in builtin_rows] != sorted(row.word for row in builtin_rows):
        raise ValueError("builtin hover rows are not bytewise sorted")
    return language_rows + builtin_rows


def load_hover_exposure(path: Path) -> set[str]:
    source = _without_comments(path.read_text(encoding="utf-8"))
    exposed: set[str] = set()
    spellings: set[str] = set()
    for row_index, body in enumerate(_macro_bodies(source, REGISTRY_MACRO), start=1):
        fields = _split_fields(body)
        if len(fields) != 9:
            raise ValueError(
                f"keyword registry row {row_index} has {len(fields)} fields; expected 9"
            )
        spelling = _string_field(fields[0], f"keyword registry row {row_index} spelling")
        if spelling in spellings:
            raise ValueError(f"keyword registry duplicates {spelling!r}")
        spellings.add(spelling)
        if "PGY_KEYWORD_TOOLING_HOVER" in fields[7]:
            exposed.add(spelling)
    return exposed


def _pgy_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    escaped = escaped.replace("\b", "\\b").replace("\f", "\\f")
    escaped = escaped.replace("\n", "\\n").replace("\r", "\\r")
    escaped = escaped.replace("\t", "\\t")
    return f'"{escaped}"'


def render(rows: list[HoverRow]) -> str:
    language_count = sum(row.language_word for row in rows)
    builtin_count = len(rows) - language_count
    lines = [
        "// Generated by scripts/render_lsp_hover_content.py.",
        "// Source: src/lsp/lsp_hover_content.def. Do not edit by hand.",
        "// The language keyword registry owns lowercase hover exposure; this",
        "// projection owns presentation prose only.",
        "",
        "func LspHoverPresentationLanguageWordCount() -> Int {",
        f"    return {language_count};",
        "}",
        "",
        "func LspHoverPresentationBuiltinCount() -> Int {",
        f"    return {builtin_count};",
        "}",
        "",
        "func LspHoverPresentationCount() -> Int {",
        f"    return {len(rows)};",
        "}",
        "",
        "func LspHoverPresentationWordAt(index: Int) -> String {",
    ]
    for index, row in enumerate(rows):
        lines.append(f"    if index == {index} {{ return {_pgy_string(row.word)}; }}")
    lines.extend([
        '    return "";',
        "}",
        "",
        "func LspHoverPresentationTextAt(index: Int) -> String {",
    ])
    for index, row in enumerate(rows):
        lines.append(f"    if index == {index} {{ return {_pgy_string(row.markdown)}; }}")
    lines.extend([
        '    return "";',
        "}",
        "",
        "func LspHoverPresentationLanguageWordAt(index: Int) -> Bool {",
    ])
    for index, row in enumerate(rows):
        value = "true" if row.language_word else "false"
        lines.append(f"    if index == {index} {{ return {value}; }}")
    lines.extend([
        "    return false;",
        "}",
        "",
        "func LspHoverPresentationTextForWord(word: String) -> Option<String> {",
    ])
    for row in rows:
        lines.append(
            f"    if word == {_pgy_string(row.word)} "
            f"{{ return Some({_pgy_string(row.markdown)}); }}"
        )
    lines.extend([
        "    return None;",
        "}",
        "",
        "func LspHoverPresentationProjectionReady() -> Bool {",
        f"    if LspHoverPresentationLanguageWordCount() != {language_count} {{",
        "        return false;",
        "    }",
        f"    if LspHoverPresentationBuiltinCount() != {builtin_count} {{",
        "        return false;",
        "    }",
        "    let index: Int = 0;",
        "    while index < LspHoverPresentationCount() {",
        "        let word: String = LspHoverPresentationWordAt(index);",
        "        let text: String = LspHoverPresentationTextAt(index);",
        "        if StringLength(word) == 0 || StringLength(text) == 0 {",
        "            return false;",
        "        }",
        "        let lookup: Option<String> = LspHoverPresentationTextForWord(word);",
        "        if !IsSome(lookup) || UnwrapOption(lookup) != text {",
        "            return false;",
        "        }",
        "        index = index + 1;",
        "    }",
        '    return !IsSome(LspHoverPresentationTextForWord("not_a_hover_word"));',
        "}",
        "",
    ])
    return "\n".join(lines)


def require_language_exposure(rows: list[HoverRow], exposed_words: set[str]) -> None:
    language_words = {row.word for row in rows if row.language_word}
    if language_words == exposed_words:
        return
    missing = sorted(exposed_words - language_words)
    unregistered = sorted(language_words - exposed_words)
    details = []
    if missing:
        details.append("missing presentation: " + ", ".join(missing))
    if unregistered:
        details.append("not HOVER-exposed: " + ", ".join(unregistered))
    raise SystemExit("language hover exposure drift; " + "; ".join(details))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("presentation", type=Path)
    parser.add_argument("language_registry", type=Path)
    parser.add_argument("projection", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    rows = load_hover_rows(args.presentation)
    exposed_words = load_hover_exposure(args.language_registry)
    require_language_exposure(rows, exposed_words)

    expected = render(rows)
    if args.check:
        if not args.projection.is_file():
            raise SystemExit(f"missing generated projection: {args.projection}")
        actual = args.projection.read_text(encoding="utf-8")
        if actual != expected:
            raise SystemExit(
                "self-host hover projection drifted; regenerate with "
                f"{Path(__file__).as_posix()} {args.presentation.as_posix()} "
                f"{args.language_registry.as_posix()} {args.projection.as_posix()} --write"
            )
        return 0

    args.projection.parent.mkdir(parents=True, exist_ok=True)
    args.projection.write_text(expected, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
