"""Routine-partition and typed-expression falsifiers for String programs."""

import copy
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
output_dir = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def routine(document, name):
    matches = [item for item in document["routines"] if item["name"] == name]
    if len(matches) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return matches[0]


def instruction(document, routine_name, instruction_id):
    matches = [
        item
        for block in routine(document, routine_name)["blocks"]
        for item in block["instructions"]
        if item["id"] == instruction_id
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected {routine_name} instruction {instruction_id}"
        )
    return matches[0]


def display_only(document):
    for item in document["routines"]:
        for block in item["blocks"]:
            for row in block["instructions"]:
                if row.get("expr0") is not None:
                    row["expr0"] = "display text is not authority"
                if row.get("expr1") is not None:
                    row["expr1"] = "display text is not authority"


def reverse_routines(document):
    document["routines"] = list(reversed(document["routines"]))


def bad_call_target(document):
    instruction(document, "Main", 2)["expr0_graph"]["nodes"][1][
        "call_target_syntax_id"
    ] = 999999


def bad_parameter_identity(document):
    node = instruction(document, "Kind", 0)["expr0_graph"]["nodes"][0]
    node["binding_kind"] = "none"
    node["binding_ordinal"] = None


def bad_string_comparison_kind(document):
    instruction(document, "Kind", 0)["expr0_graph"]["nodes"][2][
        "kind"
    ] = "add"


def bad_return_type(document):
    instruction(document, "Kind", 2)["abi_type_name"] = "Bool"


def bad_callable_edge(document):
    routine(document, "Kind")["blocks"][4]["succ_true"] = 99


def missing_terminal_return(document):
    routine(document, "Kind")["blocks"][5]["instructions"] = []


emit("display-only", display_only)
emit("routine-order", reverse_routines)
emit("bad-call-target", bad_call_target)
emit("bad-parameter-identity", bad_parameter_identity)
emit("bad-string-comparison-kind", bad_string_comparison_kind)
emit("bad-return-type", bad_return_type)
emit("bad-callable-edge", bad_callable_edge)
emit("missing-terminal-return", missing_terminal_return)
