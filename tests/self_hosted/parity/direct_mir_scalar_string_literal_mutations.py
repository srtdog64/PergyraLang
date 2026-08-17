import json
import sys

source, output = sys.argv[1:3]
with open(source, "r", encoding="utf-8") as handle:
    document = json.load(handle)

changed = False
for routine in document.get("routines", []):
    for block in routine.get("blocks", []):
        for instruction in block.get("instructions", []):
            graph = instruction.get("expr0_graph")
            if not isinstance(graph, dict):
                continue
            for node in graph.get("nodes", []):
                if node.get("kind") == "string_literal" and "\\n" in node.get("text", ""):
                    node["text"] = '"bad\\q"'
                    instruction["expr0"] = '"bad\\q"'
                    changed = True
                    break
            if changed:
                break
        if changed:
            break
    if changed:
        break
if not changed:
    raise SystemExit("escaped string literal fixture is missing")
with open(output, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write("\n")
