"""Mutate source-produced MIR, without running any invalid source program.

The plan is unchanged for CFG mutations, so its digest remains genuine and
cannot alone reject the crossed instruction identity. The production v3
cross-seal must reject the inconsistent executable rows.
"""
import copy
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
document = json.loads(path.read_text(encoding="utf-8"))
step = document["intent_execution"]["steps"][0]


def instruction_at(payload, block_id, instruction_id):
    routine = next(row for row in payload["routines"]
                   if row["source_syntax_id"] == step["routine_syntax_id"])
    block = next(row for row in routine["blocks"] if row["id"] == block_id)
    return next(row for row in block["instructions"] if row["id"] == instruction_id)


for name in ("missing-plan", "crossed-payload", "missing-completion"):
    changed = copy.deepcopy(document)
    if name == "missing-plan":
        del changed["intent_execution"]
    elif name == "crossed-payload":
        terminal = changed["intent_execution"]["terminals"][0]
        routine = next(row for row in changed["routines"]
                       if row["source_syntax_id"] == terminal["routine_syntax_id"])
        block = next(row for row in routine["blocks"]
                     if row["id"] == terminal["result_instruction_block_id"])
        instruction = next(row for row in block["instructions"]
                           if row["id"] == terminal["result_instruction_id"])
        instruction["arg0"] = "UnrelatedTerminalVariant"
    else:
        instruction = instruction_at(changed, step["completion_block_id"],
                                     step["completion_instruction_id"])
        instruction["name"] = "NotAnIntentCompletion"
    pathlib.Path(f"{path}.{name}.json").write_text(
        json.dumps(changed, separators=(",", ":")), encoding="utf-8")
