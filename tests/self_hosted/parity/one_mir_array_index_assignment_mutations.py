#!/usr/bin/env python3
"""Positive rewrites and falsifiers for collection-operation legalization."""

import copy
import json
import sys
from pathlib import Path

source = Path(sys.argv[1])
out = Path(sys.argv[2])
base = json.loads(source.read_text(encoding="utf-8"))


def rows(doc):
    return [row for block in doc["routines"][0]["blocks"]
            for row in block["instructions"]]


def typed(doc, abi, source_type):
    return next(row for row in rows(doc)
                if row.get("abi_type_name") == abi
                and row.get("source_type") == source_type)


def log_for(doc, receiver):
    return next(row for row in rows(doc)
                if row.get("kind") == "stmt"
                and row.get("arg0") == "Log"
                and row.get("uses") == [receiver])


def emit(name, change):
    doc = copy.deepcopy(base)
    change(doc)
    (out / f"{name}.json").write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def replace_value(doc, old, new):
    for row in rows(doc):
        if row.get("result") == old:
            row["result"] = new
        row["uses"] = [new if value == old else value
                       for value in row.get("uses", [])]


def display_only(doc):
    for row in rows(doc):
        row["expr0"] = "display-only"
        if row.get("expr1"):
            row["expr1"] = "display-only-target"


def renumber(doc):
    for old, new in (("nums.1", "nums.71"), ("nums.2", "nums.72"),
                     ("words.1", "words.81"), ("words.5", "words.82")):
        replace_value(doc, old, new)


def set_int_source(doc, values):
    nodes = typed(doc, "Array<Int>", "AST_LET_DECL")["expr0_graph"]["nodes"]
    for index, value in zip((1, 3, 5), values):
        nodes[index]["text"] = str(value)


def set_string_source(doc, values):
    nodes = typed(doc, "Array<String>", "AST_LET_DECL")["expr0_graph"]["nodes"]
    for index, value in zip((1, 3), values):
        nodes[index]["text"] = json.dumps(value)


def alternate(doc):
    set_int_source(doc, (4, 5, 6))
    typed(doc, "Array<Int>", "AST_ASSIGNMENT")["expr0_graph"]["nodes"][0]["text"] = "8"
    set_string_source(doc, ("x", "n"))
    typed(doc, "Array<String>", "AST_ASSIGNMENT")["expr0_graph"]["nodes"][0]["text"] = '"q"'


def alternate_indices(doc):
    typed(doc, "Array<Int>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][1]["text"] = "0"
    typed(doc, "Array<String>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][1]["text"] = "1"


def overflow_sum(doc):
    set_int_source(doc, (2147483647, 0, 0))
    typed(doc, "Array<Int>", "AST_ASSIGNMENT")["expr0_graph"]["nodes"][0]["text"] = "1"


def overwritten_only(doc):
    set_int_source(doc, (1, 777, 3))
    set_string_source(doc, ("discarded", "b"))


def reorder_groups(doc):
    block = doc["routines"][0]["blocks"][0]
    int_rows = [row for row in block["instructions"]
                if row.get("abi_type_name") == "Array<Int>"
                or row.get("uses") == ["nums.2"]]
    string_rows = [row for row in block["instructions"]
                   if row.get("abi_type_name") == "Array<String>"
                   or row.get("uses") == ["words.5"]]
    block["instructions"] = string_rows + int_rows


def remove(doc, predicate):
    for block in doc["routines"][0]["blocks"]:
        block["instructions"] = [row for row in block["instructions"]
                                 if not predicate(row)]


def log_before_set(doc, abi, result):
    block = doc["routines"][0]["blocks"][0]
    assignment = typed(doc, abi, "AST_ASSIGNMENT")
    log = log_for(doc, result)
    block["instructions"].remove(log)
    block["instructions"].insert(block["instructions"].index(assignment), log)


def duplicate_assignment(doc):
    block = doc["routines"][0]["blocks"][0]
    original = typed(doc, "Array<Int>", "AST_ASSIGNMENT")
    duplicate = copy.deepcopy(original)
    duplicate["id"] = max(row["id"] for row in rows(doc)) + 1
    duplicate["result"] = "nums.99"
    block["instructions"].insert(block["instructions"].index(original) + 1,
                                 duplicate)


def swap_layout(doc):
    assignment = typed(doc, "Array<String>", "AST_ASSIGNMENT")
    fields = assignment["abi_layout"]["fields"]
    length = next(row for row in fields if row["name"] == "length")
    capacity = next(row for row in fields if row["name"] == "capacity")
    length["offset"], capacity["offset"] = capacity["offset"], length["offset"]


def add_unreachable_block(doc):
    doc["routines"][0]["blocks"].append({
        "id": 1,
        "reachable": False,
        "instructions": [],
    })


emit("display-only", display_only)
emit("renumbered", renumber)
emit("alternate", alternate)
emit("alternate-indices", alternate_indices)
emit("overflow-sum", overflow_sum)
emit("overwritten-only", overwritten_only)
emit("groups-reordered", reorder_groups)

emit("bad-int-cross-receiver", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["uses"].__setitem__(0, "words.1"))
emit("bad-string-cross-receiver", lambda d: typed(
    d, "Array<String>", "AST_ASSIGNMENT")["uses"].__setitem__(0, "nums.1"))
emit("bad-int-target-name", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][0].
     __setitem__("text", "words"))
emit("bad-stale-int-observation", lambda d: log_for(
    d, "nums.2")["uses"].__setitem__(0, "nums.1"))
emit("bad-stale-string-observation", lambda d: log_for(
    d, "words.5")["uses"].__setitem__(0, "words.1"))
emit("bad-result-collision", lambda d: typed(
    d, "Array<String>", "AST_ASSIGNMENT").__setitem__("result", "nums.2"))
emit("bad-spurious-use", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["uses"].append("nums.1"))
emit("bad-int-index-negative", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][1].
     __setitem__("text", "-1"))
emit("bad-int-index-oob", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][1].
     __setitem__("text", "3"))
emit("bad-string-index-oob", lambda d: typed(
    d, "Array<String>", "AST_ASSIGNMENT")["expr1_graph"]["nodes"][1].
     __setitem__("text", "2"))
emit("bad-int-log-index-oob", lambda d: log_for(
    d, "nums.2")["expr0_graph"]["nodes"][10].__setitem__("text", "3"))
emit("bad-int-log-root", lambda d: log_for(
    d, "nums.2")["expr0_graph"].__setitem__("root", 12))
emit("bad-string-log-root", lambda d: log_for(
    d, "words.5")["expr0_graph"].__setitem__("root", 8))
emit("bad-int-log-before-set", lambda d: log_before_set(
    d, "Array<Int>", "nums.2"))
emit("bad-missing-int-set", lambda d: remove(
    d, lambda row: row is typed(d, "Array<Int>", "AST_ASSIGNMENT")))
emit("bad-missing-string-log", lambda d: remove(
    d, lambda row: row is log_for(d, "words.5")))
emit("bad-extra-assignment", duplicate_assignment)
emit("bad-string-layout", swap_layout)
emit("bad-int-value-type", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT")["expr0_graph"]["nodes"][0].
     __setitem__("kind", "string_literal"))
emit("bad-multiple-blocks", add_unreachable_block)
emit("bad-target-graph-missing", lambda d: typed(
    d, "Array<Int>", "AST_ASSIGNMENT").__setitem__("expr1_graph", None))
