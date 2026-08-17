#!/usr/bin/env python3
"""Falsify value-result indexed-assignment identity and SSA carriage."""

import json
import sys


def routine(document):
    return next(row for row in document["routines"] if row["name"] == "FillBounds")


def assignments(document):
    return [
        instruction
        for block in routine(document)["blocks"]
        for instruction in block["instructions"]
        if instruction.get("source_type") == "AST_ASSIGNMENT"
    ]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as source:
        document = json.load(source)
    mode = sys.argv[2]
    first, second = assignments(document)
    bounds = routine(document)["params"][1]
    if mode == "target-binding-kind":
        first["expr1_graph"]["nodes"][0]["binding_kind"] = "none"
        first["expr1_graph"]["nodes"][0]["binding_ordinal"] = None
    elif mode == "target-binding-ordinal":
        first["expr1_graph"]["nodes"][0]["binding_ordinal"] = 0
    elif mode == "target-index-edge":
        first["expr1_graph"]["nodes"][2]["right"] = 0
    elif mode == "target-index-literal":
        first["expr1_graph"]["nodes"][1]["text"] = "01"
    elif mode == "predecessor-use":
        second["uses"] = []
    elif mode == "result-chain":
        first["result"] = "bounds.7"
    elif mode == "carriage":
        bounds["carriage"] = "value"
    elif mode == "abi-layout":
        bounds["abi_layout"]["fields"][0]["offset"] = 8
    elif mode == "value-type":
        node = first["expr0_graph"]["nodes"][0]
        node["text"] = "bounds"
        node["binding_ordinal"] = 1
    elif mode == "source-tag":
        first["arg1"] = "local"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
