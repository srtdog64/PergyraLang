#!/usr/bin/env python3
"""Falsify value-result ArrayPop receiver and carriage ownership."""

import json
import sys


def routine(document, name):
    return next(row for row in document["routines"] if row["name"] == name)


def parameter_pop(document, name):
    row = routine(document, name)
    instruction = next(
        item
        for block in row["blocks"]
        for item in block["instructions"]
        if item.get("arg0") == "ArrayPop"
    )
    return row, instruction


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as source:
        document = json.load(source)
    mode = sys.argv[2]
    string_target = mode.startswith("string-")
    owner_name = "PopLastString" if string_target else "PopSecond"
    ordinal = 0 if string_target else 1
    row, pop = parameter_pop(document, owner_name)
    if mode.endswith("missing-receiver"):
        pop["local_ref"] = None
    elif mode.endswith("wrong-owner"):
        parts = pop["local_ref"].split(":")
        parts[1] = str(int(parts[1]) + 1)
        pop["local_ref"] = ":".join(parts)
    elif mode.endswith("wrong-ordinal"):
        parts = pop["local_ref"].split(":")
        parts[2] = str(len(row["params"]))
        pop["local_ref"] = ":".join(parts)
    elif mode.endswith("value-carriage"):
        row["params"][ordinal]["carriage"] = "value"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
