import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, "r", encoding="utf-8") as handle:
    document = json.load(handle)

graph = next(
    instruction["expr0_graph"]
    for block in document["routines"][-1]["blocks"]
    for instruction in block["instructions"]
    if instruction.get("expr0_graph") and "ReadFile" in instruction.get("expr0", "")
)
read_file = next(
    node for node in graph["nodes"] if node.get("call_target_name") == "ReadFile"
)
path = next(
    node for node in graph["nodes"]
    if node.get("call_target_name") == "DirectMirReadFileFixturePath"
)

if mode == "read-file-target-name":
    read_file["call_target_name"] = "ReadFileDrift"
elif mode == "read-file-target-syntax":
    read_file["call_target_syntax_id"] = 1
elif mode == "path-target-syntax":
    path["call_target_syntax_id"] = 0
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, separators=(",", ":"))
    handle.write("\n")
