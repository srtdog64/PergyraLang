#!/usr/bin/env python3
"""Structured falsifiers for the read-only Array<Int> range maximum."""
import copy
import json
import sys
from pathlib import Path

source = Path(sys.argv[1])
out = Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))


def block(doc, block_id):
    return next(row for row in doc["routines"][0]["blocks"]
                if row["id"] == block_id)


def instruction(doc, instruction_id):
    return next(row for part in doc["routines"][0]["blocks"]
                for row in part["instructions"] if row["id"] == instruction_id)


def emit(name, change):
    doc = copy.deepcopy(base)
    change(doc)
    (out / f"{name}.json").write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def set_array(doc, values):
    nodes = instruction(doc, 4)["expr0_graph"]["nodes"]
    for row, value in zip((1, 3, 5, 7, 9), values):
        nodes[row]["text"] = str(value)


emit("display-only", lambda d: [row.__setitem__("expr0", "display-only")
     for part in d["routines"][0]["blocks"] for row in part["instructions"]
     if row.get("expr0") is not None])
emit("phi-permuted", lambda d: [instruction(d, 0)["uses"].reverse(),
     instruction(d, 3)["uses"].reverse()])
emit("first-max", lambda d: set_array(d, (12, 7, 2, 9, 4)))
emit("last-max", lambda d: set_array(d, (3, 7, 2, 9, 14)))
emit("all-negative", lambda d: set_array(d, (-8, -3, -11, -2, -6)))

emit("bad-array-root", lambda d: instruction(d, 4)["expr0_graph"].
     __setitem__("root", 8))
emit("bad-array-overflow", lambda d: instruction(d, 4)["expr0_graph"]["nodes"][7].
     __setitem__("text", "2147483648"))
emit("bad-source-type", lambda d: d["routines"][0]["source_locals"][0].
     __setitem__("type", "Array<String>"))
emit("bad-array-layout", lambda d: instruction(d, 4)["abi_layout"]["fields"][1].
     __setitem__("offset", 16))


def duplicate_definition(doc):
    duplicate = copy.deepcopy(instruction(doc, 4))
    duplicate["id"] = 40
    duplicate["result"] = "xs.2"
    block(doc, 0)["instructions"].insert(1, duplicate)


emit("bad-duplicate-definition", duplicate_definition)
emit("bad-initial-index", lambda d: instruction(d, 5)["expr0_graph"]["nodes"][1].
     __setitem__("text", "1"))
emit("bad-initial-receiver", lambda d: instruction(d, 5)["uses"].
     __setitem__(0, "best.1"))
emit("bad-range-start", lambda d: instruction(d, 6)["expr0_graph"]["nodes"][0].
     __setitem__("text", "0"))
emit("bad-range-capacity", lambda d: instruction(d, 1)["expr0_graph"]["nodes"][1].
     __setitem__("call_target_name", "ArrayCapacity"))
emit("bad-range-use", lambda d: instruction(d, 1)["uses"].
     __setitem__(0, "best.4"))


def swap_edges(doc, block_id):
    owner = block(doc, block_id)
    owner["succ_true"], owner["succ_false"] = \
        owner["succ_false"], owner["succ_true"]


emit("bad-range-edges", lambda d: swap_edges(d, 1))
emit("bad-compare-kind", lambda d: instruction(d, 2)["expr0_graph"]["nodes"][4].
     __setitem__("kind", "less"))
emit("bad-compare-stale", lambda d: instruction(d, 2)["uses"].
     __setitem__(1, "best.1"))
emit("bad-compare-edges", lambda d: swap_edges(d, 2))
emit("bad-update-index", lambda d: instruction(d, 7)["expr0_graph"]["nodes"][1].
     __setitem__("text", "j"))
emit("bad-update-target", lambda d: instruction(d, 7).__setitem__("arg0", "i"))
emit("bad-update-edge", lambda d: block(d, 3).__setitem__("succ_true", 5))
emit("bad-header-phi", lambda d: instruction(d, 0)["uses"].
     __setitem__(1, "best.7"))
emit("bad-join-phi", lambda d: instruction(d, 3)["uses"].
     __setitem__(0, "best.4"))
emit("bad-log-stale", lambda d: instruction(d, 8)["uses"].
     __setitem__(0, "best.1"))
emit("bad-log-target", lambda d: instruction(d, 8)["expr0_graph"]["nodes"][1].
     __setitem__("call_target_name", "Stringify"))
emit("bad-extra-collection-use", lambda d: instruction(d, 8)["uses"].
     append("xs.1"))


def duplicate_log(doc):
    duplicate = copy.deepcopy(instruction(doc, 8))
    duplicate["id"] = 80
    block(doc, 5)["instructions"].append(duplicate)


emit("bad-duplicate-log", duplicate_log)
