#!/usr/bin/env python3
"""Validate the canonical SoT owner registry and its live authority edges."""

from __future__ import annotations

import argparse
import csv
import pathlib
import re
import subprocess
import sys
from collections import Counter
from dataclasses import dataclass


OWNER_BEGIN = "<!-- BEGIN sot-owner-spine-registry -->"
OWNER_END = "<!-- END sot-owner-spine-registry -->"
DERIVED_BEGIN = "<!-- BEGIN sot-derived-fact-registry -->"
DERIVED_END = "<!-- END sot-derived-fact-registry -->"
OWNER_FIELDS = (
    "owner_id",
    "fact_class",
    "stable_handle",
    "coq_fact",
    "coq_owner",
    "authority_path",
    "producer_term",
    "last_consumers",
    "forbidden_fallbacks",
    "enforcement_gate",
    "status",
    "open_reason",
)
DERIVED_FIELDS = ("path", "primary_term", "owner_id", "relation")
STATUSES = {"ACTIVE", "BRIDGE", "CLOSED"}
DERIVED_RELATIONS = {"projection", "cache", "bridge", "local_view"}
SOURCE_SUFFIXES = {".c", ".h", ".pgy"}


class GateFailure(RuntimeError):
    pass


@dataclass(frozen=True)
class OwnerRow:
    values: dict[str, str]

    def __getitem__(self, key: str) -> str:
        return self.values[key]


@dataclass(frozen=True)
class DerivedRow:
    values: dict[str, str]

    def __getitem__(self, key: str) -> str:
        return self.values[key]


def fail(message: str) -> None:
    raise GateFailure(message)


def block_rows(text: str, begin: str, end: str, fields: tuple[str, ...]):
    if begin not in text or end not in text:
        fail(f"registry markers are missing: {begin} / {end}")
    body = text.split(begin, 1)[1].split(end, 1)[0]
    rows: list[dict[str, str]] = []
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("```"):
            continue
        values = [part.strip() for part in line.split("|")]
        if len(values) != len(fields):
            fail(f"registry row must have {len(fields)} fields: {line}")
        rows.append(dict(zip(fields, values)))
    if not rows:
        fail(f"registry block has no rows: {begin}")
    return rows


def csv_items(value: str, field: str) -> list[str]:
    try:
        items = next(csv.reader([value], skipinitialspace=True))
    except csv.Error as exc:
        fail(f"{field} is not valid CSV: {exc}")
    cleaned = [item.strip() for item in items if item.strip()]
    if not cleaned:
        fail(f"{field} must not be empty")
    return cleaned


def require_path(root: pathlib.Path, rel: str, field: str) -> pathlib.Path:
    path = root / pathlib.PurePosixPath(rel)
    if not path.is_file():
        fail(f"{field} path does not exist: {rel}")
    return path


def evidence_ref(root: pathlib.Path, value: str, field: str):
    if "#" not in value:
        fail(f"{field} must use path#required-text: {value}")
    rel, needle = value.split("#", 1)
    if not rel or not needle:
        fail(f"{field} has an empty path or required text: {value}")
    path = require_path(root, rel, field)
    if needle not in path.read_text(encoding="utf-8"):
        fail(f"{field} required text is absent from {rel}: {needle}")
    return path, needle


def coq_authority_map(text: str) -> dict[str, str]:
    pairs = re.findall(
        r"^\s*\|\s*(SF[A-Za-z0-9_]+)\s*=>\s*(SO[A-Za-z0-9_]+)\s*$",
        text,
        flags=re.MULTILINE,
    )
    result: dict[str, str] = {}
    for fact, owner in pairs:
        if fact in result:
            fail(f"Coq authority mapping duplicates {fact}")
        result[fact] = owner
    if not result:
        fail("Coq spine authority mapping is empty")
    return result


def source_index(
    root: pathlib.Path, producer_terms: list[str]
) -> dict[pathlib.Path, str]:
    indexed: dict[pathlib.Path, str] = {}
    paths: list[pathlib.Path] = []
    try:
        result = subprocess.run(
            ["git", "grep", "-l", "-F", "-f", "-", "--", "src"],
            cwd=root,
            input="\n".join(producer_terms) + "\n",
            text=True,
            capture_output=True,
            check=False,
        )
        if result.returncode not in (0, 1):
            fail(f"git grep source index failed: {result.stderr.strip()}")
        paths = [root / line for line in result.stdout.splitlines() if line]
    except FileNotFoundError:
        paths = [path for path in (root / "src").rglob("*") if path.is_file()]

    for path in paths:
        if path.suffix not in SOURCE_SUFFIXES:
            continue
        indexed[path] = path.read_text(encoding="utf-8", errors="replace")
    return indexed


