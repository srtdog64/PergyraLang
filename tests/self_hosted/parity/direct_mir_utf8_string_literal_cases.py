#!/usr/bin/env python3
"""Byte-exact docs/110 expectations and typed-literal regression inputs."""

import copy
import json
from pathlib import Path
import sys


def expected(changed=False):
    rows = [
        "", "plain", 'quote:" slash:\\ tab:\t CR:\r end', "line1\nline2",
        "\\n", "é", "e\u0301", "€", "한글🚀" if changed else "한글🙂",
        "家/مرحبا", "🙂", "2", "3", "10", "false", "é", "한글🙂",
    ]
    return ("\n".join(rows) + "\n").encode("utf-8")


def literal_graph(program):
    for routine in program["routines"]:
        if routine["name"] != "Main":
            continue
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                graph = instruction.get("expr0_graph")
                if not graph or not any(n.get("call_target_name") == "Echo"
                                        for n in graph["nodes"]):
                    continue
                matches = [i for i, node in enumerate(graph["nodes"])
                           if node["kind"] == "string_literal"
                           and node["text"] == '"한글🙂"']
                if len(matches) == 1:
                    return instruction, graph, matches[0]
    raise ValueError("missing typed Echo UTF-8 literal")


def prepare(source, output):
    program = json.loads(source.read_text(encoding="utf-8"))
    literal_graph(program)

    def write(name, value):
        (output / f"{name}.json").write_text(
            json.dumps(value, ensure_ascii=False, separators=(",", ":")),
            encoding="utf-8")

    display = copy.deepcopy(program)
    instruction, graph, _ = literal_graph(display)
    instruction["expr0"] = "display-only"
    for node in graph["nodes"]:
        if node["kind"] in {"call", "call_argument"}:
            node["text"] = "display-only"
    write("display-only", display)

    cases = [
        ("semantic-change", "text", '"한글🚀"'),
        ("missing-quote", "text", '"한글🙂'),
        ("unsupported-escape", "text", '"한글\\q"'),
        ("unescaped-quote", "text", '"한"글🙂"'),
        ("raw-tab", "text", '"한\t글🙂"'),
        ("raw-del", "text", '"한\x7f글🙂"'),
        ("wrong-literal-kind", "kind", "integer_literal"),
        ("unexpected-binding", "binding_syntax_id", 999999),
    ]
    for name, field, value in cases:
        changed = copy.deepcopy(program)
        _, graph, index = literal_graph(changed)
        graph["nodes"][index][field] = value
        write(name, changed)


def main():
    if sys.argv[1] == "prepare":
        prepare(Path(sys.argv[2]), Path(sys.argv[3]))
    elif sys.argv[1] == "compare":
        actual = Path(sys.argv[2]).read_bytes().replace(b"\r\n", b"\n")
        wanted = expected(sys.argv[3] == "semantic-change")
        if actual != wanted:
            raise SystemExit(f"UTF-8 byte output differs\nexpected={wanted!r}\nactual={actual!r}")
    else:
        raise SystemExit("expected prepare or compare")


if __name__ == "__main__":
    main()
