import copy
import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
mutated = copy.deepcopy(document)
routine = next(row for row in mutated["routines"] if row["name"] == "Scale")
instruction = routine["blocks"][0]["instructions"][0]
nodes = instruction["expr0_graph"]["nodes"]
root = nodes[instruction["expr0_graph"]["root"]]

if mode == "missing-right-edge":
    root["right"] = None
elif mode == "wrong-right-type":
    right = nodes[root["right"]]
    right["kind"] = "string_literal"
    right["text"] = '"4"'
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(mutated, output_file, separators=(",", ":"))
    output_file.write("\n")
