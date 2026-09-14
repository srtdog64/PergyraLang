#!/usr/bin/env python3
"""Falsify the local Array<Int> indexed-assignment owner seam."""

import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], encoding="utf-8") as source:
        document = json.load(source)
    routine = next(row for row in document["routines"] if row["name"] == "ApplyEffectMask")
    assignments = [
        row
        for block in routine["blocks"]
        for row in block["instructions"]
        if row.get("source_type") == "AST_ASSIGNMENT"
    ]
    assignment, boolean = assignments
    mode = sys.argv[2]
    if mode == "missing-predecessor":
        assignment["uses"].pop(0)
    elif mode == "wrong-predecessor":
        assignment["uses"][0] = "used_effects.1"
    elif mode == "repeated-predecessor":
        assignment["uses"].insert(0, assignment["uses"][0])
    elif mode == "target-owner":
        assignment["expr1_graph"]["nodes"][0]["text"] = "used_effects"
    elif mode == "target-binding":
        node = assignment["expr1_graph"]["nodes"][0]
        node["binding_kind"] = "formal_parameter"
        node["binding_ordinal"] = 0
    elif mode == "result-owner":
        assignment["result"] = "other.6"
    elif mode == "array-type":
        assignment["abi_type_name"] = "Array<Bool>"
    elif mode == "rhs-call-target":
        call = next(row for row in assignment["expr0_graph"]["nodes"] if row["kind"] == "call")
        call["call_target_name"] = "ForeignJoin"
    elif mode == "bool-missing-predecessor":
        boolean["uses"].pop(0)
    elif mode == "bool-rhs-type":
        boolean["expr0_graph"]["nodes"][0]["kind"] = "integer_literal"
        boolean["expr0_graph"]["nodes"][0]["text"] = "1"
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
