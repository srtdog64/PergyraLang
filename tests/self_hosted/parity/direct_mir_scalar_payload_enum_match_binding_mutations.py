import json
import sys


source, kind, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)
ready = next(row for row in document["routines"]
             if row["name"] == "PayloadOutcomeReady")
match = next(instruction for block in ready["blocks"]
             for instruction in block["instructions"]
             if instruction.get("match_variant") == "PayloadCommitted")
if kind == "binding-name":
    match["match_bindings"][0] = "missing"
elif kind == "binding-type":
    match["match_binding_types"][0] = "PayloadFailure"
elif kind == "consumer-name":
    consumer = next(instruction for block in ready["blocks"]
                    for instruction in block["instructions"]
                    if "receipt.value" in (instruction.get("expr0") or ""))
    leaf = next(node for node in consumer["expr0_graph"]["nodes"]
                if node.get("text") == "receipt")
    leaf["text"] = "missing"
else:
    raise SystemExit(f"unknown mutation: {kind}")
with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(document, stream, separators=(",", ":"))
    stream.write("\n")
