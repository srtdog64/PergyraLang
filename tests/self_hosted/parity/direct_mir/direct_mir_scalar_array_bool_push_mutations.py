#!/usr/bin/env python3
"""Falsify the exact local Array<Bool> push/set targets and values."""

import copy
import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)

def instructions(name):
    row = next(item for item in document["routines"] if item["name"] == name)
    return [inst for block in row["blocks"] for inst in block["instructions"]]

push = next(inst for inst in instructions("Main")
            if inst.get("arg0") == "ArrayPush" and
            inst.get("expr0") ==
            "ArrayPush(reachable_values, reachable_fact.reachable)")
string_push = next(inst for inst in instructions("Main")
                   if inst.get("arg0") == "ArrayPush")
set_value = next(
    inst for inst in instructions("Main")
    if inst.get("arg0") == "ArraySet"
    and inst.get("expr0_graph", {}).get("nodes")
    and inst["expr0_graph"]["nodes"][inst["expr0_graph"]["root"]].get("kind")
        == "bool_literal"
    and inst["expr0_graph"]["nodes"][inst["expr0_graph"]["root"]].get("text")
        == "false"
)
if mode == "missing-bool-push-receiver":
    push["local_ref"] = None
elif mode == "wrong-bool-push-receiver-type":
    push["local_ref"] = string_push["local_ref"]
elif mode == "wrong-bool-push-value-type":
    push["expr0"] = string_push["expr0"]
    push["expr0_graph"] = copy.deepcopy(string_push["expr0_graph"])
elif mode == "missing-bool-set-receiver":
    set_value["local_ref"] = None
elif mode == "wrong-bool-set-receiver-type":
    set_value["local_ref"] = string_push["local_ref"]
elif mode == "wrong-bool-set-value-type":
    set_value["expr0"] = string_push["expr0"]
    set_value["expr0_graph"] = copy.deepcopy(string_push["expr0_graph"])
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")