def producer_definition_paths(
    indexed: dict[pathlib.Path, str], term: str
) -> set[pathlib.Path]:
    escaped = re.escape(term)
    pgy_pattern = re.compile(
        rf"^\s*(?:func|intent|struct|enum|world|zone|ability|role)\s+{escaped}\b",
        flags=re.MULTILINE,
    )
    macro_pattern = re.compile(
        rf"^\s*#\s*define\s+{escaped}\b", flags=re.MULTILINE
    )
    c_function_pattern = re.compile(
        rf"^[A-Za-z_][^;{{}}]{{0,800}}\b{escaped}\s*\([^;{{}}]{{0,800}}\)\s*\{{",
        flags=re.MULTILINE,
    )
    definitions: set[pathlib.Path] = set()
    for path, text in indexed.items():
        if path.suffix == ".pgy":
            matched = pgy_pattern.search(text)
        else:
            matched = macro_pattern.search(text) or c_function_pattern.search(text)
        if matched:
            definitions.add(path.resolve())
    return definitions


def source_references(
    indexed: dict[pathlib.Path, str], term: str
) -> list[pathlib.Path]:
    return [path for path, text in indexed.items() if term in text]


def validate_owner_rows(
    root: pathlib.Path,
    registry_text: str,
    rows: list[OwnerRow],
    indexed: dict[pathlib.Path, str],
) -> Counter:
    owner_ids = [row["owner_id"] for row in rows]
    coq_facts = [row["coq_fact"] for row in rows]
    if len(owner_ids) != len(set(owner_ids)):
        fail("registry contains duplicate owner ids")
    if len(coq_facts) != len(set(coq_facts)):
        fail("registry contains duplicate Coq fact authorities")
    coq_path = root / "docs/semantics/proofs/SoTAuthority.v"
    coq_map = coq_authority_map(coq_path.read_text(encoding="utf-8"))
    registry_map = {row["coq_fact"]: row["coq_owner"] for row in rows}
    if registry_map != coq_map:
        missing = sorted(set(registry_map) - set(coq_map))
        extra = sorted(set(coq_map) - set(registry_map))
        wrong = sorted(
            fact
            for fact in set(registry_map) & set(coq_map)
            if registry_map[fact] != coq_map[fact]
        )
        fail(
            "registry/Coq authority projection drifted "
            f"(missing={missing}, extra={extra}, wrong={wrong})"
        )

    for row in rows:
        owner_id = row["owner_id"]
        if row["status"] not in STATUSES:
            fail(f"{owner_id}: invalid status {row['status']}")
        if not row["stable_handle"] or not row["fact_class"]:
            fail(f"{owner_id}: stable handle and fact class are required")
        authority = require_path(
            root, row["authority_path"], f"{owner_id}: authority_path"
        )
        authority_text = authority.read_text(encoding="utf-8", errors="replace")
        if row["producer_term"] not in authority_text:
            fail(f"{owner_id}: producer term missing from authority path")

        definitions = producer_definition_paths(indexed, row["producer_term"])
        outside = sorted(
            path.relative_to(root).as_posix()
            for path in definitions
            if path != authority.resolve()
        )
        if outside:
            fail(
                f"{owner_id}: producer term is defined outside its authority: "
                + ", ".join(outside)
            )

        consumers = csv_items(
            row["last_consumers"], f"{owner_id}: last_consumers"
        )
        for consumer in consumers:
            require_path(root, consumer, f"{owner_id}: last_consumer")
        gate_path, _ = evidence_ref(
            root, row["enforcement_gate"], f"{owner_id}: enforcement_gate"
        )
        forbidden = csv_items(
            row["forbidden_fallbacks"], f"{owner_id}: forbidden_fallbacks"
        )

        if row["status"] == "CLOSED":
            if row["open_reason"] != "none":
                fail(f"{owner_id}: CLOSED row must have open_reason=none")
            gate_text = gate_path.read_text(encoding="utf-8", errors="replace")
            for token in forbidden:
                if token not in gate_text:
                    fail(f"{owner_id}: CLOSED gate does not name fallback {token}")

            for consumer in source_references(indexed, row["producer_term"]):
                if consumer.resolve() == authority.resolve():
                    continue
                consumer_text = indexed[consumer]
                for token in forbidden:
                    if token in consumer_text:
                        rel = consumer.relative_to(root).as_posix()
                        fail(
                            f"{owner_id}: canonical-fact consumer {rel} "
                            f"reopened fallback {token}"
                        )
        elif not row["open_reason"] or row["open_reason"] == "none":
            fail(f"{owner_id}: open row must name its remaining reason")

    counts = Counter(row["status"] for row in rows)
    summary = (
        f"CLOSED={counts['CLOSED']} BRIDGE={counts['BRIDGE']} "
        f"ACTIVE={counts['ACTIVE']}"
    )
    if summary not in registry_text:
        fail(f"registry status summary drifted; expected literal: {summary}")
    return counts


