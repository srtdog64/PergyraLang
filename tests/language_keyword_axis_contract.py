#!/usr/bin/env python3
"""Source-consistency checks, not semantic proof or another keyword registry."""

from __future__ import annotations

import argparse
from collections import Counter
from pathlib import Path
import re
import sys
from tempfile import TemporaryDirectory

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "scripts"))
import render_language_keyword_registry as registry


# Bind representative model USES to surface spellings, not to copied axes.
# Actual axes are read from keyword_axis below. Unknown constructors fail closed.
MODEL_SURFACE = {
    "KwSubject": "subject",
    "KwIntentWho": "intent",
    "KwZone": "zone",
    "KwAuthority": "authority",
    "KwEffect": "effect",
    "KwAbility": "ability",
    "KwSlot": "slot",
    "KwParallel": "parallel",
}
DOC_FACT_LABELS = {
    "Resource": "RESOURCE", "Execution": "EXECUTION", "Domain": "DOMAIN",
    "Type/Contract": "TYPE_CONTRACT",
}
MODEL_AXIS = {
    "AxResource": "RESOURCE", "AxExecution": "EXECUTION",
    "AxDomain": "DOMAIN", "AxTypeContract": "TYPE_CONTRACT",
}
PREFIX = "PGY_KEYWORD_AXIS_"
GENERIC_SCOPE_MARKER = "<!-- GENERIC-SURFACE-USES-NOT-PRIMARY-CATEGORIES -->"


def section(source: str, heading: str) -> str:
    match = re.search(
        rf"(?ms)^{re.escape(heading)}\s*\n(.*?)(?=^## |\Z)", source
    )
    if match is None:
        raise ValueError(f"missing axis contract section: {heading}")
    return match.group(1)


def doc_categories(source: str, labels: dict[str, str]) -> dict[str, str]:
    categories: dict[str, str] = {}
    seen_labels: set[str] = set()
    for line in source.splitlines():
        cells = [cell.strip() for cell in line.split("|")]
        if len(cells) != 5 or cells[1] not in labels:
            continue
        label = cells[1]
        if label in seen_labels:
            raise ValueError(f"duplicate axis category row: {label}")
        seen_labels.add(label)
        words = re.findall(r"`([a-z]+)`", cells[3])
        if not words:
            raise ValueError(f"empty axis category row: {label}")
        for word in words:
            if word in categories:
                raise ValueError(f"doc declares {word} more than one category")
            categories[word] = labels[label]
    if seen_labels != set(labels):
        raise ValueError(f"missing doc category rows: {sorted(set(labels) - seen_labels)}")
    return categories


def native_axis_values(source: str) -> dict[str, int]:
    source = re.sub(r"/\*.*?\*/", "", source, flags=re.DOTALL)
    match = re.search(
        r"typedef\s+enum\s*\{([^{}]+)\}\s*PgyLanguageKeywordAxis\s*;", source
    )
    if match is None:
        raise ValueError("missing native primary-category enum")
    values: dict[str, int] = {}
    value = -1
    for item in match.group(1).split(","):
        if not item.strip():
            continue
        row = re.fullmatch(r"\s*(PGY_KEYWORD_AXIS_[A-Z_]+)(?:\s*=\s*(\d+))?\s*", item)
        if row is None or row.group(1) in values:
            raise ValueError(f"unsupported or duplicate native axis entry: {item}")
        value = int(row.group(2)) if row.group(2) is not None else value + 1
        values[row.group(1)] = value
    return values


