#!/usr/bin/env python3
"""Falsifiers for runtime String/Array<String> expressions in GraphPlan."""

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


def definition(program: dict, result: str) -> dict:
    for instruction in instructions(program):
        if instruction.get("result") == result:
            return instruction
    raise RuntimeError(f"missing definition: {result}")


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
                if node["kind"] in {"call", "call_argument", "add", "index"}:
                    node["text"] = "display-only"
    write(output, "display-only", display)

    semantic = clone(program)
    literal(semantic, '"wor"', '"zzz"')
    literal(semantic, '"a,bb,c"', '"a,bbbb,c"')
    literal(semantic, '"42"', '"7"')
    write(output, "semantic-change", semantic)

    bad = clone(program)
    definition(bad, "parts.1")["abi_type_name"] = "String"
    write(output, "bad-split-result-type", bad)

    bad = clone(program)
    graph_with_target(bad, "Split")["nodes"][5]["left"] = 1
    write(output, "bad-split-argument-chain", bad)

    bad = clone(program)
    contains = graph_with_target(bad, "StringContains")
    contains["nodes"][4]["kind"] = "integer_literal"
    contains["nodes"][4]["text"] = "1"
    write(output, "bad-contains-argument-type", bad)

    bad = clone(program)
    indexed = next(
        instruction["expr0_graph"]
        for instruction in instructions(bad)
        if instruction.get("expr0") == "Log(parts[1])"
    )
    indexed["nodes"][1]["kind"] = "string_literal"
    indexed["nodes"][1]["text"] = '"one"'
    write(output, "bad-array-index-type", bad)

    bad = clone(program)
    unknown = graph_with_target(bad, "StringSplit")
    unknown["nodes"][0]["text"] = "UnknownStringCollection"
    unknown["nodes"][1]["call_target_name"] = "UnknownStringCollection"
    write(output, "bad-unregistered-target", bad)

    bad = clone(program)
    graph_with_target(bad, "Split")["nodes"][1]["call_target_syntax_id"] = 9
    write(output, "bad-split-syntax-identity", bad)

    bad = clone(program)
    definition(bad, "empty.1")["abi_layout"]["fields"][1]["offset"] = 16
    write(output, "bad-array-layout", bad)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
