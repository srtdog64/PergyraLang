#!/usr/bin/env python3
"""Mutate the formal-record member ArraySet lane-identity contract."""

import json
import sys


if len(sys.argv) != 4:
    raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
input_path, kind, output_path = sys.argv[1:]
with open(input_path, encoding="utf-8") as source:
    document = json.load(source)

declarations = document.get("decls", document.get("declarations", []))
declaration = next(
    (row for row in declarations if row.get("name") == "CollectionIndex"),
    None,
)
routine = next(
    (row for row in document.get("routines", [])
     if row.get("name") == "CollectionIndexOwnerAt"),
    None,
)
instruction = None
if routine is not None:
    instruction = next(
        (row for block in routine.get("blocks", [])
         for row in block.get("instructions", [])
         if row.get("kind") == "stmt" and row.get("arg0") == "ArraySet"),
        None,
    )
if declaration is None or instruction is None:
    raise SystemExit("fixture lacks the collection-index owner seam")
nodes = instruction.get("expr0_graph", {}).get("nodes", [])
if len(nodes) != 5 or len(instruction.get("uses", [])) != 3:
    raise SystemExit("collection-index ArraySet shape drifted")

if kind == "use-order":
    instruction["uses"][1], instruction["uses"][2] = (
        instruction["uses"][2], instruction["uses"][1]
    )
elif kind == "missing-formal":
    nodes[0]["binding_kind"] = "none"
    nodes[0]["binding_ordinal"] = None
elif kind == "wrong-formal":
    nodes[0]["binding_ordinal"] = 1
elif kind == "wrong-index-type":
    nodes[3]["text"] = "index"
    nodes[3]["binding_kind"] = "formal_parameter"
    nodes[3]["binding_ordinal"] = 0
elif kind == "field-type":
    field = next(
        (row for row in declaration.get("fields", [])
         if row.get("name") == "field_owner_indices"),
        None,
    )
    if field is None or field.get("type") != "Array<Int>":
        raise SystemExit("fixture lacks the Array<Int> owner-index field")
    field["type"] = "Array<String>"
else:
    raise SystemExit(f"unknown mutation: {kind}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output:
    json.dump(document, output, separators=(",", ":"))
    output.write("\n")
