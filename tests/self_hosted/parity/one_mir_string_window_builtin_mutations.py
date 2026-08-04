#!/usr/bin/env python3
"""Falsifiers for registry-owned String length/window calls in GraphPlan."""

import copy
import json
import pathlib
import sys


def write(root: pathlib.Path, name: str, value: object) -> None:
    (root / f"{name}.json").write_text(
        json.dumps(value, separators=(",", ":")), encoding="utf-8"
    )


def instructions(program: dict):
    for routine in program["routines"]:
        for block in routine["blocks"]:
            yield from block["instructions"]


def graph_with_target(program: dict, target: str) -> dict:
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if graph and any(
            node.get("call_target_name") == target for node in graph["nodes"]
        ):
            return graph
    raise RuntimeError(f"missing call target: {target}")


def literal(program: dict, spelling: str, replacement: str) -> None:
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if not graph:
            continue
        for node in graph["nodes"]:
            if node["kind"] == "string_literal" and node["text"] == spelling:
                node["text"] = replacement
                return
    raise RuntimeError(f"missing literal: {spelling}")


def clone(program: dict) -> dict:
    return copy.deepcopy(program)


def main() -> int:
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[2])
    program = json.loads(source.read_text(encoding="utf-8"))
    write(output, "program", program)

    display = clone(program)
    for instruction in instructions(display):
        if instruction.get("expr0"):
            instruction["expr0"] = "display-only"
        graph = instruction.get("expr0_graph")
        if graph:
            for node in graph["nodes"]:
                if node["kind"] in {"call", "call_argument", "subtract"}:
                    node["text"] = "display-only"
    write(output, "display-only", display)

    semantic = clone(program)
    literal(semantic, '"pergyra"', '"pergyralang"')
    write(output, "semantic-change", semantic)

    bad = clone(program)
    for instruction in instructions(bad):
        if instruction.get("result") == "n.1":
            instruction["abi_type_name"] = "String"
            break
    else:
        raise RuntimeError("missing n.1 definition")
    write(output, "bad-result-type", bad)

    bad = clone(program)
    bounded = graph_with_target(bad, "SubstringWithLen")
    bounded["nodes"][9]["right"] = 99
    write(output, "bad-final-argument-edge", bad)

    bad = clone(program)
    bounded = graph_with_target(bad, "SubstringWithLen")
    bounded["nodes"][7]["left"] = 1
    write(output, "bad-argument-chain-edge", bad)

    bad = clone(program)
    bounded = graph_with_target(bad, "SubstringWithLen")
    bounded["nodes"][6]["kind"] = "string_literal"
    bounded["nodes"][6]["text"] = '"zero"'
    write(output, "bad-argument-type", bad)

    bad = clone(program)
    unknown = graph_with_target(bad, "Substring")
    unknown["nodes"][0]["text"] = "UnknownStringWindow"
    unknown["nodes"][1]["call_target_name"] = "UnknownStringWindow"
    write(output, "bad-unregistered-target", bad)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
