#!/usr/bin/env python3
"""Fail-closed mutations for Option<Bool> equality short-circuit admission."""

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
matches = [row for row in document["routines"] if row["name"] == "Matches"]
if len(matches) != 1:
    fail("Matches fixture routine is missing")
routine = matches[0]
instructions = [
    instruction
    for block in routine["blocks"]
    for instruction in block["instructions"]
]
returns = [row for row in instructions if row["kind"] == "return"]
definitions = [row for row in instructions if row["kind"] == "def"]
if len(returns) != 1 or len(definitions) != 1:
    fail("Matches fixture instruction inventory drifted")
instruction = returns[0]
nodes = instruction["expr0_graph"]["nodes"]

if mode == "equality-kind":
    nodes[9]["kind"] = "greater"
elif mode == "equality-right-type":
    nodes[8]["text"] = "wrapped"
    nodes[8]["binding_kind"] = "none"
    nodes[8]["binding_ordinal"] = None
elif mode == "unwrap-call-identity":
    nodes[5]["call_target_kind"] = "none"
elif mode == "missing-local-use":
    instruction["uses"] = []
elif mode == "option-abi-layout":
    definitions[0]["abi_layout"]["size"] = 9
elif mode == "short-circuit-edge":
    nodes[10]["right"] = None
else:
    fail(f"unknown mutation: {mode}")

output.write_text(
    json.dumps(document, ensure_ascii=False, separators=(",", ":")),
    encoding="utf-8",
)
