#!/usr/bin/env python3
import json
import sys


def routine(document, name):
    return next(row for row in document["routines"] if row["name"] == name)


def graph_nodes(row):
    for block in row["blocks"]:
        for instruction in block["instructions"]:
            graph = instruction.get("expr0_graph")
            if graph:
                yield instruction, graph["nodes"]


def literal_graph(document):
    main_row = routine(document, "Main")
    return next(
        nodes for instruction, nodes in graph_nodes(main_row)
        if instruction.get("expr0") == "[1, 0]"
    )


def parameter_literal(document):
    row = routine(document, "ParameterLiteral")
    for block in row["blocks"]:
        for instruction in block["instructions"]:
            if instruction.get("expr0") == "[selected]":
                return row, instruction, instruction["expr0_graph"]["nodes"]
    raise RuntimeError("ParameterLiteral populated literal is missing")


def local_literal(document):
    row = routine(document, "LocalLiteral")
    for block in row["blocks"]:
        for instruction in block["instructions"]:
            if instruction.get("expr0") == "[root_id]":
                return instruction
    raise RuntimeError("LocalLiteral populated literal is missing")


def computed_literal(document):
    main_row = routine(document, "Main")
    return next(
        nodes for instruction, nodes in graph_nodes(main_row)
        if instruction.get("expr0") ==
        "[(0 - 1), (5 - 2), (9 - 4), (2 - 8)]"
    )


def nested_literal_calls(document):
    row = routine(document, "BoxedTags")
    _, nodes = next(
        (instruction, graph) for instruction, graph in graph_nodes(row)
        if "[FirstTag(), 4, 5]" in instruction.get("expr0", "")
    )
    return row, nodes, [
        node for node in nodes
        if node.get("kind") == "call" and
        node.get("call_target_kind") == "direct" and
        node.get("call_target_name") in ("FirstTag", "SecondTag")
    ]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        document = json.load(handle)
    mode = sys.argv[2]
    tags = routine(document, "Tags")
    if mode == "missing-target":
        _, nodes = next(graph_nodes(tags))
        call = next(node for node in nodes if node["kind"] == "call")
        call["call_target_syntax_id"] = 0
    elif mode == "wrong-target-type":
        wrong_id = routine(document, "WrongTag")["source_syntax_id"]
        _, nodes = next(graph_nodes(tags))
        call = next(node for node in nodes if node["kind"] == "call")
        call["call_target_syntax_id"] = wrong_id
    elif mode == "wrong-literal-type":
        nodes = literal_graph(document)
        literal = next(node for node in nodes if node["kind"] == "integer_literal")
        literal["kind"] = "bool_literal"
        literal["text"] = "true"
    elif mode == "broken-literal-spine":
        nodes = literal_graph(document)
        nodes[-1]["left"] = 0
    elif mode == "wrong-expression-element-type":
        nodes = computed_literal(document)
        nodes[1]["kind"] = "string_literal"
        nodes[1]["text"] = '"bad"'
    elif mode == "wrong-expression-element-kind":
        nodes = computed_literal(document)
        subtract = next(node for node in nodes if node["kind"] == "subtract")
        subtract["kind"] = "less"
    elif mode == "malformed-expression-element-edge":
        nodes = computed_literal(document)
        subtract = next(node for node in nodes if node["kind"] == "subtract")
        subtract["right"] = 0
    elif mode == "nested-missing-target":
        _, _, calls = nested_literal_calls(document)
        calls[0]["call_target_syntax_id"] = 0
    elif mode == "nested-nonzero-parameter":
        _, _, calls = nested_literal_calls(document)
        calls[0]["call_target_syntax_id"] = \
            routine(document, "ParameterLiteral")["source_syntax_id"]
    elif mode == "nested-wrong-return":
        _, _, calls = nested_literal_calls(document)
        calls[0]["call_target_syntax_id"] = \
            routine(document, "WrongTag")["source_syntax_id"]
    elif mode == "nested-broken-spine":
        _, nodes, _ = nested_literal_calls(document)
        element = next(node for node in nodes if node["kind"] == "array_element")
        element["left"] = element["right"]
    elif mode == "nested-multi-int-spine":
        _, nodes, _ = nested_literal_calls(document)
        element = next(
            node for node in nodes
            if node["kind"] == "array_element" and
            node["text"] == "[FirstTag(), 4]"
        )
        element["left"] = element["right"]
    elif mode == "nested-local-missing-use":
        row = routine(document, "BoxedTags")
        instruction = next(
            inst for block in row["blocks"] for inst in block["instructions"]
            if inst.get("expr0") and "[local]" in inst["expr0"]
        )
        instruction["uses"] = []
    elif mode == "wrong-parameter-owner":
        row, instruction, _ = parameter_literal(document)
        instruction["expr0_local_refs"][0]["ref"] = (
            f"parameter:{row['source_syntax_id'] + 1}:1"
        )
    elif mode == "wrong-parameter-ordinal":
        _, _, nodes = parameter_literal(document)
        leaf = next(node for node in nodes if node["text"] == "selected")
        leaf["binding_ordinal"] = 0
    elif mode == "wrong-parameter-type":
        row, instruction, nodes = parameter_literal(document)
        leaf = next(node for node in nodes if node["text"] == "selected")
        leaf["text"] = "wrong"
        leaf["binding_ordinal"] = 2
        instruction["expr0_local_refs"][0]["ref"] = (
            f"parameter:{row['source_syntax_id']}:2"
        )
    elif mode == "local-missing-use":
        local_literal(document)["uses"] = []
    elif mode == "array-int-abi":
        changed = False
        for block in tags["blocks"]:
            for instruction in block["instructions"]:
                if instruction.get("abi_type_name") == "Array<Int>":
                    instruction["abi_layout"]["fields"][0]["offset"] = 8
                    changed = True
        if not changed:
            raise SystemExit("Tags has no Array<Int> ABI row")
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as handle:
        json.dump(document, handle, separators=(",", ":"))
        handle.write("\n")


if __name__ == "__main__":
    main()
