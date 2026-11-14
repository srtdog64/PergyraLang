#!/usr/bin/env python3
"""Produce one fail-closed mutation of the Option<Int> try-let MIR."""

import json
import pathlib
import sys


def fail(message: str) -> None:
    raise SystemExit(message)


if len(sys.argv) != 4:
    fail("usage: mutations.py INPUT MODE OUTPUT")

source = pathlib.Path(sys.argv[1])
mode = sys.argv[2]
output = pathlib.Path(sys.argv[3])
document = json.loads(source.read_text(encoding="utf-8"))

routines = {row["name"]: row for row in document["routines"]}
if "PickTryValue" not in routines or "DoubleTryValue" not in routines:
    fail("try-let fixture routines are missing")

producer = routines["PickTryValue"]
consumer = routines["DoubleTryValue"]
try_instruction = consumer["blocks"][0]["instructions"][0]

if mode == "payload-type":
    producer["return"] = "Option<String>"
elif mode == "enclosing-return":
    consumer["return"] = "Int"
elif mode == "try-edge":
    graph = try_instruction["expr0_graph"]
    graph["nodes"][graph["root"]]["left"] = None
elif mode == "option-abi":
    changed = False
    for routine in document["routines"]:
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                layout = instruction.get("abi_layout")
                if instruction.get("abi_type_name") == "Option<Int>" and layout:
                    layout["fields"][1]["offset"] = 0
                    changed = True
    if not changed:
        fail("Option<Int> ABI receipt is missing")
elif mode == "value-result":
    consumer["params"][0]["carriage"] = "value-result"
else:
    fail(f"unknown mutation: {mode}")

output.write_text(
    json.dumps(document, ensure_ascii=False, separators=(",", ":")),
    encoding="utf-8",
)
