#!/usr/bin/env python3
"""Verify the language registry and render its reserved lexer projection."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re


MACRO = "PGY_LANGUAGE_KEYWORD"
EXPECTED_ROW_COUNT = 145
EXPECTED_RESERVED_ROW_COUNT = 71
EXPECTED_CONTEXTUAL_ROW_COUNT = 71
EXPECTED_SOFT_ROW_COUNT = 3

RESERVED = "PGY_KEYWORD_CLASS_RESERVED"
CONTEXTUAL = "PGY_KEYWORD_CLASS_CONTEXTUAL"
SOFT = "PGY_KEYWORD_CLASS_SOFT"
TOKEN_NONE = "PGY_KEYWORD_TOKEN_NONE"

VALID_CONTEXTS = {
    "PGY_KEYWORD_CONTEXT_DECLARATION",
    "PGY_KEYWORD_CONTEXT_STATEMENT",
    "PGY_KEYWORD_CONTEXT_EXPRESSION",
    "PGY_KEYWORD_CONTEXT_TYPE",
    "PGY_KEYWORD_CONTEXT_CLAUSE",
    "PGY_KEYWORD_CONTEXT_MODULE",
    "PGY_KEYWORD_CONTEXT_INTENT_STEP",
    "PGY_KEYWORD_CONTEXT_ZONE_BODY",
    "PGY_KEYWORD_CONTEXT_NAME",
}
VALID_AXES = {
    "PGY_KEYWORD_AXIS_GENERAL",
    "PGY_KEYWORD_AXIS_RESOURCE",
    "PGY_KEYWORD_AXIS_EXECUTION",
    "PGY_KEYWORD_AXIS_DOMAIN",
    "PGY_KEYWORD_AXIS_TYPE_CONTRACT",
}
VALID_SUPPORT = {
    "PGY_KEYWORD_SUPPORT_NATIVE",
    "PGY_KEYWORD_SUPPORT_SELF_HOST",
}
VALID_TOOLING = {
    "PGY_KEYWORD_TOOLING_COMPLETION",
    "PGY_KEYWORD_TOOLING_HOVER",
    "PGY_KEYWORD_TOOLING_HIGHLIGHT",
}


@dataclass(frozen=True)
class KeywordRow:
    spelling: str
    keyword_class: str
    token_type: str
    debug_identity: str
    context_mask: str
    axis: str
    implementation_support: str
    tooling_flags: str


def reserved_rows(rows: list[KeywordRow]) -> list[KeywordRow]:
    projected = [row for row in rows if row.keyword_class == RESERVED]
    if len(projected) != EXPECTED_RESERVED_ROW_COUNT:
        raise ValueError(
            f"registry has {len(projected)} reserved rows; "
            f"expected {EXPECTED_RESERVED_ROW_COUNT}"
        )
    if any(
        "PGY_KEYWORD_SUPPORT_SELF_HOST" not in row.implementation_support
        for row in projected
    ):
        raise ValueError("a reserved row is not marked self-host supported")
    return projected


def _macro_bodies(source: str) -> list[str]:
    bodies: list[str] = []
    cursor = 0
    while True:
        start = source.find(MACRO, cursor)
        if start < 0:
            return bodies
        opening = source.find("(", start + len(MACRO))
        if opening < 0:
            raise ValueError(f"{MACRO} without an opening parenthesis")

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
            raise ValueError(f"unterminated {MACRO} row")
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


def _plain_c_string(field: str, label: str) -> str:
    match = re.fullmatch(r'"([A-Za-z0-9_]+)"', field)
    if match is None:
        raise ValueError(f"{label} must be one plain C string, got: {field!r}")
    return match.group(1)


def _flag_terms(field: str, label: str, valid: set[str]) -> set[str]:
    terms = [term.strip() for term in field.split("|")]
    if not terms or any(not term for term in terms):
        raise ValueError(f"{label} is empty or malformed: {field!r}")
    if len(terms) != len(set(terms)):
        raise ValueError(f"{label} repeats a flag: {field!r}")
    unknown = set(terms) - valid
    if unknown:
        raise ValueError(f"{label} has unknown flags: {sorted(unknown)}")
    return set(terms)


def load_rows(registry: Path) -> list[KeywordRow]:
    source = registry.read_text(encoding="utf-8")
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    source = re.sub(r"//.*?$", "", source, flags=re.MULTILINE)
    rows: list[KeywordRow] = []
    for row_number, body in enumerate(_macro_bodies(source), start=1):
        fields = _split_fields(body)
        if len(fields) != 8:
            raise ValueError(
                f"registry row {row_number} has {len(fields)} fields; expected 8"
            )
        spelling = _plain_c_string(fields[0], f"row {row_number} spelling")
        keyword_class = fields[1]
        token_type = fields[2]
        debug_identity = _plain_c_string(
            fields[3], f"row {row_number} debug identity"
        )
        if keyword_class not in {RESERVED, CONTEXTUAL, SOFT}:
            raise ValueError(
                f"row {row_number} has unknown keyword class: {keyword_class}"
            )
        if keyword_class == RESERVED and token_type == TOKEN_NONE:
            raise ValueError(f"reserved row {row_number} has no token identity")
        if keyword_class != RESERVED and token_type != TOKEN_NONE:
            raise ValueError(
                f"non-reserved row {row_number} has lexer token identity {token_type}"
            )
        contexts = _flag_terms(
            fields[4], f"row {row_number} context mask", VALID_CONTEXTS
        )
        if not contexts:
            raise ValueError(f"row {row_number} has no grammar context")
        if fields[5] not in VALID_AXES:
            raise ValueError(
                f"row {row_number} has unknown semantic axis: {fields[5]}"
            )
        support = _flag_terms(
            fields[6], f"row {row_number} implementation support", VALID_SUPPORT
        )
        if "PGY_KEYWORD_SUPPORT_NATIVE" not in support:
            raise ValueError(f"row {row_number} is not marked native supported")
        if fields[7] != "0":
            _flag_terms(
                fields[7], f"row {row_number} tooling flags", VALID_TOOLING
            )
        rows.append(
            KeywordRow(
                spelling=spelling,
                keyword_class=keyword_class,
                token_type=token_type,
                debug_identity=debug_identity,
                context_mask=fields[4],
                axis=fields[5],
                implementation_support=fields[6],
                tooling_flags=fields[7],
            )
        )

    if len(rows) != EXPECTED_ROW_COUNT:
        raise ValueError(
            f"registry has {len(rows)} rows; expected {EXPECTED_ROW_COUNT}"
        )
    spellings = [row.spelling for row in rows]
    if spellings != sorted(spellings):
        raise ValueError("registry spellings are not bytewise sorted")
    if len(spellings) != len(set(spellings)):
        raise ValueError("registry contains duplicate spellings")
    identities = [row.debug_identity for row in rows]
    if any(re.fullmatch(r"[A-Z][A-Z0-9_]*", value) is None for value in identities):
        raise ValueError("registry contains a non-canonical debug identity")
    if any(row.debug_identity != row.spelling.upper() for row in rows):
        raise ValueError("registry debug identity does not match uppercase spelling")

    class_counts = {
        keyword_class: sum(row.keyword_class == keyword_class for row in rows)
        for keyword_class in (RESERVED, CONTEXTUAL, SOFT)
    }
    expected_counts = {
        RESERVED: EXPECTED_RESERVED_ROW_COUNT,
        CONTEXTUAL: EXPECTED_CONTEXTUAL_ROW_COUNT,
        SOFT: EXPECTED_SOFT_ROW_COUNT,
    }
    if class_counts != expected_counts:
        raise ValueError(
            f"registry class counts are {class_counts}; expected {expected_counts}"
        )
    reserved_tokens = [
        row.token_type for row in rows if row.keyword_class == RESERVED
    ]
    if len(reserved_tokens) != len(set(reserved_tokens)):
        duplicates = sorted(
            token for token in set(reserved_tokens)
            if reserved_tokens.count(token) > 1
        )
        raise ValueError(
            "registry contains duplicate reserved token identities: "
            + ", ".join(duplicates)
        )
    soft_spellings = {
        row.spelling for row in rows if row.keyword_class == SOFT
    }
    if soft_spellings != {"current", "full", "none"}:
        raise ValueError(
            "soft keyword set must be exactly current, full, none"
        )
    return rows


def render(rows: list[KeywordRow]) -> str:
    lines = [
        "// Generated by scripts/render_language_keyword_registry.py.",
        "// Source: src/lexer/language_keyword_registry.def. Do not edit by hand.",
        "// This checked-in projection gives the self-host lexer the same 71-word",
        "// spelling -> canonical debug-identity map as the native lexer.",
        "",
        "func LanguageKeywordRegistryCount() -> Int {",
        f"    return {len(rows)};",
        "}",
        "",
        "func LanguageKeywordSpellingAt(index: Int) -> String {",
    ]
    for index, row in enumerate(rows):
        lines.append(f'    if index == {index} {{ return "{row.spelling}"; }}')
    lines.extend([
        '    return "";',
        "}",
        "",
        "func LanguageKeywordDebugIdentityAt(index: Int) -> String {",
    ])
    for index, row in enumerate(rows):
        lines.append(f'    if index == {index} {{ return "{row.debug_identity}"; }}')
    lines.extend([
        '    return "";',
        "}",
        "",
        "func LanguageKeywordDebugIdentity(text: String) -> String {",
    ])
    for row in rows:
        lines.append(
            f'    if text == "{row.spelling}" {{ return "{row.debug_identity}"; }}'
        )
    lines.extend([
        '    return "IDENTIFIER";',
        "}",
        "",
        "func LanguageKeywordRegistryProjectionReady() -> Bool {",
        "    let index: Int = 0;",
        "    while index < LanguageKeywordRegistryCount() {",
        "        let spelling: String = LanguageKeywordSpellingAt(index);",
        "        let identity: String = LanguageKeywordDebugIdentityAt(index);",
        "        if StringLength(spelling) == 0 || StringLength(identity) == 0 {",
        "            return false;",
        "        }",
        "        if LanguageKeywordDebugIdentity(spelling) != identity {",
        "            return false;",
        "        }",
        "        index = index + 1;",
        "    }",
        '    return LanguageKeywordDebugIdentity("projection") == "IDENTIFIER";',
        "}",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("registry", type=Path)
    parser.add_argument("projection", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    expected = render(reserved_rows(load_rows(args.registry)))
    if args.check:
        if not args.projection.is_file():
            raise SystemExit(f"missing generated projection: {args.projection}")
        actual = args.projection.read_text(encoding="utf-8")
        if actual != expected:
            raise SystemExit(
                "self-host keyword projection drifted; regenerate with "
                f"{Path(__file__).as_posix()} {args.registry.as_posix()} "
                f"{args.projection.as_posix()} --write"
            )
        return 0

    args.projection.parent.mkdir(parents=True, exist_ok=True)
    args.projection.write_text(expected, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
