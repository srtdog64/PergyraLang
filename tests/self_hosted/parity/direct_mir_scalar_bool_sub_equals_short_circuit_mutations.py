#!/usr/bin/env python3
"""Fail-closed mutations for Bool short-circuit SubEqualsWithLen admission."""

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
routines = [row for row in document["routines"] if row["name"] == "JsonTrueAt"]
if len(routines) != 1:
    fail("JsonTrueAt fixture routine is missing")
instruction = routines[0]["blocks"][0]["instructions"][0]
graph = instruction["expr0_graph"]
nodes = graph["nodes"]

if mode == "call-name":
    nodes[6]["call_target_name"] = "SubIndexOfWithLen"
elif mode == "call-kind":
    nodes[6]["call_target_kind"] = "none"
elif mode == "call-syntax-id":
    nodes[6]["call_target_syntax_id"] = 999999
elif mode == "argument-chain":
    nodes[16]["right"] = 13
elif mode == "short-circuit-edge":
    nodes[17]["right"] = None
else:
    fail(f"unknown mutation: {mode}")

output.write_text(
    json.dumps(document, ensure_ascii=False, separators=(",", ":")),
    encoding="utf-8",
)
