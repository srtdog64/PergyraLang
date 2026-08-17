import copy
import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source_file:
    document = json.load(source_file)
mutated = copy.deepcopy(document)
routine = next(row for row in mutated["routines"] if row["name"] == "Scale")
instruction = routine["blocks"][0]["instructions"][0]

if mode == "statement-stage":
    instruction["kind"] = "stmt"
    instruction["name"] = "stmt"
elif mode == "instruction-kind-stage":
    instruction["kind"] = "invalid-kind"
elif mode == "leaf-operand-stage":
    leaf = instruction["expr0_graph"]["nodes"][0]
    leaf["binding_kind"] = "none"
    leaf["binding_ordinal"] = None
elif mode == "expression-kind-stage":
    literal = instruction["expr0_graph"]["nodes"][1]
    literal["kind"] = "bool_literal"
    literal["text"] = "true"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output_file:
    json.dump(mutated, output_file, separators=(",", ":"))
    output_file.write("\n")