def model_axes(source: str) -> dict[str, str]:
    # This source check deliberately admits only the small pinned table shape;
    # it neither interprets Rocq nor substitutes for a kernel check.
    source = re.sub(r"\(\*.*?\*\)", "", source, flags=re.DOTALL)
    enum = re.search(r"Inductive Keyword\s*:\s*Type\s*:=([^.]*)\.", source)
    body = re.search(
        r"Definition keyword_axis\s*\(k\s*:\s*Keyword\)\s*:\s*Axis\s*:=\s*"
        r"match k with(.*?)end\.", source, flags=re.DOTALL,
    )
    if enum is None or body is None:
        raise ValueError("missing actual Rocq keyword_axis definition/Keyword enum")
    constructors = re.findall(r"\bKw[A-Za-z0-9_]+\b", enum.group(1))
    if len(constructors) != len(set(constructors)) or set(constructors) != set(MODEL_SURFACE):
        raise ValueError("representative model surface binding drift; not a 147-word model")
    arm_pattern = r"\|\s*(Kw[A-Za-z0-9_]+)\s*=>\s*(Ax[A-Za-z0-9_]+)"
    arms = re.findall(arm_pattern, body.group(1))
    if re.sub(arm_pattern, "", body.group(1)).strip():
        raise ValueError("unsupported actual Rocq keyword_axis arm shape")
    if len(arms) != len(MODEL_SURFACE) or set(dict(arms)) != set(MODEL_SURFACE):
        raise ValueError("actual Rocq keyword_axis arm coverage drift")
    if any(axis not in MODEL_AXIS for _, axis in arms):
        raise ValueError("unknown actual Rocq keyword_axis owner")
    return {MODEL_SURFACE[ctor]: MODEL_AXIS[axis] for ctor, axis in arms}


def check(paths: dict[str, Path]) -> tuple[int, int, int]:
    rows = registry.load_rows(paths["registry"])
    actual = {row.spelling: row.axis.removeprefix(PREFIX) for row in rows}
    doc = paths["doc"].read_text(encoding="utf-8")
    # Pin the reached document's scope correction, not a carriage-law proof.
    generic_doc = paths["generic_doc"].read_text(encoding="utf-8")
    if GENERIC_SCOPE_MARKER not in generic_doc:
        raise ValueError("missing generic surface/category scope boundary")
    if "각 착지 키워드는 정확히 한 축에 속한다" in generic_doc or "키워드-측 단사" in generic_doc:
        raise ValueError("generic surface witnesses reopened spelling-axis injectivity")
    categories = doc_categories(
        section(doc, "## 1.1 Registry Primary Categories Are Not Fact Ownership"),
        {key.removeprefix(PREFIX): key.removeprefix(PREFIX) for key in registry.AXIS_VALUES},
    )
    surface = doc_categories(
        section(doc, "## 1. Four Top-Level Semantic Fact Axes"), DOC_FACT_LABELS
    )
    modeled = model_axes(paths["model"].read_text(encoding="utf-8"))
    problems: list[str] = []
    native = native_axis_values(paths["header"].read_text(encoding="utf-8"))
    expected = {key: value[0] for key, value in registry.AXIS_VALUES.items()}
    if native != expected:
        problems.append(f"native enum/generator axis identity drift: {native} != {expected}")
    for label, projection in (("primary doc", categories), ("surface doc", surface)):
        for word, axis in projection.items():
            if actual.get(word) != axis:
                problems.append(f"{label} category drift for {word}: registry={actual.get(word)}, doc={axis}")
    for word, axis in modeled.items():
        if surface.get(word) != axis or actual.get(word) != axis:
            problems.append(f"actual model axis drift for {word}: model={axis}, doc={surface.get(word)}, registry={actual.get(word)}")
    if problems:
        raise ValueError("\n".join(problems))
    census = Counter(actual.values())
    print("[axis-keyword] primary census: " + "; ".join(f"{key}={census[key]}" for key in sorted(census)))
    return len(rows), len(categories), len(modeled)


def mutate_category(source: str, word: str, axis: str) -> str:
    pattern = rf'(?ms)(^PGY_LANGUAGE_KEYWORD\(\s*"{re.escape(word)}".*?){PREFIX}[A-Z_]+'
    result, count = re.subn(pattern, lambda match: match.group(1) + PREFIX + axis, source, count=1)
    if count != 1 or result == source:
        raise ValueError(f"negative control did not change {word}")
    return result


