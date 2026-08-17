#!/usr/bin/env python3
"""Mutate the normalized scalar-array index operand without changing its text."""

import json
import sys


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: mutation.py INPUT OUTPUT")
    input_path, output_path = sys.argv[1:]
    with open(input_path, encoding="utf-8") as source:
        document = json.load(source)
    changed = False
    for routine in document.get("routines", []):
        if routine.get("name") != "ScalarArrayIntAtZero":
            continue
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                graph = instruction.get("expr0_graph")
                if not isinstance(graph, dict):
                    continue
                nodes = graph.get("nodes", [])
                for node in nodes:
                    if node.get("kind") == "index" and node.get("right") is not None:
                        index_node = nodes[node["right"]]
                        index_node["kind"] = "bool_literal"
                        index_node["text"] = "true"
                        changed = True
                        break
    if not changed:
        raise SystemExit("fixture has no ScalarArrayIntAtZero index node")
    with open(output_path, "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
