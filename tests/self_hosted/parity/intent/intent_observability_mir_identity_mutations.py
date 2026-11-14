#!/usr/bin/env python3
"""Verify or mutate the carried intent-observability ABI identity."""

import json
import sys


def call_nodes(document):
    for routine in document["routines"]:
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                for graph_name in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(graph_name)
                    if graph is None:
                        continue
                    for node in graph["nodes"]:
                        if node.get("call_target_kind") == "direct":
                            yield node


def unique_call(document, name):
    matches = [
        node for node in call_nodes(document)
        if node.get("call_target_name") == name
    ]
    if len(matches) != 1:
        raise SystemExit(f"expected one direct call for {name}, found {len(matches)}")
    return matches[0]


def verify(document):
    expected = {
        "IntentHistoryCount": 25,
        "IntentActiveConcurrent": 1,
        "IntentActiveStepName": 13,
    }
    for name, runtime_id in expected.items():
        node = unique_call(document, name)
        if node.get("runtime_call_abi_id") != runtime_id:
            raise SystemExit(f"{name} carried {node.get('runtime_call_abi_id')}, expected {runtime_id}")
        if node.get("call_target_syntax_id", 0) != 0:
            raise SystemExit(f"{name} mixed source and runtime identities")


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit("usage: mutations.py INPUT verify | INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as stream:
        document = json.load(stream)
    mode = sys.argv[2]
    verify(document)
    if mode == "verify":
        if len(sys.argv) != 3:
            raise SystemExit("verify mode does not write an output")
        print("intent-observability-mir-identity=ready")
        return
    if len(sys.argv) != 4:
        raise SystemExit("mutation mode requires an output")
    if mode == "missing-zero":
        unique_call(document, "IntentHistoryCount")["runtime_call_abi_id"] = 0
    elif mode == "valid-crosswire":
        unique_call(document, "IntentHistoryCount")["runtime_call_abi_id"] = 2
    elif mode == "forged-non-observability":
        unique_call(document, "ToString")["runtime_call_abi_id"] = 25
    elif mode == "syntax-conflict":
        unique_call(document, "IntentHistoryCount")["call_target_syntax_id"] = 713
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
