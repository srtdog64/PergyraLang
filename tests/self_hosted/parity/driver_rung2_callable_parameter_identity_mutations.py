"""Falsify callable-parameter and declared-callable MIR identity ownership."""

import json
import sys


def routine(document, name):
    matches = [row for row in document.get("routines", []) if row.get("name") == name]
    if len(matches) != 1:
        raise SystemExit(f"fixture must contain exactly one {name} routine")
    return matches[0]


def graphs(owner):
    for block in owner.get("blocks", []):
        for instruction in block.get("instructions", []):
            for key in ("expr0_graph", "expr1_graph"):
                graph = instruction.get(key)
                if graph is not None:
                    yield graph


def first_node(owner, predicate, description):
    matches = [node for graph in graphs(owner) for node in graph.get("nodes", [])
               if predicate(node)]
    if not matches:
        raise SystemExit(f"fixture has no {description}")
    return matches[0]


def parameter(owner, name):
    matches = [row for row in owner.get("params", []) if row.get("name") == name]
    if len(matches) != 1:
        raise SystemExit(f"fixture must contain exactly one {owner.get('name')}.{name} parameter")
    return matches[0]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)

    apply_two = routine(document, "ApplyTwo")
    main_routine = routine(document, "Main")
    plus_three = routine(document, "Plus3")
    param_a = parameter(apply_two, "a")
    param_b = parameter(apply_two, "b")
    leaf_b = first_node(
        apply_two,
        lambda node: node.get("kind") == "leaf" and node.get("text") == "b",
        "ApplyTwo b leaf",
    )
    call_b = first_node(
        apply_two,
        lambda node: node.get("kind") == "call" and node.get("call_target_name") == "b",
        "ApplyTwo b call",
    )
    plus_three_value = first_node(
        main_routine,
        lambda node: node.get("kind") == "leaf"
        and node.get("text") == "Plus3"
        and node.get("binding_kind") == "declared_callable",
        "Main Plus3 callable value",
    )
    apply_two_call = first_node(
        main_routine,
        lambda node: node.get("kind") == "call"
        and node.get("call_target_name") == "ApplyTwo",
        "Main ApplyTwo direct call",
    )

    if kind == "formal-missing-binding-id":
        leaf_b["binding_syntax_id"] = 0
    elif kind == "formal-target-zero":
        call_b["call_target_syntax_id"] = 0
    elif kind == "formal-target-crosswire":
        call_b["call_target_syntax_id"] = param_a["source_syntax_id"]
    elif kind == "formal-binding-crosswire":
        leaf_b["binding_syntax_id"] = param_a["source_syntax_id"]
    elif kind == "formal-binding-ordinal":
        leaf_b["binding_ordinal"] = 0
    elif kind == "formal-forged-target":
        call_b["call_target_syntax_id"] = 999999
    elif kind == "formal-target-declared-routine":
        call_b["call_target_syntax_id"] = plus_three["source_syntax_id"]
    elif kind == "formal-name-mismatch":
        call_b["call_target_name"] = "a"
    elif kind == "formal-param-source-missing":
        param_b["source_syntax_id"] = 0
    elif kind == "formal-type-missing":
        param_b.pop("type")
    elif kind == "formal-carriage-missing":
        param_b.pop("carriage")
    elif kind == "formal-carriage-forged":
        param_b["carriage"] = "inout"
    elif kind == "formal-malformed-signature":
        param_b["type"] = "func(Int)"
        param_b["abi_type_name"] = "func(Int)"
    elif kind == "formal-unsupported-shape":
        param_b["type"] = "func(String) -> Int"
        param_b["abi_type_name"] = "func(String) -> Int"
    elif kind == "declared-missing-binding-id":
        plus_three_value["binding_syntax_id"] = 0
    elif kind == "declared-binding-id-swap":
        plus_three_value["binding_syntax_id"] = routine(document, "Times2")["source_syntax_id"]
    elif kind == "declared-target-id-swap":
        apply_two_call["call_target_syntax_id"] = plus_three["source_syntax_id"]
    elif kind == "declared-binding-kind":
        plus_three_value["binding_kind"] = "formal_parameter"
        plus_three_value["binding_ordinal"] = 0
    elif kind == "declared-binding-ordinal":
        plus_three_value["binding_ordinal"] = 0
    elif kind == "declared-name-mismatch":
        plus_three_value["text"] = "Times2"
    else:
        raise SystemExit(f"unknown mutation: {kind}")

    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
