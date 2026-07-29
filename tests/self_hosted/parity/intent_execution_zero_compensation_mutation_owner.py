#!/usr/bin/env python3
"""Negative mutation for an unmaterialized typed-intent compensation block."""

import copy
import json
from pathlib import Path
import sys


def owned_blocks(plan, routine_id):
    owned = set()
    for step in plan["steps"]:
        if step["routine_syntax_id"] != routine_id:
            continue
        for key in (
            "outcome_instruction_block_id", "branch_block_id",
            "success_successor_block_id", "failure_successor_block_id",
            "completion_block_id",
        ):
            owned.add(step[key])
        owned.update(row["instruction_block_id"] for row in step["compensations"])
    owned.update(
        row["result_instruction_block_id"] for row in plan["terminals"]
        if row["routine_syntax_id"] == routine_id
    )
    return owned


assert len(sys.argv) == 3, sys.argv
base = json.loads(Path(sys.argv[1]).read_text(encoding="utf-8"))
plan = base["intent_execution"]
zero_steps = [step for step in plan["steps"] if not step["compensations"]]
assert zero_steps, "fixture lost its zero-compensation step"
routine_id = zero_steps[0]["routine_syntax_id"]
assert all(step["routine_syntax_id"] == routine_id for step in zero_steps)
routine = next(
    row for row in base["routines"] if row["source_syntax_id"] == routine_id
)
owned = owned_blocks(plan, routine_id)
scaffolds = [
    block for block in routine["blocks"] if block["id"] > 4
    and block["id"] not in owned and block["reachable"] is False
    and not block["instructions"]
]
assert len(scaffolds) == len(zero_steps), (len(scaffolds), len(zero_steps))

document = copy.deepcopy(base)
routine = next(
    row for row in document["routines"] if row["source_syntax_id"] == routine_id
)
scaffold_id = scaffolds[0]["id"]
next(block for block in routine["blocks"] if block["id"] == scaffold_id)[
    "reachable"
] = True
Path(sys.argv[2]).joinpath(
    "negative-zero-compensation-scaffold-reachable.mir.json"
).write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
