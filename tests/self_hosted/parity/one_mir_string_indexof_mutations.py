#!/usr/bin/env python3
"""Falsifiers for StringIndexOf range and String-window GraphPlan facts."""

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


def graph_with_target(program: dict, target: str, occurrence: int = 0) -> dict:
    found = []
    for instruction in instructions(program):
        graph = instruction.get("expr0_graph")
        if graph and any(
            node.get("call_target_name") == target for node in graph["nodes"]
        ):
            found.append(graph)
    if occurrence >= len(found):
        raise RuntimeError(f"missing call target: {target}[{occurrence}]")
    return found[occurrence]


def instruction_with_result(program: dict, result: str) -> dict:
    return next(row for row in instructions(program) if row.get("result") == result)


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
                if node["kind"] in {"call", "call_argument", "add", "subtract"}:
                    node["text"] = "display-only"
    write(output, "display-only", display)

    semantic = clone(program)
    literal(semantic, '"hello,world"', '"hey,world"')
    write(output, "semantic-change", semantic)

    absent = clone(program)
    first_search = graph_with_target(absent, "StringIndexOf", 0)
    next(node for node in first_search["nodes"] if node.get("text") == '","')[
        "text"
    ] = '"z"'
    write(output, "absent-search", absent)

    empty = clone(program)
    first_search = graph_with_target(empty, "StringIndexOf", 0)
    next(node for node in first_search["nodes"] if node.get("text") == '","')[
        "text"
    ] = '""'
    write(output, "empty-needle", empty)

    bad = clone(program)
    instruction_with_result(bad, "p.1")["abi_type_name"] = "String"
    write(output, "bad-result-type", bad)

    bad = clone(program)
    first_search = graph_with_target(bad, "StringIndexOf", 0)
    first_search["nodes"][5]["left"] = 1
    write(output, "bad-argument-chain", bad)

    bad = clone(program)
    first_search = graph_with_target(bad, "StringIndexOf", 0)
    first_search["nodes"][4]["kind"] = "integer_literal"
    first_search["nodes"][4]["text"] = "1"
    write(output, "bad-argument-type", bad)

    bad = clone(program)
    first_search = graph_with_target(bad, "StringIndexOf", 0)
    first_search["nodes"][0]["text"] = "UnknownStringSearch"
    first_search["nodes"][1]["call_target_name"] = "UnknownStringSearch"
    write(output, "bad-unregistered-target", bad)

    bad = clone(program)
    first_search = graph_with_target(bad, "StringIndexOf", 0)
    first_search["nodes"][1]["call_target_syntax_id"] = 999
    write(output, "bad-target-syntax", bad)

    bad = clone(program)
    window = graph_with_target(bad, "Substring", 1)
    add = next(node for node in window["nodes"] if node["kind"] == "add")
    window["nodes"][add["right"]]["text"] = "2"
    write(output, "bad-index-step", bad)

    bad = clone(program)
    window = graph_with_target(bad, "Substring", 1)
    subtracts = [
        index for index, node in enumerate(window["nodes"])
        if node["kind"] == "subtract"
    ]
    add = next(node for node in window["nodes"] if node["kind"] == "add")
    inner = subtracts[0]
    window["nodes"][inner]["right"] = add["right"]
    write(output, "bad-length-index-relation", bad)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
