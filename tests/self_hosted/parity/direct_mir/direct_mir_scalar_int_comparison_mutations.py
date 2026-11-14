import json
import sys

source, mode, output = sys.argv[1:4]
with open(source, "r", encoding="utf-8") as handle:
    document = json.load(handle)

changed = False
for routine in document.get("routines", []):
    target = "ComparisonCode"
    if mode == "bool-mixed-type":
        target = "BoolComparisonCode"
    elif mode == "out-of-range-literal":
        target = "Main"
    elif mode != "mixed-type":
        raise SystemExit("unknown mutation mode")
    if routine.get("name") != target:
        continue
    for block in routine.get("blocks", []):
        for instruction in block.get("instructions", []):
            graph = instruction.get("expr0_graph")
            if not isinstance(graph, dict):
                continue
            nodes = graph.get("nodes", [])
            for node in nodes:
                if mode == "out-of-range-literal" and \
                        node.get("kind") == "integer_literal" and \
                        node.get("text") == "2147483647":
                    node["text"] = "2147483648"
                    changed = True
                    break
                if node.get("kind") != "inequality":
                    continue
                right = node.get("right")
                if not isinstance(right, int) or right < 0 or right >= len(nodes):
                    raise SystemExit("inequality right operand is missing")
                literal_kind = "string_literal"
                literal_text = '"bad"'
                if mode == "bool-mixed-type":
                    literal_kind = "integer_literal"
                    literal_text = "1"
                nodes[right] = {
                    "kind": literal_kind, "text": literal_text,
                    "call_target_kind": "none", "call_target_name": "",
                    "call_target_syntax_id": 0, "binding_kind": "none",
                    "binding_ordinal": None, "left": None, "right": None,
                }
                changed = True
                break
            if changed:
                break
        if changed:
            break
if not changed:
    raise SystemExit("Int comparison mutation target is missing")
with open(output, "w", encoding="utf-8", newline="\n") as handle:
    json.dump(document, handle, ensure_ascii=False, separators=(",", ":"))
    handle.write("\n")