def selftest(paths: dict[str, Path]) -> int:
    source = {key: path.read_text(encoding="utf-8") for key, path in paths.items()}
    mutations: list[tuple[str, str, str, str]] = []
    for word, axis in (
        ("unsafe", "EXECUTION"), ("extern", "GENERAL"), ("role", "TYPE_CONTRACT"),
        ("match", "GENERAL"), ("case", "GENERAL"), ("default", "GENERAL"),
        ("on", "EXECUTION"), ("all", "EXECUTION"), ("any", "EXECUTION"),
        ("requires", "DOMAIN"),
    ):
        mutations.append((word, "registry", mutate_category(source["registry"], word, axis), "category drift"))
    mutations.extend([
        ("unknown-axis", "registry", mutate_category(source["registry"], "unsafe", "UNKNOWN"), "unknown"),
        ("numeric-axis", "header", source["header"].replace("PGY_KEYWORD_AXIS_RESOURCE,", "PGY_KEYWORD_AXIS_RESOURCE = 7,", 1), "identity drift"),
        ("actual-model", "model", source["model"].replace("| KwParallel  => AxExecution", "| KwParallel  => AxDomain", 1), "actual model axis drift"),
        ("model-scope", "model", source["model"].replace("| KwAbility | KwSlot | KwParallel.", "| KwAbility | KwSlot | KwParallel | KwExtra.", 1), "surface binding drift"),
        ("missing-doc-section", "doc", source["doc"].replace("## 1.1 Registry Primary Categories Are Not Fact Ownership", "## Removed primary categories", 1), "missing axis contract section"),
        ("missing-generic-scope", "generic_doc", source["generic_doc"].replace(GENERIC_SCOPE_MARKER, "", 1), "missing generic surface/category scope boundary"),
        ("generic-injectivity", "generic_doc", source["generic_doc"] + "\n각 착지 키워드는 정확히 한 축에 속한다(키워드-측 단사)\n", "reopened spelling-axis injectivity"),
    ])
    with TemporaryDirectory(prefix="pgy-axis-contract-") as temporary:
        scratch = {key: Path(temporary) / key for key in paths}
        for name, changed_key, changed_source, expected_error in mutations:
            if changed_source == source[changed_key]:
                raise ValueError(f"negative control did not change {name}")
            for key, path in scratch.items():
                path.write_text(changed_source if key == changed_key else source[key], encoding="utf-8")
            try:
                check(scratch)
            except ValueError as exc:
                if expected_error not in str(exc):
                    raise ValueError(f"{name} failed for an unrelated reason: {exc}") from exc
            else:
                raise ValueError(f"negative axis control was accepted: {name}")
        scratch["doc"].unlink()
        try:
            check(scratch)
        except FileNotFoundError:
            pass  # Expected missing-input refusal, not a recovery path.
        else:
            raise ValueError("missing input was accepted")
    return len(mutations) + 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path)
    parser.add_argument("--selftest", action="store_true")
    args = parser.parse_args()
    paths = {
        "registry": args.root / "src/lexer/language_keyword_registry.def",
        "doc": args.root / "docs/42_keyword_orthogonality.md",
        "generic_doc": args.root / "docs/151_generic_axis_composition.md",
        "model": args.root / "docs/semantics/proofs/AxisOwnership.v",
        "header": args.root / "src/lexer/lexer_keywords.h",
    }
    try:
        row_count, doc_count, model_count = check(paths)
        print(f"[axis-keyword] category contract ok ({row_count} rows; {doc_count} doc examples; {model_count} actual model arms; not whole-language proof)")
        if args.selftest:
            print(f"[axis-keyword] negative controls ok ({selftest(paths)})")
    except (OSError, ValueError) as exc:
        print(f"[axis-keyword] {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
