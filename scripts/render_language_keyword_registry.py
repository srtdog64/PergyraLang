#!/usr/bin/env python3
"""Verify and project the canonical language-word registry."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import json
from pathlib import Path
import re


MACRO = "PGY_LANGUAGE_KEYWORD"
EXPECTED_ROW_COUNT = 145
EXPECTED_RESERVED_ROW_COUNT = 70
EXPECTED_CONTEXTUAL_ROW_COUNT = 72
EXPECTED_SOFT_ROW_COUNT = 3

RESERVED = "PGY_KEYWORD_CLASS_RESERVED"
CONTEXTUAL = "PGY_KEYWORD_CLASS_CONTEXTUAL"
SOFT = "PGY_KEYWORD_CLASS_SOFT"
TOKEN_NONE = "PGY_KEYWORD_TOKEN_NONE"

CLASS_VALUES = {
    RESERVED: (1, "reserved"),
    CONTEXTUAL: (2, "contextual"),
    SOFT: (3, "soft"),
}
CONTEXT_VALUES = {
    "PGY_KEYWORD_CONTEXT_DECLARATION": 1 << 0,
    "PGY_KEYWORD_CONTEXT_STATEMENT": 1 << 1,
    "PGY_KEYWORD_CONTEXT_EXPRESSION": 1 << 2,
    "PGY_KEYWORD_CONTEXT_TYPE": 1 << 3,
    "PGY_KEYWORD_CONTEXT_CLAUSE": 1 << 4,
    "PGY_KEYWORD_CONTEXT_MODULE": 1 << 5,
    "PGY_KEYWORD_CONTEXT_INTENT_STEP": 1 << 6,
    "PGY_KEYWORD_CONTEXT_ZONE_BODY": 1 << 7,
    "PGY_KEYWORD_CONTEXT_NAME": 1 << 8,
    "PGY_KEYWORD_CONTEXT_PARAMETER": 1 << 9,
}
AXIS_VALUES = {
    "PGY_KEYWORD_AXIS_GENERAL": (0, "general"),
    "PGY_KEYWORD_AXIS_RESOURCE": (1, "resource"),
    "PGY_KEYWORD_AXIS_EXECUTION": (2, "execution"),
    "PGY_KEYWORD_AXIS_DOMAIN": (3, "domain"),
    "PGY_KEYWORD_AXIS_TYPE_CONTRACT": (4, "type-contract"),
}
SUPPORT_VALUES = {
    "PGY_KEYWORD_SUPPORT_NATIVE": 1 << 0,
    "PGY_KEYWORD_SUPPORT_SELF_HOST": 1 << 1,
}
TOOLING_VALUES = {
    "PGY_KEYWORD_TOOLING_COMPLETION": 1 << 0,
    "PGY_KEYWORD_TOOLING_HOVER": 1 << 1,
    "PGY_KEYWORD_TOOLING_HIGHLIGHT": 1 << 2,
}
HIGHLIGHT_SCOPE_META: dict[str, tuple[str, str] | None] = {
    "PGY_KEYWORD_HIGHLIGHT_NONE": None,
    "PGY_KEYWORD_HIGHLIGHT_CONTROL": (
        "keywords",
        "keyword.control.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_DECLARATION": (
        "keywords",
        "keyword.declaration.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_MODIFIER": (
        "keywords",
        "storage.modifier.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_CONSTANT": (
        "keywords",
        "constant.language.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_TYPE": (
        "keywords",
        "storage.type.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_DOMAIN": (
        "domain-keywords",
        "keyword.other.domain.pergyra",
    ),
    "PGY_KEYWORD_HIGHLIGHT_INTENT": (
        "intent-keywords",
        "keyword.other.intent.pergyra",
    ),
}
HIGHLIGHT_SCOPE_ORDER = tuple(HIGHLIGHT_SCOPE_META)


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
    highlight_scope: str


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


def _plain_symbol(field: str, label: str) -> str:
    if re.fullmatch(r"[A-Z][A-Z0-9_]*", field) is None:
        raise ValueError(f"{label} must be one stable symbol, got: {field!r}")
    return field


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


def _flag_value(field: str, values: dict[str, int]) -> int:
    if field == "0":
        return 0
    value = 0
    for term in _flag_terms(field, "registry flags", set(values)):
        value |= values[term]
    return value


def _word_variant(spelling: str) -> str:
    return "Word" + "_".join(
        part[:1].upper() + part[1:] for part in spelling.split("_")
    )


def load_rows(registry: Path) -> list[KeywordRow]:
    source = registry.read_text(encoding="utf-8")
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    source = re.sub(r"//.*?$", "", source, flags=re.MULTILINE)
    rows: list[KeywordRow] = []
    for row_number, body in enumerate(_macro_bodies(source), start=1):
        fields = _split_fields(body)
        if len(fields) != 9:
            raise ValueError(
                f"registry row {row_number} has {len(fields)} fields; expected 9"
            )
        spelling = _plain_c_string(fields[0], f"row {row_number} spelling")
        keyword_class = fields[1]
        token_type = fields[2]
        debug_identity = _plain_symbol(
            fields[3], f"row {row_number} debug identity"
        )
        if keyword_class not in CLASS_VALUES:
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
            fields[4],
            f"row {row_number} context mask",
            set(CONTEXT_VALUES),
        )
        if not contexts:
            raise ValueError(f"row {row_number} has no grammar context")
        if fields[5] not in AXIS_VALUES:
            raise ValueError(
                f"row {row_number} has unknown semantic axis: {fields[5]}"
            )
        support = _flag_terms(
            fields[6],
            f"row {row_number} implementation support",
            set(SUPPORT_VALUES),
        )
        if "PGY_KEYWORD_SUPPORT_NATIVE" not in support:
            raise ValueError(f"row {row_number} is not marked native supported")
        tooling = set()
        if fields[7] != "0":
            tooling = _flag_terms(
                fields[7],
                f"row {row_number} tooling flags",
                set(TOOLING_VALUES),
            )
        if fields[8] not in HIGHLIGHT_SCOPE_META:
            raise ValueError(
                f"row {row_number} has unknown highlight scope: {fields[8]}"
            )
        is_highlighted = "PGY_KEYWORD_TOOLING_HIGHLIGHT" in tooling
        has_scope = fields[8] != "PGY_KEYWORD_HIGHLIGHT_NONE"
        if is_highlighted != has_scope:
            raise ValueError(
                f"row {row_number} HIGHLIGHT/scope ownership disagrees"
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
                highlight_scope=fields[8],
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
    if len(identities) != len(set(identities)):
        raise ValueError("registry contains duplicate language-word identities")
    if any(row.debug_identity != row.spelling.upper() for row in rows):
        raise ValueError("registry debug identity does not match uppercase spelling")
    variants = [_word_variant(row.spelling) for row in rows]
    if len(variants) != len(set(variants)):
        raise ValueError("registry contains duplicate self-host word identities")

    class_counts = {
        keyword_class: sum(row.keyword_class == keyword_class for row in rows)
        for keyword_class in CLASS_VALUES
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
            token
            for token in set(reserved_tokens)
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
        raise ValueError("soft keyword set must be exactly current, full, none")
    return rows


def _append_index_string_function(
    lines: list[str],
    name: str,
    rows: list[KeywordRow],
    value,
) -> None:
    lines.extend(["", f"func {name}(index: Int) -> String {{"])
    for index, row in enumerate(rows):
        lines.append(f'    if index == {index} {{ return "{value(row)}"; }}')
    lines.extend(['    return "";', "}"])


def _append_index_int_function(
    lines: list[str],
    name: str,
    rows: list[KeywordRow],
    value,
) -> None:
    lines.extend(["", f"func {name}(index: Int) -> Int {{"])
    for index, row in enumerate(rows):
        lines.append(f"    if index == {index} {{ return {value(row)}; }}")
    lines.extend(["    return -1;", "}"])


PROJECTION_PART_FILES = (
    "language_word_identity_projection_owner.pgy",
    "language_word_index_projection_owner.pgy",
    "language_word_class_projection_owner.pgy",
    "language_word_axis_projection_owner.pgy",
    "language_word_semantic_projection_owner.pgy",
    "language_word_tooling_projection_owner.pgy",
    "language_keyword_compatibility_projection_owner.pgy",
)


def _projection_header(responsibility: str) -> list[str]:
    return [
        "// Generated by scripts/render_language_keyword_registry.py.",
        "// Source: src/lexer/language_keyword_registry.def. Do not edit by hand.",
        f"// Responsibility: {responsibility}.",
        "",
    ]


def _render_identity_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("stable language-word identity and spelling")
    lines.append("enum LanguageWordId {")
    for row in rows:
        lines.append(f"    {_word_variant(row.spelling)},")
    lines.extend(
        [
            "}",
            "",
            "func LanguageWordRegistryCount() -> Int {",
            f"    return {len(rows)};",
            "}",
            "",
            "func LanguageWordSpelling(id: LanguageWordId) -> String {",
        ]
    )
    for row in rows:
        lines.append(
            f'    if id == LanguageWordId.{_word_variant(row.spelling)} '
            f'{{ return "{row.spelling}"; }}'
        )
    lines.extend(
        [
            '    return "";',
            "}",
            "",
            "func LanguageWordLength(id: LanguageWordId) -> Int {",
            "    return StringLength(LanguageWordSpelling(id));",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def _render_index_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("ordered spelling and debug-identity index")
    _append_index_string_function(
        lines, "LanguageWordSpellingAt", rows, lambda row: row.spelling
    )
    _append_index_string_function(
        lines,
        "LanguageWordDebugIdentityAt",
        rows,
        lambda row: row.debug_identity,
    )
    lines.append("")
    return "\n".join(lines)


def _render_class_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("lexical class metadata")
    _append_index_int_function(
        lines,
        "LanguageWordClassAt",
        rows,
        lambda row: CLASS_VALUES[row.keyword_class][0],
    )
    _append_index_string_function(
        lines,
        "LanguageWordClassNameAt",
        rows,
        lambda row: CLASS_VALUES[row.keyword_class][1],
    )
    lines.append("")
    return "\n".join(lines)


def _render_axis_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("language-axis metadata")
    _append_index_int_function(
        lines,
        "LanguageWordAxisAt",
        rows,
        lambda row: AXIS_VALUES[row.axis][0],
    )
    _append_index_string_function(
        lines,
        "LanguageWordAxisNameAt",
        rows,
        lambda row: AXIS_VALUES[row.axis][1],
    )
    lines.append("")
    return "\n".join(lines)


def _render_semantic_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("grammar context and implementation support")
    _append_index_int_function(
        lines,
        "LanguageWordContextMaskAt",
        rows,
        lambda row: _flag_value(row.context_mask, CONTEXT_VALUES),
    )
    _append_index_int_function(
        lines,
        "LanguageWordImplementationSupportAt",
        rows,
        lambda row: _flag_value(row.implementation_support, SUPPORT_VALUES),
    )
    lines.append("")
    return "\n".join(lines)


def _render_tooling_projection(rows: list[KeywordRow]) -> str:
    lines = _projection_header("tooling flags, TextMate scope, and completion")
    _append_index_int_function(
        lines,
        "LanguageWordToolingFlagsAt",
        rows,
        lambda row: _flag_value(row.tooling_flags, TOOLING_VALUES),
    )
    _append_index_string_function(
        lines,
        "LanguageWordHighlightScopeAt",
        rows,
        lambda row: (
            ""
            if HIGHLIGHT_SCOPE_META[row.highlight_scope] is None
            else HIGHLIGHT_SCOPE_META[row.highlight_scope][1]
        ),
    )
    lines.extend(["", "func LanguageWordCompletionEnabledAt(index: Int) -> Bool {"])
    for index, row in enumerate(rows):
        enabled = "PGY_KEYWORD_TOOLING_COMPLETION" in row.tooling_flags
        lines.append(
            f"    if index == {index} {{ return {str(enabled).lower()}; }}"
        )
    lines.extend(
        [
            "    return false;",
            "}",
            "",
            "func LanguageWordCompletionOwnedAt(index: Int) -> Bool {",
            "    return LanguageWordCompletionEnabledAt(index);",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def _render_compatibility_projection(rows: list[KeywordRow]) -> str:
    legacy = reserved_rows(rows)
    lines = _projection_header("reserved lexer compatibility view")
    lines.extend(
        [
            "func LanguageKeywordRegistryCount() -> Int {",
            f"    return {len(legacy)};",
            "}",
        ]
    )
    _append_index_string_function(
        lines,
        "LanguageKeywordSpellingAt",
        legacy,
        lambda row: row.spelling,
    )
    _append_index_string_function(
        lines,
        "LanguageKeywordDebugIdentityAt",
        legacy,
        lambda row: row.debug_identity,
    )
    lines.extend(["", "func LanguageKeywordDebugIdentity(text: String) -> String {"])
    for row in legacy:
        lines.append(
            f'    if text == "{row.spelling}" '
            f'{{ return "{row.debug_identity}"; }}'
        )
    lines.extend(['    return "IDENTIFIER";', "}", ""])
    return "\n".join(lines)


def _render_aggregate_projection() -> str:
    lines = _projection_header(
        "compatibility import hub and cross-projection readiness"
    )
    for filename in PROJECTION_PART_FILES:
        lines.append(f'import "{filename}";')
    lines.extend(
        [
            "",
            "func LanguageWordRegistryProjectionReady() -> Bool {",
            "    let index: Int = 0;",
            "    while index < LanguageWordRegistryCount() {",
            "        if StringLength(LanguageWordSpellingAt(index)) == 0 ||",
            "            StringLength(LanguageWordDebugIdentityAt(index)) == 0 ||",
            "            LanguageWordClassAt(index) < 1 ||",
            "            StringLength(LanguageWordClassNameAt(index)) == 0 ||",
            "            LanguageWordContextMaskAt(index) <= 0 ||",
            "            LanguageWordAxisAt(index) < 0 ||",
            "            StringLength(LanguageWordAxisNameAt(index)) == 0 ||",
            "            LanguageWordImplementationSupportAt(index) <= 0 ||",
            "            LanguageWordToolingFlagsAt(index) < 0 {",
            "            return false;",
            "        }",
            "        index = index + 1;",
            "    }",
            f"    return LanguageWordRegistryCount() == {EXPECTED_ROW_COUNT};",
            "}",
            "",
            "func LanguageKeywordRegistryProjectionReady() -> Bool {",
            "    if !LanguageWordRegistryProjectionReady() { return false; }",
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
        ]
    )
    return "\n".join(lines)


def render_projection_files(
    rows: list[KeywordRow], aggregate_name: str
) -> dict[str, str]:
    return {
        aggregate_name: _render_aggregate_projection(),
        PROJECTION_PART_FILES[0]: _render_identity_projection(rows),
        PROJECTION_PART_FILES[1]: _render_index_projection(rows),
        PROJECTION_PART_FILES[2]: _render_class_projection(rows),
        PROJECTION_PART_FILES[3]: _render_axis_projection(rows),
        PROJECTION_PART_FILES[4]: _render_semantic_projection(rows),
        PROJECTION_PART_FILES[5]: _render_tooling_projection(rows),
        PROJECTION_PART_FILES[6]: _render_compatibility_projection(rows),
    }


def render(rows: list[KeywordRow]) -> str:
    return _render_aggregate_projection()


def _without_comments(source: str) -> str:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    return re.sub(r"//.*?$", "", source, flags=re.MULTILINE)


def _native_parser_use_counts(
    rows: list[KeywordRow], root: Path
) -> dict[str, int]:
    counts = {row.spelling: 0 for row in rows}
    helper_selector = re.compile(
        r"parser_(?:decl_(?:match|check)_contextual_keyword|"
        r"match_identifier_keyword(?:_on_line)?|intent_match_keyword|"
        r"check_contextual_keyword)\s*\(\s*parser\s*,\s*\"([a-z]+)\"",
        re.DOTALL,
    )
    direct_selector = re.compile(
        r"strcmp\s*\(\s*(?:parser->(?:current_token|previous_token)|mode)"
        r"\.text\s*,\s*\"([a-z]+)\"\s*\)"
    )
    sources = []
    for owner in sorted((root / "src/parser").glob("parser*.c")):
        sources.append(_without_comments(owner.read_text(encoding="utf-8")))
    joined = "\n".join(sources)
    for spelling in helper_selector.findall(joined):
        if spelling in counts:
            counts[spelling] += 1
    for spelling in direct_selector.findall(joined):
        if spelling in counts:
            counts[spelling] += 1
    for row in rows:
        # Contextual/soft consumers may select the stable registry identity
        # directly instead of comparing a spelling through the legacy helper.
        # Count that typed edge as native implementation evidence.
        language_word_id = "PGY_LANGUAGE_WORD_" + row.debug_identity
        counts[row.spelling] += len(
            re.findall(rf"\b{re.escape(language_word_id)}\b", joined)
        )
        if row.keyword_class == RESERVED:
            counts[row.spelling] += len(
                re.findall(rf"\b{re.escape(row.token_type)}\b", joined)
            )
    return counts


def _self_host_parser_use_counts(
    rows: list[KeywordRow], root: Path
) -> dict[str, int]:
    counts = {row.spelling: 0 for row in rows}
    sources = []
    for owner in sorted((root / "src/self_hosted/parser").glob("*.pgy")):
        sources.append(_without_comments(owner.read_text(encoding="utf-8")))
    joined = "\n".join(sources)
    for row in rows:
        selector = "LanguageWordId." + _word_variant(row.spelling)
        counts[row.spelling] = len(
            re.findall(
                rf"\bMatchLanguageWord\s*\([^)]*\b{re.escape(selector)}\b[^)]*\)",
                joined,
            )
        )
    return counts


def _self_host_direct_selector_counts(
    rows: list[KeywordRow], root: Path
) -> dict[str, int]:
    counts = {row.spelling: 0 for row in rows}
    selector_variables = {
        "stmt_owner.pgy": ("head",),
        "expr_primary_owner.pgy": ("base",),
        "type_name_owner.pgy": ("base",),
        "decl_intent_owner.pgy": ("clause",),
    }
    parser_root = root / "src/self_hosted/parser"
    for owner_name, variables in selector_variables.items():
        source = _without_comments(
            (parser_root / owner_name).read_text(encoding="utf-8")
        )
        variable_pattern = "|".join(re.escape(value) for value in variables)
        right_literal = re.compile(
            rf"\b(?:{variable_pattern})\s*(?:==|!=)\s*"
            r'"([a-z][a-z0-9_]*)"'
        )
        left_literal = re.compile(
            r'"([a-z][a-z0-9_]*)"\s*(?:==|!=)\s*'
            rf"(?:{variable_pattern})\b"
        )
        for spelling in right_literal.findall(source) + left_literal.findall(source):
            if spelling in counts:
                counts[spelling] += 1
    raw_match = re.compile(
        r'MatchKeyword\s*\([^;\n]*,\s*"([a-z][a-z0-9_]*)"\s*\)'
    )
    for owner in sorted(parser_root.glob("*.pgy")):
        source = _without_comments(owner.read_text(encoding="utf-8"))
        for spelling in raw_match.findall(source):
            if spelling in counts:
                counts[spelling] += 1
    return counts


def _fixture_evidence_counts(
    rows: list[KeywordRow], root: Path
) -> dict[str, int]:
    counts = {row.spelling: 0 for row in rows}
    fixtures = []
    for source_root in (root / "src", root / "tests", root / "examples"):
        if not source_root.is_dir():
            continue
        fixtures.extend(
            path
            for path in source_root.rglob("*.pgy")
            if any("fixture" in part.lower() for part in path.parts)
        )
    spellings = set(counts)
    for path in sorted(fixtures):
        source = _without_comments(path.read_text(encoding="utf-8"))
        present = set(re.findall(r"\b[a-z][a-z0-9_]*\b", source)) & spellings
        for spelling in present:
            counts[spelling] += 1
    return counts


def _tooling_exposure(row: KeywordRow) -> str:
    exposures = []
    if "PGY_KEYWORD_TOOLING_COMPLETION" in row.tooling_flags:
        exposures.append("completion")
    if "PGY_KEYWORD_TOOLING_HOVER" in row.tooling_flags:
        exposures.append("hover")
    scope = HIGHLIGHT_SCOPE_META[row.highlight_scope]
    if scope is not None:
        exposures.append("TextMate:" + scope[1])
    return ", ".join(exposures) if exposures else "none"


def render_implementation_inventory(rows: list[KeywordRow], root: Path) -> str:
    native = _native_parser_use_counts(rows, root)
    self_host_typed = _self_host_parser_use_counts(rows, root)
    self_host_direct = _self_host_direct_selector_counts(rows, root)
    fixtures = _fixture_evidence_counts(rows, root)

    def classification(row: KeywordRow) -> str:
        native_used = native[row.spelling] > 0
        typed_used = self_host_typed[row.spelling] > 0
        direct_used = self_host_direct[row.spelling] > 0
        if native_used and typed_used:
            return "native+selfhost-typed"
        if native_used and direct_used:
            return "native+selfhost-direct-only"
        if native_used:
            return "native-only"
        if typed_used:
            return "selfhost-typed-only"
        if direct_used:
            return "selfhost-direct-only"
        return "no-parser-selector"

    classification_counts = {
        label: sum(classification(row) == label for row in rows)
        for label in (
            "native+selfhost-typed",
            "native+selfhost-direct-only",
            "native-only",
            "selfhost-typed-only",
            "selfhost-direct-only",
            "no-parser-selector",
        )
    }
    direct_occurrences = sum(self_host_direct.values())
    direct_word_count = sum(value > 0 for value in self_host_direct.values())
    typed_occurrences = sum(self_host_typed.values())
    typed_word_count = sum(value > 0 for value in self_host_typed.values())
    action = next(row for row in rows if row.spelling == "action")
    action_scope = HIGHLIGHT_SCOPE_META[action.highlight_scope]
    action_scope_name = "none" if action_scope is None else action_scope[1]
    lines = [
        "# Generated Language-Word Implementation Inventory",
        "",
        "> Generated by `scripts/render_language_keyword_registry.py` from the",
        "> canonical registry and current source tree. Do not edit by hand.",
        "",
        "This is an occurrence inventory, not an implementation-complete claim.",
        "Declared support remains a registry declaration. Parser selector counts",
        "and fixture-file counts are separate evidence, while TextMate scope is",
        "only the current editor projection and is never parser implementation",
        "evidence.",
        "",
        "## Closure status",
        "",
        "- `BRIDGE`: stable IDs and projections exist, but row implementation",
        "  remains split across native, typed self-host, and direct selectors.",
        f"- Typed-selector evidence: {typed_occurrences} calls across "
        f"{typed_word_count} language words.",
        f"- Direct-selector debt: {direct_occurrences} occurrences across "
        f"{direct_word_count} language words.",
        "- No row is promoted to `CLOSED` by this inventory.",
        "",
        "## Classification summary",
        "",
        "| classification | rows |",
        "|---|---:|",
    ]
    for label, count in classification_counts.items():
        lines.append(f"| {label} | {count} |")
    lines.extend(
        [
            "",
            "## Action audit",
            "",
            f"- native parser selectors: {native[action.spelling]}",
            f"- self-host typed selectors: {self_host_typed[action.spelling]}",
            f"- self-host direct string selectors: {self_host_direct[action.spelling]}",
            f"- fixture files containing `action`: {fixtures[action.spelling]}",
            f"- tooling: {_tooling_exposure(action)}",
            f"- TextMate scope (projection only): `{action_scope_name}`",
            "",
            "## Row inventory",
            "",
            "| word | class | declared support | native parser uses | "
            "self-host typed uses | self-host direct selectors | fixture files | "
            "tooling exposure | evidence class |",
            "|---|---|---|---:|---:|---:|---:|---|---|",
        ]
    )
    for row in rows:
        support = row.implementation_support.replace(
            "PGY_KEYWORD_SUPPORT_", ""
        ).replace(" | ", "+").lower()
        lines.append(
            f"| `{row.spelling}` | {CLASS_VALUES[row.keyword_class][1]} | "
            f"{support} | {native[row.spelling]} | "
            f"{self_host_typed[row.spelling]} | "
            f"{self_host_direct[row.spelling]} | {fixtures[row.spelling]} | "
            f"{_tooling_exposure(row)} | "
            f"{classification(row)} |"
        )
    lines.extend(
        [
            "",
            "## Counting boundaries",
            "",
            "- Native counts include contextual string selectors and reserved token",
            "  identities in `src/parser/parser*.c` after comments are removed.",
            "- Self-host typed counts include `MatchLanguageWord` calls with",
            "  `LanguageWordId.Word*` in `src/self_hosted/parser/*.pgy` after",
            "  comments are removed; derived cursor-length uses are not counted",
            "  again as selectors.",
            "- Direct-selector counts separately include registry spellings",
            "  compared with identifier-read `head`, `base`, or `clause` values",
            "  in the named statement/expression/type/intent owners, plus literal",
            "  `MatchKeyword` calls. They are migration debt, not typed closure.",
            "- Fixture evidence is the number of `.pgy` files below a path segment",
            "  containing `fixture` whose non-comment source contains the word.",
            "- A zero count is retained as evidence of a gap; it is not repaired or",
            "  hidden by declared support or editor highlighting.",
            "",
        ]
    )
    return "\n".join(lines)


def textmate_keyword_owners(
    rows: list[KeywordRow],
) -> dict[str, dict[str, list[dict[str, str]]]]:
    owners = {
        "keywords": {"patterns": []},
        "domain-keywords": {"patterns": []},
        "intent-keywords": {"patterns": []},
    }
    for scope in HIGHLIGHT_SCOPE_ORDER:
        meta = HIGHLIGHT_SCOPE_META[scope]
        if meta is None:
            continue
        owner, textmate_scope = meta
        spellings = [
            row.spelling for row in rows if row.highlight_scope == scope
        ]
        if not spellings:
            raise ValueError(f"highlight scope {scope} owns no rows")
        owners[owner]["patterns"].append(
            {
                "name": textmate_scope,
                "match": r"\b(?:" + "|".join(spellings) + r")\b",
            }
        )
    return owners


def check_textmate_grammar(rows: list[KeywordRow], grammar_path: Path) -> None:
    if not grammar_path.is_file():
        raise SystemExit(f"missing TextMate grammar: {grammar_path}")
    grammar = json.loads(grammar_path.read_text(encoding="utf-8"))
    repository = grammar.get("repository")
    if not isinstance(repository, dict):
        raise SystemExit("TextMate grammar repository is missing")
    expected = textmate_keyword_owners(rows)
    actual = {owner: repository.get(owner) for owner in expected}
    if actual != expected:
        raise SystemExit(
            "TextMate keyword owners drifted from registry spelling/scope facts; "
            "regenerate with --write and --textmate-grammar"
        )


def write_textmate_grammar(rows: list[KeywordRow], grammar_path: Path) -> None:
    if not grammar_path.is_file():
        raise SystemExit(f"missing TextMate grammar: {grammar_path}")
    grammar = json.loads(grammar_path.read_text(encoding="utf-8"))
    repository = grammar.get("repository")
    if not isinstance(repository, dict):
        raise SystemExit("TextMate grammar repository is missing")
    for owner, value in textmate_keyword_owners(rows).items():
        repository[owner] = value
    grammar_path.write_text(
        json.dumps(grammar, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("registry", type=Path)
    parser.add_argument("projection", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    parser.add_argument("--textmate-grammar", type=Path)
    parser.add_argument("--implementation-inventory", type=Path)
    args = parser.parse_args()

    rows = load_rows(args.registry)
    expected_files = render_projection_files(rows, args.projection.name)
    if args.check:
        for filename, expected in expected_files.items():
            output = args.projection.parent / filename
            if not output.is_file():
                raise SystemExit(f"missing generated projection: {output}")
            actual = output.read_text(encoding="utf-8")
            if actual != expected:
                raise SystemExit(
                    "self-host language-word projection drifted: "
                    f"{output.as_posix()}; regenerate with "
                    f"{Path(__file__).as_posix()} {args.registry.as_posix()} "
                    f"{args.projection.as_posix()} --write"
                )
        if args.textmate_grammar is not None:
            check_textmate_grammar(rows, args.textmate_grammar)
        if args.implementation_inventory is not None:
            root = args.registry.resolve().parents[2]
            inventory = render_implementation_inventory(rows, root)
            if not args.implementation_inventory.is_file():
                raise SystemExit(
                    "missing generated implementation inventory: "
                    f"{args.implementation_inventory}"
                )
            actual_inventory = args.implementation_inventory.read_text(
                encoding="utf-8"
            )
            if actual_inventory != inventory:
                raise SystemExit(
                    "language-word implementation inventory drifted; "
                    "regenerate with --write"
                )
        return 0

    args.projection.parent.mkdir(parents=True, exist_ok=True)
    for filename, expected in expected_files.items():
        (args.projection.parent / filename).write_text(
            expected, encoding="utf-8", newline="\n"
        )
    if args.textmate_grammar is not None:
        write_textmate_grammar(rows, args.textmate_grammar)
    if args.implementation_inventory is not None:
        root = args.registry.resolve().parents[2]
        args.implementation_inventory.parent.mkdir(parents=True, exist_ok=True)
        args.implementation_inventory.write_text(
            render_implementation_inventory(rows, root),
            encoding="utf-8",
            newline="\n",
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
