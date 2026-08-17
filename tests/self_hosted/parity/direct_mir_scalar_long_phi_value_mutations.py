import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
routine = next(row for row in document["routines"]
               if row["name"] == "SelectLong")
instructions = [instruction for block in routine["blocks"]
                for instruction in block["instructions"]]
phi = next(instruction for instruction in instructions
           if instruction["kind"] == "phi" and instruction["name"] == "value")

if mode == "wrong-incoming-type":
    definition = next(instruction for instruction in instructions
                      if instruction.get("result") == phi["uses"][0])
    definition["abi_type_name"] = "Int"
elif mode == "non-dominating-incoming":
    phi["uses"][1] = phi["uses"][0]
elif mode == "missing-incoming":
    phi["uses"].pop()
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(document, output_file, separators=(",", ":"))
    output_file.write("\n")
