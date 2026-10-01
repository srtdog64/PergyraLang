#!/usr/bin/env python3
"""Falsify release admission without running any rejected artifact.

The lifetime mutations reuse complete producer-issued expression graphs: an
outer digest failure alone is not the release proof being tested.
"""
from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path


def graph_digest(graph: dict) -> int:
    """Wire-digest counterpart; re-sealing must reach lifetime admission."""
    def number(value: int, field: int) -> int:
        return (value * 131 + field + 2) % 268435456

    def string(value: int, field: str) -> int:
        data = field.encode("utf-8")
        value = (value * 131 + len(data)) % 268435456
        for byte in data:
            value = (value * 131 + byte) % 268435456
        return value

    value = number(number(71, graph["root"]), len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = string(value, node[key])
        for key in ("runtime_call_abi_id", "left", "right"):
            value = number(value, -1 if node[key] is None else node[key])
    return 1073741824 + value


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("scalar", type=Path)
    parser.add_argument("own", type=Path)
    parser.add_argument("pair", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    scalar = json.loads(args.scalar.read_text(encoding="utf-8-sig"))
    owned = json.loads(args.own.read_text(encoding="utf-8-sig"))
    pair = json.loads(args.pair.read_text(encoding="utf-8-sig"))

    def emit(name: str, document: dict) -> None:
        (args.output / f"{name}.mir.json").write_text(
            json.dumps(document, separators=(",", ":")), encoding="utf-8")

    def first_instructions(document: dict) -> list[dict]:
        return next(row for row in document["routines"] if row["name"] == "Main")["blocks"][0]["instructions"]

    double = copy.deepcopy(scalar)
    rows = first_instructions(double)
    drop = next(row for row in rows if row.get("expr0") == "ArrayDrop(values)")
    replacement = next(row for row in rows if row.get("arg0") == "Log" and row.get("expr0") != "Log(values[0])")
    identity = replacement["id"]
    replacement.clear()
    replacement.update(copy.deepcopy(drop))
    replacement["id"] = identity
    emit("mir-double-drop", double)

    after = copy.deepcopy(scalar)
    rows = first_instructions(after)
    read = next(i for i, row in enumerate(rows) if row.get("expr0") == "Log(values[0])")
    release = next(i for i, row in enumerate(rows) if row.get("expr0") == "ArrayDrop(values)")
    before_id, after_id = rows[read]["id"], rows[release]["id"]
    rows[read], rows[release] = rows[release], rows[read]
    rows[read]["id"], rows[release]["id"] = before_id, after_id
    emit("mir-use-after-drop", after)

    borrow = copy.deepcopy(owned)
    forward = next(row for row in borrow["routines"] if row["name"] == "Forward")
    forward["params"][0]["carriage"] = "value"
    emit("mir-borrowed-forward", borrow)

    own_after = copy.deepcopy(owned)
    rows = first_instructions(own_after)
    read = next(i for i, row in enumerate(rows) if row.get("expr0") == "Log(values[0])")
    transfer = next(i for i, row in enumerate(rows) if row.get("expr0") == "Forward(values)")
    before_id, after_id = rows[read]["id"], rows[transfer]["id"]
    rows[read], rows[transfer] = rows[transfer], rows[read]
    rows[read]["id"], rows[transfer]["id"] = before_id, after_id
    emit("mir-use-after-transfer", own_after)

    duplicate = copy.deepcopy(pair)
    call = next(row for row in first_instructions(duplicate)
                if row.get("expr0", "").startswith("RetirePair("))
    graph = call["expr0_graph"]
    assert graph_digest(graph) == graph["digest"], "wire digest counterpart drifted"
    assert sum(node["kind"] == "leaf" and node["text"] == "second"
               for node in graph["nodes"]) == 1
    call["expr0"] = call["expr0"].replace("second", "first")
    for node in graph["nodes"]:
        node["text"] = node["text"].replace("second", "first")
    call["uses"] = [value for value in call["uses"]
                    if not value.startswith("second.")]
    graph["digest"] = graph_digest(graph)
    emit("mir-duplicate-own-argument", duplicate)


if __name__ == "__main__":
    main()
