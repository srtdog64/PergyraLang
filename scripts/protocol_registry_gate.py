#!/usr/bin/env python3
"""Validate the derived Protocol/ABI/API crosswalk without creating a new SoT."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


BEGIN = "<!-- BEGIN protocol-abi-api-registry -->"
END = "<!-- END protocol-abi-api-registry -->"
FIELDS = (
    "protocol_id",
    "owner_ref",
    "wire_layout",
    "producer",
    "last_consumer",
    "projection",
    "missing_fact_failure",
    "compatibility_policy",
    "gate",
    "status",
)
STATUSES = {"IMPLEMENTED", "BRIDGE", "OPEN"}
PROTOCOL_ID = re.compile(
    r"^pergyra\.(?:[a-z0-9-]+\.)+(?:v[0-9]+|unversioned)$"
)


class GateFailure(RuntimeError):
    pass


def fail(message: str) -> None:
    raise GateFailure(message)


def load_rows(text: str) -> list[dict[str, str]]:
    if BEGIN not in text or END not in text:
        fail("protocol registry markers are missing")
    body = text.split(BEGIN, 1)[1].split(END, 1)[0]
    rows: list[dict[str, str]] = []
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("```"):
            continue
        values = [part.strip() for part in line.split("|")]
        if len(values) != len(FIELDS):
            fail(f"protocol row must have {len(FIELDS)} fields: {line}")
        rows.append(dict(zip(FIELDS, values)))
    if not rows:
        fail("protocol registry has no rows")
    return rows


def owner_ids(root: pathlib.Path) -> set[str]:
    path = root / "docs/semantics/sot_owner_spine_registry.md"
    text = path.read_text(encoding="utf-8")
    marker = "<!-- BEGIN sot-owner-spine-registry -->"
    end = "<!-- END sot-owner-spine-registry -->"
    if marker not in text or end not in text:
        fail("canonical SoT owner registry markers are missing")
    body = text.split(marker, 1)[1].split(end, 1)[0]
    result: set[str] = set()
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("```"):
            continue
        result.add(line.split("|", 1)[0].strip())
    if not result:
        fail("canonical SoT owner registry has no owner rows")
    return result


def check_ref(root: pathlib.Path, ref: str, field: str, *, allow_gap: bool) -> None:
    ref = ref.strip()
    if ref.startswith("UNSPECIFIED:") or ref.startswith("UNREGISTERED:"):
        if allow_gap and len(ref.split(":", 1)[1].strip()) > 0:
            return
        fail(f"{field} has an invalid gap marker: {ref}")
    if "#" not in ref:
        path = root / pathlib.PurePosixPath(ref)
        if not path.is_file():
            fail(f"{field} path does not exist: {ref}")
        return
    rel, term = ref.split("#", 1)
    if not rel or not term:
        fail(f"{field} has an empty path or term: {ref}")
    path = root / pathlib.PurePosixPath(rel)
    if not path.is_file():
        fail(f"{field} path does not exist: {rel}")
    if term not in path.read_text(encoding="utf-8", errors="replace"):
        fail(f"{field} term is absent from {rel}: {term}")


def check_refs(root: pathlib.Path, value: str, field: str, *, allow_gap: bool) -> None:
    if not value.strip():
        fail(f"{field} must not be empty")
    parts = [item.strip() for item in value.split(";") if item.strip()]
    if not parts:
        fail(f"{field} must not be empty")
    for item in parts:
        check_ref(root, item, field, allow_gap=allow_gap)


def validate(root: pathlib.Path) -> list[dict[str, str]]:
    doc = root / "docs/192_protocol_abi_api_registry.md"
    rows = load_rows(doc.read_text(encoding="utf-8"))
    ids = [row["protocol_id"] for row in rows]
    if len(ids) != len(set(ids)):
        fail("protocol registry contains duplicate protocol IDs")
    owners = owner_ids(root)
    for row in rows:
        protocol_id = row["protocol_id"]
        if not PROTOCOL_ID.fullmatch(protocol_id):
            fail(f"invalid protocol ID/version: {protocol_id}")
        status = row["status"]
        if status not in STATUSES:
            fail(f"{protocol_id}: invalid status {status}")
        owner = row["owner_ref"]
        if owner.startswith("registry:"):
            owner_id = owner.split(":", 1)[1]
            if owner_id not in owners:
                fail(f"{protocol_id}: owner is absent from canonical SoT registry: {owner_id}")
        elif owner.startswith("UNREGISTERED:"):
            if status == "IMPLEMENTED":
                fail(f"{protocol_id}: unregistered owner cannot be IMPLEMENTED")
        else:
            fail(f"{protocol_id}: owner_ref must be registry:<id> or UNREGISTERED:<reason>")

        for field in FIELDS[2:-1]:
            check_refs(
                root,
                row[field],
                f"{protocol_id}: {field}",
                allow_gap=True,
            )
        if status == "IMPLEMENTED":
            for field in ("missing_fact_failure", "compatibility_policy", "gate"):
                if row[field].startswith("UNSPECIFIED:"):
                    fail(f"{protocol_id}: IMPLEMENTED row has unspecified {field}")
    return rows


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", nargs="?", default=".")
    args = parser.parse_args()
    root = pathlib.Path(args.root).resolve()
    try:
        rows = validate(root)
    except (GateFailure, OSError) as exc:
        print(f"[protocol-registry] FAIL {exc}", file=sys.stderr)
        return 1
    print(f"[protocol-registry] {len(rows)} protocol rows valid; no authority duplicated")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
