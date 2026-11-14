#!/usr/bin/env python3
"""Structured positives and falsifiers for the bounded ArrayPop GraphPlan."""

import copy
import json
import sys
from pathlib import Path

source = Path(sys.argv[1])
out = Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))


def routine(doc):
    return doc["routines"][0]


def instructions(doc):
    return [row for block in routine(doc)["blocks"]
            for row in block["instructions"]]


def instruction(doc, instruction_id):
    return next(row for row in instructions(doc) if row["id"] == instruction_id)


def emit(name, change):
    doc = copy.deepcopy(base)
    change(doc)
    (out / f"{name}.json").write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def set_literals(doc, int_values, string_values):
    int_nodes = instruction(doc, 2)["expr0_graph"]["nodes"]
    for row, value in zip((1, 3, 5, 7), int_values):
        int_nodes[row]["text"] = str(value)
    string_nodes = instruction(doc, 6)["expr0_graph"]["nodes"]
    for row, value in zip((1, 3, 5), string_values):
        string_nodes[row]["text"] = json.dumps(value)


def renumber(doc):
    instruction(doc, 2)["result"] = "xs.7"
    for row in instructions(doc):
        row["uses"] = ["xs.7" if value == "xs.1" else value
                       for value in row.get("uses", [])]
    instruction(doc, 6)["result"] = "ws.9"
    for row in instructions(doc):
        row["uses"] = ["ws.9" if value == "ws.1" else value
                       for value in row.get("uses", [])]


def display_only(doc):
    for instruction_id in (7, 8, 11):
        instruction(doc, instruction_id)["expr0"] = "display-only"


def remove_instruction(doc, instruction_id):
    for block in routine(doc)["blocks"]:
        block["instructions"] = [row for row in block["instructions"]
                                 if row["id"] != instruction_id]


def duplicate_string_pop(doc):
    duplicate = copy.deepcopy(instruction(doc, 11))
    duplicate["id"] = 14
    routine(doc)["blocks"][3]["instructions"].insert(4, duplicate)


def move_string_pop_after_observation(doc):
    block = routine(doc)["blocks"][3]
    pop = next(row for row in block["instructions"] if row["id"] == 11)
    block["instructions"].remove(pop)
    block["instructions"].append(pop)


def wrong_length_target(doc):
    nodes = instruction(doc, 10)["expr0_graph"]["nodes"]
    nodes[2]["text"] = "ArrayCapacity"
    nodes[3]["text"] = "ArrayCapacity()"
    nodes[3]["call_target_name"] = "ArrayCapacity"


def swap_string_length_capacity(doc):
    fields = instruction(doc, 6)["abi_layout"]["fields"]
    length = next(row for row in fields if row["name"] == "length")
    capacity = next(row for row in fields if row["name"] == "capacity")
    length["offset"], capacity["offset"] = capacity["offset"], length["offset"]


emit("display-only", display_only)
emit("renumbered", renumber)
emit("alternate", lambda d: set_literals(
    d, (4, -5, 6, 7), ("x", "yy", "zzz")
))
emit("popped-only", lambda d: set_literals(
    d, (10, 20, -7, -8), ("a", "b", "removed")
))

emit("bad-first-int-receiver", lambda d: instruction(d, 7)["uses"].
     __setitem__(0, "xs.404"))
emit("bad-second-int-receiver", lambda d: instruction(d, 8)["uses"].
     __setitem__(0, "xs.404"))
emit("bad-string-cross-receiver", lambda d: instruction(d, 11)["uses"].
     __setitem__(0, "xs.1"))
emit("bad-pop-result", lambda d: instruction(d, 11).
     __setitem__("result", "popped.1"))
emit("bad-missing-int-pop", lambda d: remove_instruction(d, 8))
emit("bad-missing-string-pop", lambda d: remove_instruction(d, 11))
emit("bad-extra-string-pop", duplicate_string_pop)
emit("bad-pop-after-observation", move_string_pop_after_observation)
emit("bad-length-target", wrong_length_target)
emit("bad-string-index-oob", lambda d: instruction(d, 13)["expr0_graph"]
     ["nodes"][1].__setitem__("text", "2"))
emit("bad-string-layout", swap_string_length_capacity)
emit("bad-loop-successor", lambda d: routine(d)["blocks"][1].
     __setitem__("succ_false", 2))
