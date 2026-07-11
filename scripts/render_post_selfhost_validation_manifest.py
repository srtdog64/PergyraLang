#!/usr/bin/env python3
"""Render and verify the frozen post-self-host validation table."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


BEGIN = "<!-- BEGIN GENERATED BUG CLASS TABLE -->"
END = "<!-- END GENERATED BUG CLASS TABLE -->"


def claim(row: dict[str, object]) -> str:
    if row["static_claim"] and row["runtime_claim"]:
        return "S+R"
    return "S" if row["static_claim"] else "R"


def cell(value: object) -> str:
    return str(value).replace("|", "\\|")


def render(data: dict[str, object]) -> str:
    rows = data["bug_classes"]
    totals = data["claim_totals"]
    policy = data["pass_policy"]
    lines = [
        BEGIN,
        "| # | Bug class | Claim | Crawler fixture sketch | Machinery |",
        "|---:|---|:---:|---|---|",
    ]
    for row in rows:
        lines.append(
            f"| {row['id']} | {cell(row['title'])} | {claim(row)} | "
            f"{cell(row['crawler_fixture_sketch'])} | {cell(row['machinery'])} |"
        )
    lines.extend([
        "",
        "Generated claim totals: "
        f"N={totals['total']}; S-containing={totals['static']}; "
        f"R-containing={totals['runtime']}; S-only={totals['static_only']}; "
        f"R-only={totals['runtime_only']}; S+R={totals['static_and_runtime']}.",
        "",
        "- **PASS**: >= "
        f"{policy['static_required']} of {totals['static']} S-containing classes "
        "are caught statically by Pergyra and missed by the conventional port, "
        f"AND all {policy['runtime_required']} R-containing classes fail closed "
        "deterministically with the same C/LLVM observable.",
        "- **FAIL**: most classes are caught by neither, or by both; the "
        "domain-meaning layer then buys little over a conventional type system.",
        END,
    ])
    return "\n".join(lines)


def replace_block(document: str, block: str) -> str:
    start = document.find(BEGIN)
    end = document.find(END)
    if start < 0 or end < start:
        raise ValueError("generated bug-class markers are missing")
    end += len(END)
    return document[:start] + block + document[end:]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("document", type=Path)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true")
    mode.add_argument("--write", action="store_true")
    args = parser.parse_args()

    data = json.loads(args.manifest.read_text(encoding="utf-8"))
    document = args.document.read_text(encoding="utf-8")
    expected = replace_block(document, render(data))
    if args.check:
        if expected != document:
            raise SystemExit("post-self-host validation document drifted from manifest")
        return 0
    args.document.write_text(expected, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
