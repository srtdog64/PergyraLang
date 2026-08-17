import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, "r", encoding="utf-8") as handle:
    document = json.load(handle)

graph = next(
    instruction["expr0_graph"]
    for block in document["routines"][-1]["blocks"]
    for instruction in block["instructions"]
    if instruction.get("expr0_graph") and "FileExists" in instruction.get("expr0", "")
)
exists = next(node for node in graph["nodes"] if node.get("call_target_name") == "FileExists")
path = next(
    node for node in graph["nodes"]
    if node.get("call_target_name") == "DirectMirFileExistsFixturePath"
)

if mode == "file-exists-target-name":
    exists["call_target_name"] = "FileExistsDrift"
elif mode == "file-exists-target-syntax":
    exists["call_target_syntax_id"] = 1
elif mode == "path-target-syntax":
    path["call_target_syntax_id"] = 0
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, separators=(",", ":"))
    handle.write("\n")
