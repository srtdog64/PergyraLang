#!/usr/bin/env python3
"""Falsify value-result nested-record Array<Int> assignment facts."""

import json
import sys


def assignments(document):
    routine = next(row for row in document["routines"] if row["name"] == "ResolveTarget")
    return [
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("source_type") == "AST_ASSIGNMENT"
    ], routine


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], encoding="utf-8") as source:
        document = json.load(source)
    rows, routine = assignments(document)
    first, second = rows
    nodes = second["expr1_graph"]["nodes"]
    mode = sys.argv[2]
    if mode == "formal-binding":
        nodes[0]["binding_kind"] = "none"
        nodes[0]["binding_ordinal"] = None
    elif mode == "formal-ordinal":
        nodes[0]["binding_ordinal"] = 1
    elif mode == "carriage":
        routine["params"][0]["carriage"] = "value"
    elif mode == "missing-predecessor":
        second["uses"].pop(0)
    elif mode == "wrong-predecessor":
        second["uses"][0] = "analysis.99"
    elif mode == "member-identity":
        nodes[5]["text"] = "other_kinds"
        nodes[6]["text"] = "analysis.expression_graph.arena.other_kinds"
        nodes[8]["text"] = "analysis.expression_graph.arena.other_kinds[node_id]"
    elif mode == "rhs-type":
        second["expr0"] = '"wrong"'
        second["expr0_graph"]["nodes"] = [{
            "kind": "string_literal",
            "text": '"wrong"',
            "call_target_kind": "none",
            "call_target_name": "",
            "call_target_syntax_id": 0,
            "runtime_call_abi_id": 0,
            "binding_kind": "none",
            "binding_ordinal": None,
            "left": None,
            "right": None,
        }]
        second["expr0_graph"]["root"] = 0
    elif mode == "result-owner":
        second["result"] = "other.5"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