def validate_derived_rows(
    root: pathlib.Path, owners: list[OwnerRow], rows: list[DerivedRow]
) -> None:
    owner_ids = {row["owner_id"] for row in owners}
    authority_paths = {
        pathlib.PurePosixPath(row["authority_path"]).as_posix() for row in owners
    }
    derived_paths: set[str] = set()
    primary_terms: set[str] = set()
    for row in rows:
        rel = pathlib.PurePosixPath(row["path"]).as_posix()
        if rel in derived_paths:
            fail(f"derived fact registry duplicates path: {rel}")
        if row["primary_term"] in primary_terms:
            fail(
                "derived fact registry duplicates primary term: "
                + row["primary_term"]
            )
        derived_paths.add(rel)
        primary_terms.add(row["primary_term"])
        if rel in authority_paths:
            fail(f"authority path must not also be a derived fact: {rel}")
        if row["owner_id"] not in owner_ids:
            fail(f"{rel}: derived fact references unknown owner {row['owner_id']}")
        if row["relation"] not in DERIVED_RELATIONS:
            fail(f"{rel}: invalid derived relation {row['relation']}")
        path = require_path(root, rel, "derived fact")
        if row["primary_term"] not in path.read_text(
            encoding="utf-8", errors="replace"
        ):
            fail(f"{rel}: primary term is missing: {row['primary_term']}")

    actual = {
        path.relative_to(root).as_posix()
        for path in (root / "src/self_hosted").rglob("*_fact_owner.pgy")
    }
    classified = (actual & authority_paths) | derived_paths
    unclassified = sorted(actual - classified)
    stale = sorted(derived_paths - actual)
    if unclassified:
        fail(
            "self-host fact owner has no authority/derived classification: "
            + ", ".join(unclassified)
        )
    if stale:
        fail("derived fact registry contains stale paths: " + ", ".join(stale))


def validate_layer_inputs(root: pathlib.Path) -> None:
    codegen_root = root / "src/self_hosted/codegen"
    producer_pattern = re.compile(
        r"SemanticAst[A-Za-z0-9_]*FactsFromArtifact\s*\("
    )
    artifact_bridge_refs: list[str] = []
    for path in codegen_root.rglob("*.pgy"):
        text = path.read_text(encoding="utf-8", errors="replace")
        rel = path.relative_to(root).as_posix()
        if producer_pattern.search(text):
            fail(
                f"{rel}: codegen re-produces semantic facts; consume a "
                "semantic-owned bundle/view"
            )
        if "/input/" in rel and "SemanticAstBodyTypeBundleFromAnalysis(" in text:
            fail(
                f"{rel}: codegen input view synthesizes the semantic body "
                "bundle instead of projecting a supplied bundle"
            )
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("import ") and "parser/" in stripped:
                fail(f"{rel}: codegen imports a parser implementation owner")
            if "AstTreeArtifactFromText(" in line:
                artifact_bridge_refs.append(rel)
        if "LoadSemanticSource" in text:
            fail(f"{rel}: codegen reopened semantic source loading")

    allowed_bridge = "src/self_hosted/codegen/emission/program_entry_owner.pgy"
    if artifact_bridge_refs != [allowed_bridge]:
        fail(
            "self-host codegen AST-text bridge drifted; expected only "
            f"{allowed_bridge}, got {artifact_bridge_refs}"
        )

    native_codegen = root / "src/codegen"
    for path in native_codegen.rglob("*"):
        if path.suffix not in {".c", ".h"} or not path.is_file():
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        if "AIRProgram" in text:
            rel = path.relative_to(root).as_posix()
            fail(f"{rel}: backend reads verification-only AIR")
    for rel in (
        "src/codegen/transpiler_entry.c",
        "src/codegen/transpiler_entry.h",
        "src/codegen/llvm_api.c",
        "src/codegen/llvm_api.h",
    ):
        path = root / rel
        if path.is_file() and "ASTNode *" in path.read_text(
            encoding="utf-8", errors="replace"
        ):
            fail(f"{rel}: public backend boundary accepts raw AST authority")


def load_registry(root: pathlib.Path):
    registry_path = root / "docs/semantics/sot_owner_spine_registry.md"
    text = registry_path.read_text(encoding="utf-8")
    owners = [
        OwnerRow(row)
        for row in block_rows(text, OWNER_BEGIN, OWNER_END, OWNER_FIELDS)
    ]
    derived = [
        DerivedRow(row)
        for row in block_rows(text, DERIVED_BEGIN, DERIVED_END, DERIVED_FIELDS)
    ]
    return text, owners, derived


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    try:
        text, owners, derived = load_registry(root)
        indexed = source_index(
            root, [row["producer_term"] for row in owners]
        )
        counts = validate_owner_rows(root, text, owners, indexed)
        validate_derived_rows(root, owners, derived)
        validate_layer_inputs(root)
    except (GateFailure, OSError) as exc:
        print(f"[sot-authority-edge] FAIL {exc}", file=sys.stderr)
        return 1
    print(
        "[sot-authority-edge] "
        f"{len(owners)} authorities, {len(derived)} derived fact carriers; "
        f"CLOSED={counts['CLOSED']} BRIDGE={counts['BRIDGE']} "
        f"ACTIVE={counts['ACTIVE']}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
