import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
routine_name = "GreaterLong"
if mode.startswith("wrong-equality-"):
    routine_name = "EqualLong"
elif mode.startswith("wrong-inequality-"):
    routine_name = "NotEqualLong"
elif mode.startswith("wrong-less-"):
    routine_name = "LessLong"
routine = next(row for row in document["routines"]
               if row["name"] == routine_name)
instruction = routine["blocks"][0]["instructions"][0]
nodes = instruction["expr0_graph"]["nodes"]
root = nodes[instruction["expr0_graph"]["root"]]

if mode in ("wrong-right-type", "wrong-equality-right-type",
            "wrong-inequality-right-type", "wrong-less-right-type"):
    routine["params"][1]["type"] = "Int"
    routine["params"][1]["abi_type_name"] = "Int"
elif mode == "wrong-less-left-type":
    routine["params"][0]["type"] = "Int"
    routine["params"][0]["abi_type_name"] = "Int"
elif mode == "wrong-expression-kind":
    root["kind"] = "logical_and"
elif mode == "wrong-equality-expression-kind":
    root["kind"] = "logical_and"
elif mode == "wrong-inequality-expression-kind":
    root["kind"] = "logical_and"
elif mode == "wrong-less-expression-kind":
    root["kind"] = "logical_and"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")
