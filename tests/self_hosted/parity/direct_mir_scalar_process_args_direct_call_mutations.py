import copy
import json
import sys


def expression_nodes(document):
    for routine in document["routines"]:
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                graph = instruction.get("expr0_graph")
                if graph:
                    yield graph["nodes"]


def mutate(document, mode):
    changed = copy.deepcopy(document)
    nodes = next(
        rows for rows in expression_nodes(changed)
        if any(row.get("call_target_name") == "Args" for row in rows)
    )
    args_call = next(
        row for row in nodes if row.get("call_target_name") == "Args"
    )
    outer_call = next(
        row for row in nodes
        if row.get("call_target_name") == "ConsumeArgs"
    )
    if mode == "args-target-name":
        args_call["call_target_name"] = "UnknownArgs"
    elif mode == "args-target-syntax":
        args_call["call_target_syntax_id"] = outer_call[
            "call_target_syntax_id"
        ]
    elif mode == "outer-target-syntax":
        outer_call["call_target_syntax_id"] = 0
    else:
        raise ValueError(mode)
    return changed


with open(sys.argv[1], encoding="utf-8") as source:
    document = json.load(source)
with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as target:
    json.dump(mutate(document, sys.argv[2]), target, separators=(",", ":"))
    target.write("\n")
