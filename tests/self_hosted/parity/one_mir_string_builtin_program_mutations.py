#!/usr/bin/env python3
"""Falsifiers for the typed nested String-builtin program route."""

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


def graphs_with_target(program: dict, target: str) -> list[dict]:
    rows = []
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if graph and any(
            node.get("call_target_name") == target for node in graph["nodes"]
        ):
            rows.append(graph)
    return rows


def literal(program: dict, spelling: str, replacement: str) -> None:
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if not graph:
            continue
        for node in graph["nodes"]:
            if node["kind"] in {"integer_literal", "string_literal"} and node["text"] == spelling:
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
                if node["kind"] in {"call", "call_argument", "add"}:
                    node["text"] = "display-only"
    write(output, "display-only", display)

    semantic = clone(program)
    literal(semantic, '"foo"', '"zap"')
    write(output, "semantic-change", semantic)

    builtin_semantic = clone(program)
    literal(builtin_semantic, "7", "8")
    literal(builtin_semantic, '"ab"', '"az"')
    literal(builtin_semantic, '"CD"', '"XY"')
    write(output, "builtin-semantic-change", builtin_semantic)

    bad = clone(program)
    graph_with_target(bad, "ToString")["nodes"][2]["call_target_syntax_id"] = 9
    write(output, "bad-to-string-syntax", bad)

    bad = clone(program)
    graph_with_target(bad, "ToString")["nodes"][2]["left"] = 0
    write(output, "bad-call-marker-edge", bad)

    bad = clone(program)
    graph_with_target(bad, "ToString")["nodes"][4]["right"] = 99
    write(output, "bad-call-argument-edge", bad)

    bad = clone(program)
    graph_with_target(bad, "ToString")["nodes"][4]["right"] = 0
    write(output, "bad-to-string-argument-type", bad)

    bad = clone(program)
    string_to_string = graphs_with_target(bad, "ToString")[1]
    string_to_string["nodes"][2]["kind"] = "boolean_literal"
    string_to_string["nodes"][2]["text"] = "true"
    write(output, "bad-to-string-string-argument-type", bad)

    bad = clone(program)
    upper = graph_with_target(bad, "ToUpper")
    upper["nodes"][2]["kind"] = "integer_literal"
    upper["nodes"][2]["text"] = "1"
    write(output, "bad-to-upper-argument-type", bad)

    bad = clone(program)
    unknown = graph_with_target(bad, "ToUpper")
    unknown["nodes"][0]["text"] = "UnknownBuiltin"
    unknown["nodes"][1]["call_target_name"] = "UnknownBuiltin"
    write(output, "bad-unregistered-builtin", bad)

    bad = clone(program)
    duplicate = graph_with_target(bad, "ToUpper")
    duplicate["nodes"][7]["left"] = 1
    write(output, "bad-duplicate-call-consumption", bad)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
