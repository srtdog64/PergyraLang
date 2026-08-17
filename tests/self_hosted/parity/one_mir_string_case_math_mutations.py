#!/usr/bin/env python3
"""Falsifiers for ordered direct calls and StringReplace/Int math GraphPlan."""

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


def routine(program: dict, name: str) -> dict:
    return next(row for row in program["routines"] if row["name"] == name)


def graph_with_target(program: dict, target: str) -> dict:
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if graph and any(
            node.get("call_target_name") == target for node in graph["nodes"]
        ):
            return graph
    raise RuntimeError(f"missing call target: {target}")


def graph_with_expr(program: dict, expr0: str) -> dict:
    for instruction in instructions(program):
        if instruction.get("expr0") == expr0:
            return instruction["expr0_graph"]
    raise RuntimeError(f"missing expression: {expr0}")


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
                if node["kind"] in {
                    "call", "call_argument", "add", "subtract", "negate"
                }:
                    node["text"] = "display-only"
    write(output, "display-only", display)

    semantic = clone(program)
    initial = graph_with_expr(semantic, '"Hello, World!"')
    initial["nodes"][0]["text"] = '"Hello, Codex!"'
    write(output, "semantic-change", semantic)

    reordered = clone(program)
    reordered["routines"].reverse()
    write(output, "routine-order", reordered)

    bad = clone(program)
    clamp = graph_with_target(bad, "Min")
    next(node for node in clamp["nodes"] if node.get("text") == "lo")[
        "binding_ordinal"
    ] = 3
    write(output, "bad-parameter-ordinal", bad)

    bad = clone(program)
    parameter = routine(bad, "ClampVal")["params"][2]
    parameter["type"] = "Bool"
    parameter["abi_type_name"] = "Bool"
    write(output, "bad-parameter-type", bad)

    bad = clone(program)
    direct = graph_with_target(bad, "ClampVal")
    direct["nodes"][9]["left"] = 5
    write(output, "bad-direct-call-chain", bad)

    bad = clone(program)
    direct = graph_with_target(bad, "ClampVal")
    next(node for node in direct["nodes"] if node.get("call_target_name") == "ClampVal")[
        "call_target_syntax_id"
    ] = 999
    write(output, "bad-direct-target-syntax", bad)

    bad = clone(program)
    routine(bad, "ClampVal")["blocks"][0]["instructions"][0][
        "abi_type_name"
    ] = "Bool"
    write(output, "bad-return-type", bad)

    bad = clone(program)
    minimum = graph_with_expr(bad, "Log(ToString(Min(3, 7)))")
    minimum["nodes"][6]["kind"] = "string_literal"
    minimum["nodes"][6]["text"] = '"7"'
    write(output, "bad-min-argument-type", bad)

    bad = clone(program)
    maximum = graph_with_expr(bad, "Log(ToString(Max(3, 7)))")
    maximum["nodes"][0]["text"] = "UnknownMath"
    maximum["nodes"][1]["call_target_name"] = "UnknownMath"
    write(output, "bad-builtin-target", bad)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
