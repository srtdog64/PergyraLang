#!/usr/bin/env python3
"""Falsify typed array-mutation receiver, expression, and ABI ownership."""

import copy
import json
import sys


def routine(document, name):
    return next(row for row in document["routines"] if row["name"] == name)


def instructions(row):
    for block in row["blocks"]:
        yield from block["instructions"]


def array_sets(document):
    return [
        instruction
        for instruction in instructions(routine(document, "Main"))
        if instruction.get("arg0") == "ArraySet"
    ]


def array_pops(document):
    return [
        instruction
        for instruction in instructions(routine(document, "Main"))
        if instruction.get("arg0") == "ArrayPop"
    ]


def parameter_set(document):
    return next(
        instruction
        for instruction in instructions(routine(document, "IncrementFirst"))
        if instruction.get("arg0") == "ArraySet"
    )
def parameter_push(document):
    return next(
        instruction for instruction in instructions(routine(document, "AppendSecond"))
        if instruction.get("arg0") == "ArrayPush"
    )


def parameter_string_set(document):
    return next(
        instruction for instruction in instructions(routine(document, "ReplaceFirst"))
        if instruction.get("arg0") == "ArraySet"
    )


def local_definition(document, name):
    return next(
        instruction
        for instruction in instructions(routine(document, "Main"))
        if instruction.get("kind") == "def" and instruction.get("arg0") == name
    )


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as source:
        document = json.load(source)
    mode = sys.argv[2]
    int_set, string_set = array_sets(document)[:2]
    int_pop, _ = array_pops(document)
    if mode == "missing-receiver":
        int_set["local_ref"] = None
    elif mode == "wrong-receiver-type":
        int_set["local_ref"] = string_set["local_ref"]
    elif mode == "wrong-index-type":
        int_set["expr1"] = string_set["expr0"]
        int_set["expr1_graph"] = copy.deepcopy(string_set["expr0_graph"])
    elif mode == "wrong-value-type":
        int_set["expr0"] = string_set["expr0"]
        int_set["expr0_graph"] = copy.deepcopy(string_set["expr0_graph"])
    elif mode == "missing-index-graph":
        int_set["expr1_graph"] = None
    elif mode == "broken-index-spine":
        int_set["expr1_graph"]["root"] = len(int_set["expr1_graph"]["nodes"])
    elif mode == "duplicate-consumed-use":
        int_set["expr1"] = "ints[0]"
        int_set["expr1_graph"] = {
            "root": 2,
            "nodes": [
                {"kind": "leaf", "text": "ints"},
                {"kind": "integer_literal", "text": "0"},
                {"kind": "index", "text": "ints[0]", "left": 0, "right": 1},
            ],
        }
        int_set["uses"] = [int_set["uses"][0], int_set["uses"][0]]
    elif mode == "missing-pop-receiver":
        int_pop["local_ref"] = None
    elif mode == "pop-expression-graph":
        int_pop["expr0_graph"] = copy.deepcopy(int_set["expr0_graph"])
    elif mode == "array-int-abi":
        local_definition(document, "ints")["abi_layout"]["fields"][0]["offset"] = 8
    elif mode == "array-string-abi":
        local_definition(document, "strings")["abi_layout"]["fields"][0]["offset"] = 8
    elif mode == "missing-parameter-receiver":
        parameter_set(document)["local_ref"] = None
    elif mode == "missing-parameter-push-receiver":
        parameter_push(document)["local_ref"] = None
    elif mode in ("wrong-parameter-owner", "wrong-parameter-ordinal",
                  "wrong-parameter-push-owner", "wrong-parameter-push-ordinal"):
        row = parameter_push(document) if "push" in mode else parameter_set(document)
        parts = row["local_ref"].split(":")
        parts[1 if mode.endswith("owner") else 2] = str(
            int(parts[1]) + 1) if mode.endswith("owner") else "2"
        row["local_ref"] = ":".join(parts)
    elif mode == "wrong-parameter-push-value-type":
        row = parameter_push(document)
        row["expr0"] = string_set["expr0"]
        row["expr0_graph"] = copy.deepcopy(string_set["expr0_graph"])
    elif mode == "missing-string-parameter-receiver":
        parameter_string_set(document)["local_ref"] = None
    elif mode in ("wrong-string-parameter-owner", "wrong-string-parameter-ordinal"):
        row = parameter_string_set(document)
        parts = row["local_ref"].split(":")
        parts[1 if mode.endswith("owner") else 2] = str(
            int(parts[1]) + 1) if mode.endswith("owner") else "1"
        row["local_ref"] = ":".join(parts)
    elif mode == "wrong-string-parameter-value-type":
        row = parameter_string_set(document)
        row["expr0"] = int_set["expr0"]
        row["expr0_graph"] = copy.deepcopy(int_set["expr0_graph"])
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
