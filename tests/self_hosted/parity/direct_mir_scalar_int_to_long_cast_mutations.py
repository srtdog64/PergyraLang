import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
long_to_int = mode.startswith("long-to-int-")
routine_name = "LongToInt" if long_to_int else "IntToLong"
mutation = mode.removeprefix("long-to-int-") if long_to_int else mode
routine = next(row for row in document["routines"]
               if row["name"] == routine_name)
instruction = routine["blocks"][0]["instructions"][0]
nodes = instruction["expr0_graph"]["nodes"]
type_name = nodes[1]
cast = nodes[instruction["expr0_graph"]["root"]]

if mutation == "wrong-source-type":
    replacement = "Int" if long_to_int else "Long"
    routine["params"][0]["type"] = replacement
    routine["params"][0]["abi_type_name"] = replacement
elif mutation == "wrong-target-type":
    type_name["text"] = "Long" if long_to_int else "Int"
elif mutation == "malformed-type-name-shape":
    type_name["kind"] = "leaf"
elif mutation == "non-cast-use":
    cast["kind"] = "add"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")
