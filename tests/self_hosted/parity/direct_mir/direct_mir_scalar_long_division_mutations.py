import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
routine = next(row for row in document["routines"]
               if row["name"] == "LongDivision")
instruction = routine["blocks"][0]["instructions"][0]
nodes = instruction["expr0_graph"]["nodes"]
root = nodes[instruction["expr0_graph"]["root"]]

if mode == "wrong-right-type":
    right = routine["params"][1]
    right["type"] = "Int"
    right["abi_type_name"] = "Int"
elif mode == "wrong-expression-kind":
    root["kind"] = "logical_and"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")
