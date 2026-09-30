import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, "r", encoding="utf-8") as handle:
    document = json.load(handle)

graph = document["routines"][-1]["blocks"][0]["instructions"][0]["expr0_graph"]
nodes = graph["nodes"]
dir_walk = next(node for node in nodes if node.get("call_target_name") == "DirWalk")
fixture_dir = next(
    node for node in nodes
    if node.get("call_target_name") == "DirectMirDirWalkFixtureDir"
)
write_file = next(
    node
    for routine in document["routines"]
    for block in routine["blocks"]
    for instruction in block["instructions"]
    for node in (instruction.get("expr0_graph") or {}).get("nodes", [])
    if node.get("call_target_name") == "WriteFile"
)

if mode == "dirwalk-target-name":
    dir_walk["call_target_name"] = "DirWalkDrift"
elif mode == "writefile-target-name":
    write_file["call_target_name"] = "WriteFileDrift"
elif mode == "dirwalk-target-syntax":
    dir_walk["call_target_syntax_id"] = 1
elif mode == "fixture-target-syntax":
    fixture_dir["call_target_syntax_id"] = 0
elif mode == "array-layout-offset":
    array_result = next(
        instruction
        for routine in document["routines"]
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("abi_type_name") == "Array<String>"
        and instruction.get("abi_layout_required") is True
    )
    array_result["abi_layout"]["fields"][3]["offset"] = 16
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, separators=(",", ":"))
    handle.write("\n")
