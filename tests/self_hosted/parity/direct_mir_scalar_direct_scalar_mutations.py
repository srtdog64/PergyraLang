import json
import copy
import sys


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT [ROUTINE]")
    source, kind, output = sys.argv[1:4]
    routine_name = sys.argv[4] if len(sys.argv) == 5 else "EchoAt"
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == routine_name), None)
    if routine is None:
        raise SystemExit(f"fixture has no {routine_name} routine")
    if kind.startswith("print-"):
        main = next(row for row in document["routines"]
                    if row["name"] == "Main")
        instruction = next(
            row for block in main["blocks"]
            for row in block["instructions"]
            if row.get("expr0") == 'Print("print:")'
        )
        graph = instruction["expr0_graph"]
        if kind == "print-wrong-type":
            literal = next(node for node in graph["nodes"]
                           if node["kind"] == "string_literal")
            literal["kind"], literal["text"] = "integer_literal", "7"
        elif kind == "print-wrong-arity":
            old_root = graph["root"]
            literal = copy.deepcopy(next(
                node for node in graph["nodes"]
                if node["kind"] == "string_literal"
            ))
            literal["text"] = '"extra"'
            literal_row = len(graph["nodes"])
            graph["nodes"].append(literal)
            call_argument = copy.deepcopy(graph["nodes"][old_root])
            call_argument["left"] = old_root
            call_argument["right"] = literal_row
            call_argument["text"] = 'Print("print:", "extra")'
            graph["nodes"].append(call_argument)
            graph["root"] = len(graph["nodes"]) - 1
        elif kind == "print-forged-target":
            marker = next(node for node in graph["nodes"]
                          if node.get("call_target_name") == "Print")
            marker["call_target_syntax_id"] = 999999
        elif kind == "print-extra-use":
            instruction["uses"] = ["forged.1"]
        else:
            raise SystemExit(f"unknown mutation: {kind}")
    elif kind.startswith("discarded-"):
        main = next(row for row in document["routines"]
                    if row["name"] == "Main")
        instruction = next(
            row for block in main["blocks"]
            for row in block["instructions"]
            if row.get("expr0") == 'EchoAt("discarded", 0)'
        )
        if kind == "discarded-extra-use":
            instruction["uses"] = ["forged.1"]
        elif kind == "discarded-noncall":
            graph = instruction["expr0_graph"]
            literal = copy.deepcopy(next(
                node for node in graph["nodes"]
                if node["kind"] == "string_literal"
            ))
            graph["nodes"], graph["root"] = [literal], 0
        else:
            raise SystemExit(f"unknown mutation: {kind}")
    elif kind == "carriage":
        routine["params"][0]["carriage"] = "readonly-ref"
    elif kind == "pass":
        routine["params"][1]["pass"] = "indirect"
    elif kind == "resource":
        routine["params"][0]["resource"] = "own"
    elif kind == "abi-required":
        routine["params"][1]["abi_layout_required"] = True
        routine["params"][1]["abi_layout_id"] = 1
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
