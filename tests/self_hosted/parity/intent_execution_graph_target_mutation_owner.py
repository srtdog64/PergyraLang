#!/usr/bin/env python3
"""Valid-digest mutations for exact typed-intent graph target identity."""

import copy
import json
from pathlib import Path
import sys

MODULUS = 268435456


def hash_string(value, text):
    payload = text.encode("utf-8")
    value = (value * 131 + len(payload)) % MODULUS
    for byte in payload:
        value = (value * 131 + byte) % MODULUS
    return value


def hash_int(value, number):
    return (value * 131 + number + 2) % MODULUS


def graph_digest(graph):
    value = hash_int(71, graph["root"])
    value = hash_int(value, len(graph["nodes"]))
    for node in graph["nodes"]:
        for key in ("kind", "text", "call_target_kind", "call_target_name"):
            value = hash_string(value, node[key])
        value = hash_int(value, -1 if node["left"] is None else node["left"])
        value = hash_int(value, -1 if node["right"] is None else node["right"])
    return 1073741824 + value


def instruction(routine, block_id, instruction_id):
    matches = [
        row for block in routine["blocks"] if block["id"] == block_id
        for row in block["instructions"] if row["id"] == instruction_id
    ]
    assert len(matches) == 1, (block_id, instruction_id, len(matches))
    return matches[0]


def action_graphs(document):
    step = document["intent_execution"]["steps"][0]
    routines = document["routines"]
    intent = next(
        row for row in routines
        if row["source_syntax_id"] == step["routine_syntax_id"]
    )
    action = next(
        row for row in routines
        if row["source_syntax_id"] == step["action_syntax_id"]
    )
    collisions = [
        row for row in routines if row["kind"] == "method"
        and row["name"] == action["name"] and row["owner"] != action["owner"]
    ]
    assert collisions, "fixture lost the same-local-name foreign-owner case"
    outcome = instruction(
        intent, step["outcome_instruction_block_id"],
        step["outcome_instruction_id"],
    )
    branch = instruction(
        intent, step["branch_block_id"], step["branch_instruction_id"]
    )
    graphs = [outcome["expr0_graph"], branch["expr0_graph"]]
    assert all(graph_digest(graph) == graph["digest"] for graph in graphs)
    return action, collisions[0], graphs


def rewrite_targets(document, target_name):
    action, _, graphs = action_graphs(document)
    for graph in graphs:
        calls = [
            node for node in graph["nodes"] if node["kind"] == "call"
            and node["call_target_kind"] == "member"
            and node["call_target_name"] == action["name"]
        ]
        assert len(calls) == 1, (action["name"], len(calls))
        calls[0]["call_target_name"] = target_name
        graph["digest"] = graph_digest(graph)


assert len(sys.argv) == 3, sys.argv
base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
output = Path(sys.argv[2])
_, foreign, _ = action_graphs(base)
for name, target in (
    ("foreign-owner", f"{foreign['owner']}_{foreign['name']}"),
    ("projection-missing", ""),
):
    document = copy.deepcopy(base)
    rewrite_targets(document, target)
    (output / f"negative-action-target-{name}.mir.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )
