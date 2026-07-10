#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST="$ROOT_DIR/docs/semantics/boundary_migration_manifest.md"

PYTHON_BIN="${PYTHON_BIN:-}"
if [[ -z "$PYTHON_BIN" ]]; then
    if command -v python3 >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python3)"
    elif command -v python >/dev/null 2>&1; then
        PYTHON_BIN="$(command -v python)"
    fi
fi
if [[ -z "$PYTHON_BIN" ]]; then
    echo "[boundary-migration] Python is required for the manifest gate" >&2
    exit 1
fi

"$PYTHON_BIN" - "$ROOT_DIR" "$MANIFEST" <<'PY'
from __future__ import annotations

import copy
import pathlib
import sys

root = pathlib.Path(sys.argv[1])
manifest_path = pathlib.Path(sys.argv[2])
begin = "<!-- BEGIN boundary-migration-manifest -->"
end = "<!-- END boundary-migration-manifest -->"
statuses = {"shadow", "repointing", "mandatory", "retired"}


def fail(message: str) -> None:
    raise ValueError(message)


def evidence_ref(value: str, field: str) -> tuple[pathlib.Path, str]:
    if "#" not in value:
        fail(f"{field} must use path#required-text: {value}")
    rel, needle = value.split("#", 1)
    if not rel or not needle:
        fail(f"{field} has an empty path or required text: {value}")
    path = root / rel
    if not path.is_file():
        fail(f"{field} path does not exist: {rel}")
    if needle not in path.read_text(encoding="utf-8"):
        fail(f"{field} required text is absent from {rel}: {needle}")
    return path, needle


def path_list(value: str, field: str) -> list[pathlib.Path]:
    entries = [item.strip() for item in value.split(",") if item.strip()]
    if not entries:
        fail(f"{field} must not be empty")
    paths = [root / item for item in entries]
    missing = [str(path.relative_to(root)) for path in paths if not path.is_file()]
    if missing:
        fail(f"{field} path does not exist: {', '.join(missing)}")
    return paths


def parse_rows(text: str) -> list[dict[str, str]]:
    if begin not in text or end not in text:
        fail("manifest markers are missing")
    body = text.split(begin, 1)[1].split(end, 1)[0]
    rows: list[dict[str, str]] = []
    fields = (
        "migration_id",
        "fact_kind",
        "stable_handle",
        "old_owner",
        "new_owner",
        "allowed_bridge",
        "forbidden_old_reads",
        "consumer_inventory",
        "parity_fixture",
        "negative_fixture",
        "retirement_gate",
        "status",
    )
    for raw in body.splitlines():
        line = raw.strip()
        if not line or line.startswith("```"):
            continue
        values = [part.strip() for part in line.split("|")]
        if len(values) != len(fields):
            fail(f"manifest row must have {len(fields)} fields: {line}")
        rows.append(dict(zip(fields, values)))
    if not rows:
        fail("manifest has no migration rows")
    return rows


def validate(rows: list[dict[str, str]]) -> None:
    ids: set[str] = set()
    for row in rows:
        migration_id = row["migration_id"]
        if not migration_id or migration_id in ids:
            fail(f"migration_id is empty or duplicated: {migration_id}")
        ids.add(migration_id)
        if not row["fact_kind"] or not row["stable_handle"]:
            fail(f"{migration_id}: fact_kind and stable_handle are required")
        if row["status"] not in statuses:
            fail(f"{migration_id}: invalid status {row['status']}")

        old_owner = root / row["old_owner"]
        new_owner = root / row["new_owner"]
        if not new_owner.is_file():
            fail(f"{migration_id}: new owner does not exist: {row['new_owner']}")
        if row["status"] == "retired":
            if old_owner.exists():
                fail(f"{migration_id}: retired old owner still exists")
        elif not old_owner.is_file():
            fail(f"{migration_id}: active migration old owner is missing")

        bridge = row["allowed_bridge"]
        if bridge == "none":
            pass
        else:
            path_list(bridge, "allowed_bridge")
        path_list(row["consumer_inventory"], "consumer_inventory")
        evidence_ref(row["parity_fixture"], "parity_fixture")
        evidence_ref(row["negative_fixture"], "negative_fixture")
        retirement_path, _ = evidence_ref(
            row["retirement_gate"], "retirement_gate"
        )

        forbidden = [
            item.strip()
            for item in row["forbidden_old_reads"].split(",")
            if item.strip()
        ]
        if not forbidden:
            fail(f"{migration_id}: forbidden_old_reads must not be empty")
        retirement_text = retirement_path.read_text(encoding="utf-8")
        for needle in forbidden:
            if needle not in retirement_text:
                fail(
                    f"{migration_id}: retirement gate does not name forbidden "
                    f"old read: {needle}"
                )


text = manifest_path.read_text(encoding="utf-8")
rows = parse_rows(text)
validate(rows)


def must_reject(mutator, label: str) -> None:
    candidate = copy.deepcopy(rows)
    mutator(candidate)
    try:
        validate(candidate)
    except ValueError:
        return
    fail(f"self-test mutation was accepted: {label}")


must_reject(lambda rs: rs[0].__setitem__("status", "unknown"), "bad status")
must_reject(lambda rs: rs.append(copy.deepcopy(rs[0])), "duplicate id")
must_reject(
    lambda rs: rs[0].__setitem__("new_owner", "missing/new_owner.pgy"),
    "missing new owner",
)
must_reject(
    lambda rs: rs[0].__setitem__("old_owner", "AGENTS.md"),
    "retired old owner exists",
)
must_reject(
    lambda rs: rs[0].__setitem__(
        "forbidden_old_reads", "missing_retirement_token"
    ),
    "retirement gate omission",
)
must_reject(
    lambda rs: rs[0].__setitem__("consumer_inventory", ""),
    "empty consumer inventory",
)

print(
    f"[boundary-migration] {len(rows)} migration row(s) valid; "
    "negative self-tests reject drift"
)
PY
