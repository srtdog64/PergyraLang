#!/usr/bin/env python3
"""Falsify dynamic Array<String> target identity, use order, and copyout."""

import json
import sys


def routine(document):
    return next(row for row in document["routines"] if row["name"] == "FillNames")


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
    names = routine(document)["params"][0]
    if mode == "target-binding-kind":
        first["expr1_graph"]["nodes"][0]["binding_kind"] = "none"
        first["expr1_graph"]["nodes"][0]["binding_ordinal"] = None
    elif mode == "target-binding-ordinal":
        first["expr1_graph"]["nodes"][0]["binding_ordinal"] = 1
    elif mode == "target-index-edge":
        first["expr1_graph"]["nodes"][4]["right"] = 0
    elif mode == "index-use-order":
        first["uses"][0], first["uses"][1] = first["uses"][1], first["uses"][0]
    elif mode == "predecessor-use":
        second["uses"] = second["uses"][1:]
    elif mode == "result-chain":
        first["result"] = "names.9"
    elif mode == "carriage":
        names["carriage"] = "value"
    elif mode == "abi-layout":
        names["abi_layout"]["fields"][0]["offset"] = 8
    elif mode == "value-type":
        first["expr0_graph"]["nodes"][0]["text"] = "first_index"
    elif mode == "source-tag":
        first["arg1"] = "local"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
