#!/usr/bin/env python3
"""Validate and project the callable-contract vocabulary declaration."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
import re


CAPABILITY_MACRO = "PGY_CALLABLE_CONTRACT_CAPABILITY"
EFFECT_MACRO = "PGY_CALLABLE_CONTRACT_EFFECT"
EXPECTED_AXIS_COUNT = 9
ZERO_DISALLOWED = "PGY_CALLABLE_CONTRACT_ZERO_DISALLOWED"
ZERO_EXCLUSIVE = "PGY_CALLABLE_CONTRACT_ZERO_EXCLUSIVE"


@dataclass(frozen=True)
class ContractWord:
    axis: str
    identity: str
    stable_id: int
    spelling: str
    mask_symbol: str
    canonical_rank: int
    zero_policy: str
    external_name: str

    @property
    def axis_value(self) -> int:
        return 1 if self.axis == "capability" else 2

    @property
    def zero_policy_value(self) -> int:
        return 1 if self.zero_policy == ZERO_EXCLUSIVE else 0


def _strip_comments(source: str) -> str:
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


def _plain_symbol(field: str, label: str) -> str:
    if re.fullmatch(r"[A-Z][A-Z0-9_]*", field) is None:
        raise ValueError(f"{label} must be one stable symbol, got {field!r}")
    return field


def _plain_int(field: str, label: str) -> int:
    if re.fullmatch(r"0|[1-9][0-9]*", field) is None:
        raise ValueError(f"{label} must be one non-negative integer, got {field!r}")
    return int(field)


def _plain_string(field: str, label: str, *, allow_empty: bool) -> str:
    match = re.fullmatch(r'"([A-Za-z0-9_]*)"', field)
    if match is None or (not allow_empty and match.group(1) == ""):
        qualifier = "possibly empty" if allow_empty else "non-empty"
        raise ValueError(f"{label} must be one {qualifier} C string, got {field!r}")
    return match.group(1)


def _parse_rows(source: str, macro: str, axis: str) -> list[ContractWord]:
    rows: list[ContractWord] = []
    for row_number, body in enumerate(_macro_bodies(source, macro), start=1):
        fields = _split_fields(body)
        if len(fields) != 7:
            raise ValueError(
                f"{axis} row {row_number} has {len(fields)} fields; expected 7"
            )
        rows.append(
            ContractWord(
                axis=axis,
                identity=_plain_symbol(fields[0], f"{axis} identity"),
                stable_id=_plain_int(fields[1], f"{axis} stable id"),
                spelling=_plain_string(
                    fields[2], f"{axis} spelling", allow_empty=False
                ),
                mask_symbol=_plain_symbol(fields[3], f"{axis} mask symbol"),
                canonical_rank=_plain_int(fields[4], f"{axis} canonical rank"),
                zero_policy=_plain_symbol(fields[5], f"{axis} zero policy"),
                external_name=_plain_string(
                    fields[6], f"{axis} external name", allow_empty=True
                ),
            )
        )
    return rows


def load_rows(registry: Path) -> list[ContractWord]:
    source = _strip_comments(registry.read_text(encoding="utf-8"))
    if source.rfind(CAPABILITY_MACRO) > source.find(EFFECT_MACRO):
        raise ValueError("capability rows must precede effect rows")
    capabilities = _parse_rows(source, CAPABILITY_MACRO, "capability")
    effects = _parse_rows(source, EFFECT_MACRO, "effect")
    if len(capabilities) != EXPECTED_AXIS_COUNT:
        raise ValueError(
            f"registry has {len(capabilities)} capabilities; "
            f"expected {EXPECTED_AXIS_COUNT}"
        )
    if len(effects) != EXPECTED_AXIS_COUNT:
        raise ValueError(
            f"registry has {len(effects)} effects; expected {EXPECTED_AXIS_COUNT}"
        )

    rows = capabilities + effects
    expected_ids = list(range(len(rows)))
    if [row.stable_id for row in rows] != expected_ids:
        raise ValueError("stable ids must be contiguous registry order 0..17")
    if len({row.identity for row in rows}) != len(rows):
        raise ValueError("registry contains duplicate stable identities")
    if len({row.spelling for row in rows}) != len(rows):
        raise ValueError("registry contains duplicate spellings")

    for axis_rows in (capabilities, effects):
        if sorted(row.canonical_rank for row in axis_rows) != list(
            range(EXPECTED_AXIS_COUNT)
        ):
            raise ValueError(
                f"{axis_rows[0].axis} ranks must be unique and contiguous 0..8"
            )
        if len({row.mask_symbol for row in axis_rows}) != len(axis_rows):
            raise ValueError(f"{axis_rows[0].axis} mask symbols must be unique")

    exclusive = [row for row in rows if row.zero_policy == ZERO_EXCLUSIVE]
    if len(exclusive) != 1:
        raise ValueError("registry must have exactly one zero-exclusive row")
    local = exclusive[0]
    if (
        local.axis != "effect"
        or local.spelling != "local"
        or local.mask_symbol != "EFFECT_NONE"
    ):
        raise ValueError("the sole zero-exclusive row must be effect local/EFFECT_NONE")

    for row in rows:
        if row.zero_policy not in {ZERO_DISALLOWED, ZERO_EXCLUSIVE}:
            raise ValueError(f"unknown zero policy on {row.spelling}")
        if row.zero_policy == ZERO_DISALLOWED and row.mask_symbol.endswith("_NONE"):
            raise ValueError(f"non-exclusive row {row.spelling} uses a zero mask")
        if row.axis == "capability":
            if not row.mask_symbol.startswith("PGY_CAP_"):
                raise ValueError(f"capability {row.spelling} does not reference PGY_CAP_*")
            if row.external_name != row.spelling.upper():
                raise ValueError(
                    f"capability {row.spelling} external name must be uppercase spelling"
                )
        else:
            if not row.mask_symbol.startswith("EFFECT_"):
                raise ValueError(f"effect {row.spelling} does not reference EFFECT_*")
            if row.external_name != "":
                raise ValueError(f"effect {row.spelling} must not own an external name")
    return rows


def _append_index_string(
    lines: list[str], name: str, rows: list[ContractWord], field: str
) -> None:
    lines.extend(["", f"func {name}(index: Int) -> String {{"])
    for index, row in enumerate(rows):
        value = getattr(row, field)
        lines.append(f'    if index == {index} {{ return "{value}"; }}')
    lines.extend(['    return "";', "}"])


def _append_index_int(
    lines: list[str], name: str, rows: list[ContractWord], field: str
) -> None:
    lines.extend(["", f"func {name}(index: Int) -> Int {{"])
    for index, row in enumerate(rows):
        value = getattr(row, field)
        lines.append(f"    if index == {index} {{ return {value}; }}")
    lines.extend(["    return -1;", "}"])


def render_projection(rows: list[ContractWord]) -> str:
    lines = [
        "// Generated by scripts/render_callable_contract_vocabulary.py.",
        "// Source: src/semantic/callable_contract_vocabulary.def. Do not edit by hand.",
        "// Responsibility: stable callable capability/effect vocabulary projection.",
        "",
        "func CallableContractVocabularyCount() -> Int {",
        f"    return {len(rows)};",
        "}",
        "",
        "func CallableContractVocabularyCapabilityAxis() -> Int { return 1; }",
        "func CallableContractVocabularyEffectAxis() -> Int { return 2; }",
        "func CallableContractVocabularyZeroDisallowed() -> Int { return 0; }",
        "func CallableContractVocabularyZeroExclusive() -> Int { return 1; }",
    ]
    _append_index_int(lines, "CallableContractVocabularyStableIdAt", rows, "stable_id")
    _append_index_int(
        lines,
        "CallableContractVocabularyAxisAt",
        rows,
        "axis_value",
    )
    _append_index_string(lines, "CallableContractVocabularyIdentityAt", rows, "identity")
    _append_index_string(lines, "CallableContractVocabularySpellingAt", rows, "spelling")
    _append_index_string(
        lines, "CallableContractVocabularyMaskSymbolAt", rows, "mask_symbol"
    )
    _append_index_int(
        lines, "CallableContractVocabularyCanonicalRankAt", rows, "canonical_rank"
    )
    _append_index_int(
        lines, "CallableContractVocabularyZeroPolicyAt", rows, "zero_policy_value"
    )
    _append_index_string(
        lines, "CallableContractVocabularyExternalNameAt", rows, "external_name"
    )

    lines.extend(
        [
            "",
            "func CallableContractVocabularyFindIndex(axis: Int, spelling: String) -> Int {",
        ]
    )
    for index, row in enumerate(rows):
        axis_value = 1 if row.axis == "capability" else 2
        lines.append(
            f'    if axis == {axis_value} && spelling == "{row.spelling}" '
            f"{{ return {index}; }}"
        )
    lines.extend(
        [
            "    return -1;",
            "}",
            "",
            "func CallableContractVocabularyIndexAtRank(axis: Int, rank: Int) -> Int {",
        ]
    )
    for index, row in enumerate(rows):
        axis_value = 1 if row.axis == "capability" else 2
        lines.append(
            f"    if axis == {axis_value} && rank == {row.canonical_rank} "
            f"{{ return {index}; }}"
        )
    lines.extend(
        [
            "    return -1;",
            "}",
            "",
            "func CallableContractVocabularyContains(axis: Int, spelling: String) -> Bool {",
            "    return CallableContractVocabularyFindIndex(axis, spelling) >= 0;",
            "}",
            "",
            "func CallableContractVocabularyProjectionReady() -> Bool {",
            "    if CallableContractVocabularyCount() != 18 { return false; }",
            "    let index: Int = 0;",
            "    let zero_exclusive_count: Int = 0;",
            "    while index < CallableContractVocabularyCount() {",
            "        let axis: Int = CallableContractVocabularyAxisAt(index);",
            "        let rank: Int = CallableContractVocabularyCanonicalRankAt(index);",
            "        let zero_policy: Int = CallableContractVocabularyZeroPolicyAt(index);",
            "        if CallableContractVocabularyStableIdAt(index) != index ||",
            "            StringLength(CallableContractVocabularyIdentityAt(index)) == 0 ||",
            "            StringLength(CallableContractVocabularySpellingAt(index)) == 0 ||",
            "            StringLength(CallableContractVocabularyMaskSymbolAt(index)) == 0 ||",
            "            (axis != CallableContractVocabularyCapabilityAxis() &&",
            "             axis != CallableContractVocabularyEffectAxis()) ||",
            "            CallableContractVocabularyIndexAtRank(axis, rank) != index {",
            "            return false;",
            "        }",
            "        if zero_policy == CallableContractVocabularyZeroExclusive() {",
            "            zero_exclusive_count = zero_exclusive_count + 1;",
            "            if axis != CallableContractVocabularyEffectAxis() ||",
            '                CallableContractVocabularySpellingAt(index) != "local" ||',
            '                CallableContractVocabularyMaskSymbolAt(index) != "EFFECT_NONE" {',
            "                return false;",
            "            }",
            "        } else if zero_policy != CallableContractVocabularyZeroDisallowed() {",
            "            return false;",
            "        }",
            "        index = index + 1;",
            "    }",
            "    return zero_exclusive_count == 1;",
            "}",
            "",
        ]
    )
    return "\n".join(lines)


def render_runtime_capability_projection(rows: list[ContractWord]) -> str:
    lines = [
        "/* Generated by scripts/render_callable_contract_vocabulary.py.",
        " * Source: src/semantic/callable_contract_vocabulary.def. Do not edit.",
        " * The including runtime owner defines PGY_CALLABLE_CONTRACT_RUNTIME_CAP.",
        " */",
    ]
    for row in rows:
        if row.axis == "capability":
            lines.append(
                f'PGY_CALLABLE_CONTRACT_RUNTIME_CAP("{row.spelling}", '
                f"{row.mask_symbol})"
            )
    lines.append("")
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("registry", type=Path)
    parser.add_argument("projection", type=Path)
    parser.add_argument("runtime_projection", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    rows = load_rows(args.registry)
    expected = render_projection(rows)
    expected_runtime = render_runtime_capability_projection(rows)
    if args.check:
        if not args.projection.is_file():
            raise SystemExit(f"missing generated projection: {args.projection}")
        if args.projection.read_text(encoding="utf-8") != expected:
            raise SystemExit(
                "self-host callable-contract projection drifted; regenerate with "
                f"{Path(__file__).as_posix()} {args.registry.as_posix()} "
                f"{args.projection.as_posix()} "
                f"{args.runtime_projection.as_posix()} --write"
            )
        if not args.runtime_projection.is_file():
            raise SystemExit(
                f"missing generated runtime projection: {args.runtime_projection}"
            )
        if args.runtime_projection.read_text(encoding="utf-8") != expected_runtime:
            raise SystemExit(
                "runtime callable-capability projection drifted; regenerate with "
                f"{Path(__file__).as_posix()} {args.registry.as_posix()} "
                f"{args.projection.as_posix()} "
                f"{args.runtime_projection.as_posix()} --write"
            )
        return 0

    args.projection.parent.mkdir(parents=True, exist_ok=True)
    args.projection.write_text(expected, encoding="utf-8", newline="\n")
    args.runtime_projection.parent.mkdir(parents=True, exist_ok=True)
    args.runtime_projection.write_text(
        expected_runtime, encoding="utf-8", newline="\n"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
