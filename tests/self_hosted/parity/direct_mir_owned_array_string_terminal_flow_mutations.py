#!/usr/bin/env python3
"""Mutate the reached consuming-return fixture, preserving real owner imports."""
import copy
import json
import pathlib
import sys


def routine(document, name):
    rows = [row for row in document["routines"] if row["name"] == name]
    if len(rows) != 1:
        raise SystemExit(f"expected one routine: {name}")
    return rows[0]


def returned(document, name):
    rows = [instruction for block in routine(document, name)["blocks"]
            for instruction in block["instructions"] if instruction["kind"] == "return"]
    if len(rows) != 1:
        raise SystemExit(f"expected one return: {name}")
    return rows[0]


def main():
    source, mode, output = sys.argv[1:]
    document = json.loads(pathlib.Path(source).read_text(encoding="utf-8"))
    instruction = returned(document, "JoinAtReturn")
    if mode == "missing-return-graph":
        del instruction["expr0_graph"]
    elif mode == "orphan-return-call":
        graph = instruction["expr0_graph"]
        template = next(node for row in document["routines"] for block in row["blocks"]
                        for item in block["instructions"]
                        for node in (item.get("expr0_graph") or {}).get("nodes", [])
                        if node["kind"] == "string_literal")
        literal = copy.deepcopy(template)
        literal["text"] = '"orphaned"'
        graph["nodes"].append(literal)
        graph["root"] = len(graph["nodes"]) - 1
        instruction["expr0"] = '"orphaned"'
    elif mode == "duplicate-return-call-operand":
        graph = returned(document, "JoinNested")["expr0_graph"]
        outer = graph["nodes"][graph["root"]]
        first = graph["nodes"][outer["left"]]
        if outer["kind"] != "call_argument" or first["kind"] != "call_argument":
            raise SystemExit("nested Concat argument chain drifted")
        outer["right"] = first["right"]
    elif mode == "missing-caller-identity":
        routine(document, "JoinAtReturn")["source_syntax_id"] = 0
    elif mode == "missing-owned-parameter-abi":
        params = routine(document, "CodegenJoinOwnedStringFragments")["params"]
        if params[0]["type"] != "Array<String>" or params[0]["carriage"] != "owner-handle":
            raise SystemExit("real join parameter identity drifted")
        del params[0]["abi_layout"]
    elif mode == "missing-constructor-operand":
        graph = returned(document, "ConsumingRecord")["expr0_graph"]
        root = graph["nodes"][graph["root"]]
        if root["kind"] != "call_argument" or root["right"] is None:
            raise SystemExit("constructor operand shape drifted")
        root["right"] = None
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    pathlib.Path(output).write_text(json.dumps(document, separators=(",", ":")) + "\n",
                                    encoding="utf-8")


if __name__ == "__main__":
    main()
