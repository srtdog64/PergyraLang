"""Bounded falsifiers for the executed ability-bind parent's identity claim."""

import copy
import json
import pathlib
import sys


def method(doc, owner):
    declaration = next(row for row in doc["decls"] if row["name"] == owner)
    return next(row for row in declaration["methods"] if row["name"] == "Put")


def rewrite_target(value, old, new):
    if isinstance(value, dict):
        if value.get("call_target_syntax_id") == old:
            value["call_target_syntax_id"] = new
        for child in value.values():
            rewrite_target(child, old, new)
    elif isinstance(value, list):
        for child in value:
            rewrite_target(child, old, new)


def main():
    source, destination = map(pathlib.Path, sys.argv[1:])
    original = json.loads(source.read_text(encoding="utf-8"))
    ability_id = method(original, "Bufferable")["source_syntax_id"]
    runtime_id = method(original, "IntBuffer")["source_syntax_id"]
    routine = next(row for row in original["routines"]
                   if row.get("owner") == "IntBuffer" and row["name"] == "Put")
    assert ability_id > 0 and runtime_id > 0 and ability_id != runtime_id
    assert runtime_id == routine["source_syntax_id"]
    destination.mkdir(parents=True, exist_ok=True)

    for name in ("missing-ability-id", "zero-ability-id", "negative-ability-id",
                 "string-ability-id", "unknown-target", "duplicate-id",
                 "crossed-owner-ids", "missing-runtime-id", "unknown-runtime-id",
                 "declarations-reordered", "ability-rekeyed"):
        doc = copy.deepcopy(original)
        ability = method(doc, "Bufferable")
        runtime = method(doc, "IntBuffer")
        if name == "missing-ability-id":
            del ability["source_syntax_id"]
        elif name == "zero-ability-id":
            ability["source_syntax_id"] = 0
        elif name == "negative-ability-id":
            ability["source_syntax_id"] = -1
        elif name == "string-ability-id":
            ability["source_syntax_id"] = str(ability_id)
        elif name == "unknown-target":
            rewrite_target(doc, ability_id, ability_id + 100000)
        elif name == "duplicate-id":
            ability["source_syntax_id"] = runtime_id
        elif name == "crossed-owner-ids":
            ability["source_syntax_id"], runtime["source_syntax_id"] = runtime_id, ability_id
        elif name == "missing-runtime-id":
            del runtime["source_syntax_id"]
        elif name == "unknown-runtime-id":
            runtime["source_syntax_id"] += 100000
        elif name == "declarations-reordered":
            doc["decls"].reverse()
        elif name == "ability-rekeyed":
            ability["source_syntax_id"] += 100000
            rewrite_target(doc, ability_id, ability["source_syntax_id"])
        (destination / f"{name}.mir.json").write_text(
            json.dumps(doc, separators=(",", ":")), encoding="utf-8")


if __name__ == "__main__":
    main()
