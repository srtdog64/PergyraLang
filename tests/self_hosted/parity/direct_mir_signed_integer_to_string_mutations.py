#!/usr/bin/env python3
"""Typed conversion-edge regression inputs; no source-text recovery."""

import copy
import json
from pathlib import Path
import sys


def describe(program):
    routines = [r for r in program["routines"] if r["name"] == "Describe"]
    if len(routines) != 1:
        raise ValueError("expected exactly one Describe routine")
    instructions = [i for b in routines[0]["blocks"] for i in b["instructions"]]
    returns = [i for i in instructions if i["kind"] == "return"]
    if len(returns) != 1:
        raise ValueError("expected exactly one Describe return")
    return returns[0]


def main():
    program = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
    output = Path(sys.argv[2])
    original = describe(program)["expr0_graph"]
    nodes = original["nodes"]
    calls = [i for i, n in enumerate(nodes)
             if n["kind"] == "call" and n["call_target_name"] == "ToString"]
    if len(calls) != 1:
        raise ValueError("expected exactly one typed ToString call")
    call = calls[0]
    argument = original["root"]
    if nodes[argument]["kind"] != "call_argument" or nodes[argument]["left"] != call:
        raise ValueError("unexpected ToString argument topology")
    operand = nodes[argument]["right"]
    if nodes[operand]["binding_kind"] != "formal_parameter":
        raise ValueError("expected a typed formal parameter operand")

    def write(name, change):
        changed = copy.deepcopy(program)
        change(describe(changed))
        (output / f"{name}.json").write_text(
            json.dumps(changed, separators=(",", ":")), encoding="utf-8")

    def display(instruction):
        instruction["expr0"] = "display-only"
        for node in instruction["expr0_graph"]["nodes"]:
            if node["kind"] in {"call", "call_argument"}:
                node["text"] = "display-only"

    write("display-only", display)
    for name, index, field, value in [
        ("missing-operand", argument, "right", None),
        ("wrong-operand", argument, "right", call),
        ("missing-binding", operand, "binding_syntax_id", 0),
        ("wrong-builtin-runtime", call, "runtime_call_abi_id", 1),
        ("wrong-builtin-target", call, "call_target_syntax_id", 999999),
    ]:
        write(name, lambda instruction, i=index, f=field, v=value:
              instruction["expr0_graph"]["nodes"][i].__setitem__(f, v))
    write("wrong-result-type", lambda instruction:
          instruction.__setitem__("abi_type_name", "Long"))


if __name__ == "__main__":
    main()
