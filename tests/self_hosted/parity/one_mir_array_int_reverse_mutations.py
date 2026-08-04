#!/usr/bin/env python3
"""Structured positives and falsifiers for fresh bounded ArrayReverse."""

import copy
import json
import sys
from pathlib import Path

source = Path(sys.argv[1])
out = Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))


def routine(doc):
    return doc["routines"][0]


def block(doc):
    return routine(doc)["blocks"][0]


def instruction(doc, instruction_id):
    return next(row for row in block(doc)["instructions"]
                if row["id"] == instruction_id)


def emit(name, change):
    doc = copy.deepcopy(base)
    change(doc)
    (out / f"{name}.json").write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def set_array(doc, values):
    nodes = instruction(doc, 0)["expr0_graph"]["nodes"]
    for row, value in zip((1, 3, 5), values):
        nodes[row]["text"] = str(value)


def renumber_values(doc):
    instruction(doc, 0)["result"] = "nums.7"
    instruction(doc, 1)["uses"][0] = "nums.7"
    instruction(doc, 1)["result"] = "reversed.8"
    for row in range(2, 6):
        instruction(doc, row)["uses"][0] = "reversed.8"


emit("display-only", lambda d: [row.__setitem__("expr0", "display-only")
     for row in block(d)["instructions"] if row.get("expr0") is not None])
emit("renumbered", renumber_values)
emit("alternate", lambda d: set_array(d, (4, -5, 6)))

emit("bad-array-root", lambda d: instruction(d, 0)["expr0_graph"].
     __setitem__("root", 4))
emit("bad-array-overflow", lambda d: instruction(d, 0)["expr0_graph"]
     ["nodes"][3].__setitem__("text", "2147483648"))
emit("bad-result-source-type", lambda d: routine(d)["source_locals"][1].
     __setitem__("type", "Array<String>"))
emit("bad-result-layout", lambda d: instruction(d, 1)["abi_layout"]
     ["fields"][1].__setitem__("offset", 16))
emit("bad-result-value-alias", lambda d: instruction(d, 1).
     __setitem__("result", "nums.1"))
emit("bad-result-name-alias", lambda d: instruction(d, 1).
     __setitem__("arg0", "nums"))
emit("bad-call-target", lambda d: instruction(d, 1)["expr0_graph"]
     ["nodes"][1].__setitem__("call_target_name", "ArraySort"))


def coherent_wrong_call(doc):
    nodes = instruction(doc, 1)["expr0_graph"]["nodes"]
    nodes[0]["text"] = "ArraySort"
    nodes[1]["text"] = "ArraySort()"
    nodes[1]["call_target_name"] = "ArraySort"


emit("bad-coherent-call", coherent_wrong_call)
emit("bad-call-edge", lambda d: instruction(d, 1)["expr0_graph"]
     ["nodes"][3].__setitem__("right", 0))
emit("bad-self-use", lambda d: instruction(d, 1)["uses"].
     __setitem__(0, "reversed.1"))


def wrong_length_target(doc):
    nodes = instruction(doc, 2)["expr0_graph"]["nodes"]
    nodes[0]["text"] = "ArrayCapacity"
    nodes[1]["text"] = "ArrayCapacity()"
    nodes[1]["call_target_name"] = "ArrayCapacity"


emit("bad-length-target", wrong_length_target)
emit("bad-length-receiver", lambda d: instruction(d, 2)["uses"].
     __setitem__(0, "nums.1"))
emit("bad-index-receiver", lambda d: instruction(d, 3)["uses"].
     __setitem__(0, "nums.1"))
emit("bad-index-oob", lambda d: instruction(d, 5)["expr0_graph"]
     ["nodes"][1].__setitem__("text", "3"))
emit("bad-index-duplicate", lambda d: instruction(d, 5)["expr0_graph"]
     ["nodes"][1].__setitem__("text", "1"))
emit("bad-extra-result-use", lambda d: instruction(d, 5)["uses"].
     append("reversed.1"))
emit("bad-missing-log", lambda d: block(d)["instructions"].pop())
emit("bad-unreachable", lambda d: block(d).__setitem__("reachable", False))
emit("bad-successor", lambda d: block(d).__setitem__("succ_true", 0))


def duplicate_reverse(doc):
    duplicate = copy.deepcopy(instruction(doc, 1))
    duplicate["id"] = 6
    duplicate["result"] = "reversed.2"
    block(doc)["instructions"].insert(2, duplicate)


emit("bad-duplicate-reverse", duplicate_reverse)
