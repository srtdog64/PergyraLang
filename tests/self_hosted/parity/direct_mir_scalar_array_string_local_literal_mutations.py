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
         if row.get("name") == "LogLocalSingleton"),
        None,
    )
    if routine is None:
        raise SystemExit("fixture has no LogLocalSingleton routine")
    instructions = [
        row for block in routine.get("blocks", [])
        for row in block.get("instructions", [])
    ]
    source_def = next(
        (row for row in instructions if row.get("result") == "source_text.1"),
        None,
    )
    literal_def = next(
        (row for row in instructions if row.get("expr0") == "[source_text]"),
        None,
    )
    if source_def is None or literal_def is None:
        raise SystemExit("fixture local literal identity drifted")
    graph = literal_def.get("expr0_graph", {})
    nodes = graph.get("nodes", [])
    if len(nodes) != 3 or literal_def.get("uses") != ["source_text.1"]:
        raise SystemExit("fixture local literal use drifted")
    local = nodes[1]
    spine = nodes[2]
    if kind == "local-missing-use":
        literal_def["uses"] = []
    elif kind == "local-wrong-use":
        literal_def["uses"] = ["missing.1"]
    elif kind == "local-extra-use":
        literal_def["uses"].append("source_text.1")
    elif kind == "local-wrong-type":
        source_def["abi_type_name"] = "Int"
        next(row for row in routine["source_locals"]
             if row.get("name") == "source_text")["type"] = "Int"
    elif kind == "local-forged-formal":
        local["binding_kind"] = "formal_parameter"
        local["binding_ordinal"] = 0
    elif kind == "local-wrong-element-kind":
        local["kind"] = "integer_literal"
    elif kind == "local-reordered-spine":
        spine["left"], spine["right"] = spine["right"], spine["left"]
    elif kind == "local-wrong-root":
        graph["root"] = 1
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
