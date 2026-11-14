import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, "r", encoding="utf-8") as handle:
    document = json.load(handle)

instruction = next(
    instruction
    for routine in document["routines"]
    if routine["name"] == "LogNestedStringLiteral"
    for block in routine["blocks"]
    for instruction in block["instructions"]
    if instruction.get("expr0_graph") and instruction.get("expr0", "").startswith("[Concat(")
)
graph = instruction["expr0_graph"]
root = graph["nodes"][graph["root"]]
mixed_instruction = next(
    candidate
    for routine in document["routines"]
    if routine["name"] == "LogMixedStringLiteral"
    for block in routine["blocks"]
    for candidate in block["instructions"]
    if candidate.get("expr0_graph")
    and candidate.get("expr0", "").startswith('["Program:\\n", prefix, FormatMixedStringLiteral(')
)
mixed_graph = mixed_instruction["expr0_graph"]
mixed_root = mixed_graph["nodes"][mixed_graph["root"]]

if mode == "nested-missing-use":
    instruction["uses"].pop()
elif mode == "nested-wrong-use":
    instruction["uses"][0] = "not_the_local.1"
elif mode == "nested-wrong-element-kind":
    root["kind"] = "integer_literal"
elif mode == "nested-wrong-seed-parent":
    root["left"] = 1
elif mode == "nested-wrong-root":
    graph["root"] = root["right"]
elif mode == "nested-wrong-parameter-ref":
    formal = next(
        node
        for node in graph["nodes"]
        if node.get("binding_kind") == "formal_parameter"
    )
    formal["binding_ordinal"] = 1
elif mode == "mixed-missing-use":
    mixed_instruction["uses"].pop()
elif mode == "mixed-wrong-use":
    mixed_instruction["uses"][0] = "not_the_mixed_local.1"
elif mode == "mixed-wrong-spine":
    mixed_root["left"] = mixed_root["right"]
elif mode == "mixed-wrong-element-kind":
    element = mixed_graph["nodes"][mixed_root["right"]]
    element["kind"] = "integer_literal"
    element["text"] = "1"
    element["left"] = None
    element["right"] = None
elif mode == "mixed-owned-parameter":
    routine = next(row for row in document["routines"] if row["name"] == "LogMixedStringLiteral")
    parameter = next(row for row in routine["params"] if row["name"] == "prefix")
    parameter["carriage"] = "owner-handle"
else:
    raise SystemExit(f"unknown mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, separators=(",", ":"))
    handle.write("\n")
