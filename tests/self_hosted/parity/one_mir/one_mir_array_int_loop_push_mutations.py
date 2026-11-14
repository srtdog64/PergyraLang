#!/usr/bin/env python3
"""Structured positive and negative falsifiers for bounded Array<Int> push."""
import copy
import json
import sys
from pathlib import Path

source = Path(sys.argv[1])
out = Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))
routine = base["routines"][0]


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
emit("value-add", lambda d: instruction(d, 12)["expr0_graph"]["nodes"][2].
     __setitem__("kind", "add"))
emit("bound-three", lambda d: instruction(d, 1)["expr0_graph"]["nodes"][1].
     __setitem__("text", "3"))
emit("phi-permuted", lambda d: instruction(d, 0)["uses"].reverse())

emit("bad-push-receiver", lambda d: instruction(d, 12).
     __setitem__("uses", ["i.3", "i.3"]))
emit("bad-push-value", lambda d: instruction(d, 12)["uses"].
     __setitem__(1, "i.1"))
emit("bad-push-use-missing", lambda d: instruction(d, 12).
     __setitem__("uses", ["xs.1"]))
emit("bad-push-use-duplicate", lambda d: instruction(d, 12).
     __setitem__("uses", ["xs.1", "i.3", "i.3"]))
emit("bad-push-leaf", lambda d: instruction(d, 12)["expr0_graph"]["nodes"][1].
     __setitem__("text", "j"))
emit("bad-push-edge", lambda d: instruction(d, 12)["expr0_graph"]["nodes"][2].
     __setitem__("right", 0))
emit("bad-push-route", lambda d: instruction(d, 12).
     __setitem__("arg0", "ArraySet"))
emit("bad-bound-zero", lambda d: instruction(d, 1)["expr0_graph"]["nodes"][1].
     __setitem__("text", "0"))
emit("bad-bound-overflow", lambda d: instruction(d, 1)["expr0_graph"]["nodes"][1].
     __setitem__("text", "46341"))


def swap_producer_edges(doc):
    owner = block(doc, 1)
    owner["succ_true"], owner["succ_false"] = \
        owner["succ_false"], owner["succ_true"]


emit("bad-producer-edges", swap_producer_edges)
emit("bad-producer-backedge", lambda d: block(d, 2).
     __setitem__("succ_true", 3))
emit("bad-step", lambda d: instruction(d, 7)["expr0_graph"]["nodes"][1].
     __setitem__("text", "2"))


def duplicate_push(doc):
    body = block(doc, 2)["instructions"]
    duplicate = copy.deepcopy(instruction(doc, 12))
    duplicate["id"] = 120
    body.insert(1, duplicate)


emit("bad-duplicate-push", duplicate_push)
emit("bad-length-target", lambda d: instruction(d, 4)["expr0_graph"]["nodes"][2].
     __setitem__("call_target_name", "ArrayCapacity"))
emit("bad-length-use", lambda d: instruction(d, 4)["uses"].
     __setitem__(1, "i.3"))
emit("bad-read-collection", lambda d: instruction(d, 10)["uses"].
     __setitem__(1, "i.3"))
emit("bad-read-index", lambda d: instruction(d, 10)["uses"].
     __setitem__(2, "j.1"))
emit("bad-read-edge", lambda d: instruction(d, 10)["expr0_graph"]["nodes"][3].
     __setitem__("right", 1))
emit("bad-final-length-target", lambda d: instruction(d, 14)["expr0_graph"]["nodes"][3].
     __setitem__("call_target_name", "ArrayCapacity"))
emit("bad-final-length-use", lambda d: instruction(d, 14).
     __setitem__("uses", ["i.3"]))
emit("bad-source-type", lambda d: d["routines"][0]["source_locals"][0].
     __setitem__("type", "Array<String>"))
emit("bad-duplicate-local", lambda d: d["routines"][0]["source_locals"].
     append({"name": "xs", "type": "Array<Int>"}))
emit("bad-array-layout", lambda d: instruction(d, 5)["abi_layout"]["fields"][1].
     __setitem__("offset", 16))
