import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "LogSingleton"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 1:
        raise SystemExit("fixture has no exact LogSingleton signature")
    instruction = next(
        (row for block in routine.get("blocks", [])
         for row in block.get("instructions", [])
         if row.get("kind") == "def" and row.get("expr0") == "[text]"),
        None,
    )
    if instruction is None:
        raise SystemExit("fixture has no exact formal Array<String> literal")
    graph = instruction.get("expr0_graph", {})
    nodes = graph.get("nodes", [])
    if len(nodes) != 3 or nodes[1].get("binding_kind") != "formal_parameter":
        raise SystemExit("fixture expression identity drifted")
    formal = nodes[1]
    spine = nodes[2]
    parameter = routine["params"][0]
    if kind == "formal-missing-binding":
        formal["binding_kind"] = "none"
        formal["binding_ordinal"] = None
    elif kind == "formal-wrong-ordinal":
        formal["binding_ordinal"] = 1
    elif kind == "formal-wrong-name":
        formal["text"] = "other"
    elif kind == "formal-wrong-type":
        parameter["type"] = "Int"
        parameter["abi_type_name"] = "Int"
    elif kind == "formal-wrong-carriage":
        parameter["carriage"] = "readonly-ref"
        parameter["pass"] = "indirect"
    elif kind == "formal-wrong-element-kind":
        formal["kind"] = "integer_literal"
    elif kind == "formal-reordered-spine":
        spine["left"], spine["right"] = spine["right"], spine["left"]
    elif kind == "formal-wrong-root":
        graph["root"] = 1
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
