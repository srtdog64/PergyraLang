import copy
import json
import sys

def target(document):
    return next(row for row in document["routines"] if row.get("name") == "Schedule")

def call_target_node(document):
    main = next(row for row in document["routines"] if row.get("name") == "Main")
    return next(
        node
        for block in main["blocks"]
        for instruction in block["instructions"]
        for graph_name in ("expr0_graph", "expr1_graph")
        if instruction.get(graph_name)
        for node in instruction[graph_name]["nodes"]
        if node.get("call_target_name") == "Schedule"
    )

def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = target(document)
    param = routine["params"][0]
    if kind == "parameter-type":
        param["type"] = "OtherScheduleGraph"
        param["abi_type_name"] = "OtherScheduleGraph"
    elif kind == "carriage-invalid":
        param["carriage"] = "shared-owner"
    elif kind == "carriage-copyout":
        param["carriage"] = "value-result"
    elif kind == "parameter-pass":
        param["pass"] = "indirect"
    elif kind == "parameter-resource":
        param["resource"] = "owned"
    elif kind == "parameter-abi":
        param["abi_layout_required"] = True
        param["abi_layout_id"] = 1
    elif kind == "return-type":
        routine["return"] = "OtherScheduleGraph"
    elif kind == "return-abi":
        instruction = next(
            row for block in routine["blocks"] for row in block["instructions"]
            if row.get("kind") == "return"
        )
        instruction["abi_layout_required"] = True
        instruction["abi_layout_id"] = 1
    elif kind == "parameter-count":
        extra = copy.deepcopy(param)
        extra["name"] = "other"
        extra["type"] = "Int"
        extra["abi_type_name"] = "Int"
        extra["carriage"] = "value"
        routine["params"].append(extra)
    elif kind == "call-target-missing":
        call_target_node(document)["call_target_syntax_id"] = 0
    elif kind == "call-target-foreign":
        main = next(row for row in document["routines"] if row.get("name") == "Main")
        call_target_node(document)["call_target_syntax_id"] = main["source_syntax_id"]
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")

if __name__ == "__main__":
    main()
