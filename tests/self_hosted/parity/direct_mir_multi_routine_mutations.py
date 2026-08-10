#!/usr/bin/env python3
"""Create exact callable-identity negatives for the scalar multi-routine gate."""

import json
import sys


def direct_call_nodes(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for lane in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(lane)
                    if not isinstance(graph, dict):
                        continue
                    for node in graph.get("nodes", []):
                        if node.get("call_target_kind") == "direct":
                            yield node


def option_int_instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == "Option<Int>":
                    yield instruction


def nominal_instructions(document):
    declarations = document.get("decls", document.get("declarations", []))
    candidates = [
        declaration
        for declaration in declarations
        if declaration.get("kind") == "struct"
        and [field.get("type") for field in declaration.get("fields", [])]
        == ["Int", "Int"]
    ]
    if len(candidates) != 1:
        return
    nominal_name = candidates[0].get("name")
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == nominal_name:
                    yield instruction


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    input_path, kind, output_path = sys.argv[1:]
    with open(input_path, encoding="utf-8") as source:
        document = json.load(source)

    if kind == "missing-call-target":
        node = next(direct_call_nodes(document), None)
        if node is None:
            raise SystemExit("fixture has no direct call target")
        node["call_target_syntax_id"] = 999999
    elif kind == "duplicate-routine-identity":
        routines = document.get("routines", [])
        if len(routines) < 2:
            raise SystemExit("fixture has fewer than two routines")
        routines[1]["source_syntax_id"] = routines[0]["source_syntax_id"]
    elif kind == "option-abi-layout":
        instruction = next(option_int_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no Option<Int> ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field Option<Int> ABI")
        fields[1]["offset"] = 0
    elif kind == "two-int-nominal-abi-layout":
        instruction = next(nominal_instructions(document), None)
        if instruction is None:
            raise SystemExit("fixture has no nominal ABI instruction")
        fields = instruction.get("abi_layout", {}).get("fields", [])
        if len(fields) != 2:
            raise SystemExit("fixture has no two-field nominal ABI")
        fields[1]["offset"] = 0
    else:
        raise SystemExit(f"unknown mutation: {kind}")

    with open(output_path, "w", encoding="utf-8", newline="\n") as output:
        json.dump(document, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
