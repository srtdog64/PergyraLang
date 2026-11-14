#!/usr/bin/env python3
"""Structured falsifiers for initialized Array<Int> sum and static set."""
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


emit("display-only", lambda d: [row.__setitem__("expr0", "display-only")
     for part in d["routines"][0]["blocks"] for row in part["instructions"]
     if row.get("expr0") is not None])


def array_values(doc):
    nodes = instruction(doc, 3)["expr0_graph"]["nodes"]
    for row, value in zip((1, 3, 5), ("4", "5", "6")):
        nodes[row]["text"] = value


emit("array-values", array_values)
emit("set-value-42", lambda d: instruction(d, 9)["expr0_graph"]["nodes"][0].
     __setitem__("text", "42"))
emit("set-index-two", lambda d: instruction(d, 9)["expr1_graph"]["nodes"][0].
     __setitem__("text", "2"))
emit("post-read-index-two", lambda d: instruction(d, 10)["expr0_graph"]["nodes"][3].
     __setitem__("text", "2"))
emit("phi-permuted", lambda d: [instruction(d, 0)["uses"].reverse(),
     instruction(d, 1)["uses"].reverse()])

emit("bad-array-root", lambda d: instruction(d, 3)["expr0_graph"].
     __setitem__("root", 4))
emit("bad-array-spine", lambda d: instruction(d, 3)["expr0_graph"]["nodes"][6].
     __setitem__("left", 2))
emit("bad-array-element-kind", lambda d: instruction(d, 3)["expr0_graph"]["nodes"][3].
     __setitem__("kind", "string_literal"))
emit("bad-array-overflow", lambda d: instruction(d, 3)["expr0_graph"]["nodes"][5].
     __setitem__("text", "2147483648"))
emit("bad-source-type", lambda d: d["routines"][0]["source_locals"][0].
     __setitem__("type", "Array<String>"))
emit("bad-duplicate-local", lambda d: d["routines"][0]["source_locals"].
     append({"name": "xs", "type": "Array<Int>"}))
emit("bad-array-layout", lambda d: instruction(d, 3)["abi_layout"]["fields"][1].
     __setitem__("offset", 16))
emit("bad-stale-result", lambda d: instruction(d, 3).
     __setitem__("result", "xs.9"))


def duplicate_definition(doc):
    duplicate = copy.deepcopy(instruction(doc, 3))
    duplicate["id"] = 30
    duplicate["result"] = "xs.2"
    block(doc, 0)["instructions"].insert(1, duplicate)


emit("bad-duplicate-definition", duplicate_definition)
emit("bad-length-target", lambda d: instruction(d, 2)["expr0_graph"]["nodes"][2].
     __setitem__("call_target_name", "ArrayCapacity"))
emit("bad-length-edge", lambda d: instruction(d, 2)["expr0_graph"]["nodes"][5].
     __setitem__("right", 0))
emit("bad-length-use", lambda d: instruction(d, 2)["uses"].
     __setitem__(1, "total.4"))


def swap_guard_edges(doc):
    owner = block(doc, 1)
    owner["succ_true"], owner["succ_false"] = \
        owner["succ_false"], owner["succ_true"]


emit("bad-guard-edges", swap_guard_edges)
emit("bad-backedge", lambda d: block(d, 2).__setitem__("succ_true", 3))
emit("bad-step", lambda d: instruction(d, 7)["expr0_graph"]["nodes"][1].
     __setitem__("text", "2"))
emit("bad-index-phi", lambda d: instruction(d, 1)["uses"].
     __setitem__(0, "total.1"))
emit("bad-sum-phi", lambda d: instruction(d, 0)["uses"].
     __setitem__(1, "total.1"))
emit("bad-read-accumulator", lambda d: instruction(d, 6)["uses"].
     __setitem__(0, "i.5"))
emit("bad-read-collection", lambda d: instruction(d, 6)["uses"].
     __setitem__(1, "i.5"))
emit("bad-read-index", lambda d: instruction(d, 6)["uses"].
     __setitem__(2, "i.1"))
emit("bad-read-edge", lambda d: instruction(d, 6)["expr0_graph"]["nodes"][3].
     __setitem__("right", 0))
emit("bad-set-receiver", lambda d: instruction(d, 9).
     __setitem__("uses", ["total.4"]))
emit("bad-set-negative-index", lambda d: instruction(d, 9)["expr1_graph"]["nodes"][0].
     __setitem__("text", "-1"))
emit("bad-set-oob-index", lambda d: instruction(d, 9)["expr1_graph"]["nodes"][0].
     __setitem__("text", "3"))
emit("bad-set-value-kind", lambda d: instruction(d, 9)["expr0_graph"]["nodes"][0].
     __setitem__("kind", "string_literal"))
emit("bad-set-value-overflow", lambda d: instruction(d, 9)["expr0_graph"]["nodes"][0].
     __setitem__("text", "2147483648"))


def duplicate_set(doc):
    duplicate = copy.deepcopy(instruction(doc, 9))
    duplicate["id"] = 90
    block(doc, 3)["instructions"].insert(2, duplicate)


emit("bad-duplicate-set", duplicate_set)


def set_after_read(doc):
    rows = block(doc, 3)["instructions"]
    rows[1], rows[2] = rows[2], rows[1]


emit("bad-set-order", set_after_read)
emit("bad-post-read-receiver", lambda d: instruction(d, 10).
     __setitem__("uses", ["total.4"]))
emit("bad-post-read-negative", lambda d: instruction(d, 10)["expr0_graph"]["nodes"][3].
     __setitem__("text", "-1"))
emit("bad-post-read-oob", lambda d: instruction(d, 10)["expr0_graph"]["nodes"][3].
     __setitem__("text", "3"))
emit("bad-post-read-edge", lambda d: instruction(d, 10)["expr0_graph"]["nodes"][4].
     __setitem__("right", 2))
emit("bad-final-length-target", lambda d: instruction(d, 11)["expr0_graph"]["nodes"][3].
     __setitem__("call_target_name", "ArrayCapacity"))
emit("bad-final-length-use", lambda d: instruction(d, 11).
     __setitem__("uses", ["total.4"]))
emit("bad-extra-collection-use", lambda d: instruction(d, 8)["uses"].
     append("xs.1"))
