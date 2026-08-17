#!/usr/bin/env python3
"""Falsify the persisted nested-record Array<String> assignment facts."""

import json
import sys


def assignment(document):
    routine = next(row for row in document["routines"] if row["name"] == "RewriteNested")
    return next(
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("source_type") == "AST_ASSIGNMENT"
    )


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], encoding="utf-8") as source:
        document = json.load(source)
    row = assignment(document)
    nodes = row["expr1_graph"]["nodes"]
    mode = sys.argv[2]
    if mode == "root-name":
        nodes[0]["text"] = "other"
    elif mode == "member-identity":
        nodes[5]["text"] = "other_texts"
        nodes[6]["text"] = "surfaces.expression_graph.arena.other_texts"
        nodes[8]["text"] = "surfaces.expression_graph.arena.other_texts[index]"
    elif mode == "member-path":
        nodes[6]["left"] = 2
    elif mode == "index-edge":
        nodes[8]["right"] = 0
    elif mode == "use-order":
        row["uses"][0], row["uses"][1] = row["uses"][1], row["uses"][0]
    elif mode == "missing-index-use":
        row["uses"].pop()
    elif mode == "result-chain":
        row["result"] = "surfaces.99"
    elif mode == "source-tag":
        row["arg1"] = "inout_param"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
