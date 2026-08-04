#!/usr/bin/env python3
"""Typed positive and negative MIR variants for Array<String> call carriage."""

import copy
import json
import pathlib
import sys


source_path = pathlib.Path(sys.argv[1])
output_dir = pathlib.Path(sys.argv[2])
base = json.loads(source_path.read_text(encoding="utf-8"))


def write(name: str, document: dict) -> None:
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def routines(document: dict) -> tuple[dict, dict]:
    pick = next(row for row in document["routines"] if row["name"] == "Pick")
    main = next(row for row in document["routines"] if row["name"] == "Main")
    return pick, main


def return_nodes(document: dict) -> list[dict]:
    pick, _ = routines(document)
    return pick["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"]


def literal_nodes(document: dict) -> list[dict]:
    _, main = routines(document)
    return main["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"]


def call_nodes(document: dict) -> list[dict]:
    _, main = routines(document)
    return main["blocks"][0]["instructions"][1]["expr0_graph"]["nodes"]


write("program", base)

display = copy.deepcopy(base)
pick, main = routines(display)
pick["blocks"][0]["instructions"][0]["expr0"] = "display-only-return"
main["blocks"][0]["instructions"][0]["expr0"] = "display-only-literal"
main["blocks"][0]["instructions"][1]["expr0"] = "display-only-call"
write("display-only", display)

routine_order = copy.deepcopy(base)
routine_order["routines"].reverse()
write("routine-order", routine_order)

semantic = copy.deepcopy(base)
values = ["red", "green", "blue"]
nodes = literal_nodes(semantic)
for node, value in zip((nodes[1], nodes[3], nodes[5]), values):
    node["text"] = json.dumps(value)
return_nodes(semantic)[1]["text"] = "2"
pick, main = routines(semantic)
pick["blocks"][0]["instructions"][0]["expr0"] = "xs[2]"
main["blocks"][0]["instructions"][0]["expr0"] = '["red", "green", "blue"]'
write("semantic", semantic)

bad_param_type = copy.deepcopy(base)
routines(bad_param_type)[0]["params"][0]["type"] = "Array<Int>"
write("bad-param-type", bad_param_type)

bad_carriage = copy.deepcopy(base)
routines(bad_carriage)[0]["params"][0]["carriage"] = "ref"
write("bad-param-carriage", bad_carriage)

bad_pass = copy.deepcopy(base)
routines(bad_pass)[0]["params"][0]["pass"] = "indirect"
write("bad-param-pass", bad_pass)

bad_required = copy.deepcopy(base)
routines(bad_required)[0]["params"][0]["abi_layout_required"] = False
write("bad-param-abi-required", bad_required)

bad_param_layout = copy.deepcopy(base)
routines(bad_param_layout)[0]["params"][0]["abi_layout_id"] += 1
write("bad-param-layout", bad_param_layout)

bad_local_layout = copy.deepcopy(base)
routines(bad_local_layout)[1]["blocks"][0]["instructions"][0]["abi_layout_id"] += 1
write("bad-local-layout", bad_local_layout)

bad_target = copy.deepcopy(base)
next(node for node in call_nodes(bad_target) if node["kind"] == "call")[
    "call_target_syntax_id"
] += 1
write("bad-call-target", bad_target)

bad_argument = copy.deepcopy(base)
next(node for node in call_nodes(bad_argument) if node["kind"] == "call_argument")[
    "right"
] = 1
write("bad-call-argument", bad_argument)

bad_return = copy.deepcopy(base)
pick, _ = routines(bad_return)
pick["return"] = "Int"
pick["blocks"][0]["instructions"][0]["abi_type_name"] = "Int"
write("bad-return-type", bad_return)

bad_negative = copy.deepcopy(base)
return_nodes(bad_negative)[1]["text"] = "-1"
write("bad-index-negative", bad_negative)

bad_upper = copy.deepcopy(base)
return_nodes(bad_upper)[1]["text"] = "3"
write("bad-index-upper", bad_upper)

bad_topology = copy.deepcopy(base)
root = return_nodes(bad_topology)[2]
root["left"], root["right"] = root["right"], root["left"]
write("bad-index-topology", bad_topology)

bad_literal = copy.deepcopy(base)
literal_nodes(bad_literal)[-1]["kind"] = "add"
write("bad-literal-spine", bad_literal)
