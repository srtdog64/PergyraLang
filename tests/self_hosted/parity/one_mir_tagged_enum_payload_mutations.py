import json
import sys


source, kind, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)

declaration = document["decls"][0]
instructions = [
    instruction
    for routine in document["routines"]
    for block in routine["blocks"]
    for instruction in block["instructions"]
]

if kind == "variant-name":
    declaration["variants"][0]["name"] = "MissingVariant"
elif kind == "payload-count":
    declaration["variants"][0]["param_count"] = 2
elif kind == "payload-type":
    declaration["variants"][0]["param_types"][0] = "Long"
elif kind == "payload-ordinal":
    target = next(row for row in instructions
                  if row.get("expr0") == "Log(r.Rect._1)")
    leaf = next(node for node in target["expr0_graph"]["nodes"]
                if node.get("text") == "_1")
    leaf["text"] = "_2"
elif kind == "member-valid-variant":
    target = next(row for row in instructions
                  if row.get("expr0") == "Log(c.Circle._0)")
    leaf = next(node for node in target["expr0_graph"]["nodes"]
                if node.get("text") == "Circle")
    leaf["text"] = "Rect"
elif kind == "future-ssa-use":
    target = next(row for row in instructions
                  if row.get("expr0") == "Log(c.Circle._0)")
    target["expr0_graph"]["nodes"][0]["text"] = "x"
    target["uses"][0] = "x.1"
elif kind == "stale-ssa-use":
    target = instructions[-1]
    target["uses"][0] = "c.1"
elif kind == "graph-edge":
    target = next(row for row in instructions
                  if row.get("expr0") == "Log(c.Circle._0)")
    root = target["expr0_graph"]["root"]
    target["expr0_graph"]["nodes"][root]["left"] = 0
elif kind == "ssa-use":
    target = next(row for row in instructions
                  if row.get("expr0") == "Log(c.Circle._0)")
    target["uses"][0] = "r.1"
else:
    raise SystemExit(f"unknown mutation: {kind}")

with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(document, stream, separators=(",", ":"))
    stream.write("\n")
