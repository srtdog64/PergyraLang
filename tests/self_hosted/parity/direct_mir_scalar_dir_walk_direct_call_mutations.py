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

if mode == "dirwalk-target-name":
    dir_walk["call_target_name"] = "DirWalkDrift"
elif mode == "dirwalk-target-syntax":
    dir_walk["call_target_syntax_id"] = 1
elif mode == "fixture-target-syntax":
    fixture_dir["call_target_syntax_id"] = 0
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, separators=(",", ":"))
    handle.write("\n")
