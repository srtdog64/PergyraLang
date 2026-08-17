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
         if row.get("name") == "RetainRows"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 3:
        raise SystemExit("fixture has no exact RetainRows signature")
    record_array = routine["params"][1]
    destructure = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ParseDestructureLetStmt"),
        None,
    )
    if destructure is None or len(destructure.get("params", [])) != 5:
        raise SystemExit("fixture has no exact ParseDestructureLetStmt signature")
    graph_rows = destructure["params"][4]
    push = next(
        (instruction for block in routine.get("blocks", [])
         for instruction in block.get("instructions", [])
         if instruction.get("arg0") == "ArrayPush"), None)
    record_set = next(
        (instruction for block in routine.get("blocks", [])
         for instruction in block.get("instructions", [])
         if instruction.get("arg0") == "ArraySet" and
         instruction.get("local_ref") ==
         f"parameter:{routine['source_syntax_id']}:1"), None)
    if kind.startswith("record-array-push-") and push is None:
        raise SystemExit("fixture has no record-array push")
    if kind.startswith("record-array-set-") and record_set is None:
        raise SystemExit("fixture has no record-array set")
    main = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "Main"), None)
    if main is None:
        raise SystemExit("fixture has no Main routine")
    local_definition = next(
        (instruction for block in main.get("blocks", [])
         for instruction in block.get("instructions", [])
         if instruction.get("arg0") == "rows"), None)
    if local_definition is None:
        raise SystemExit("fixture has no local record-array definition")
    if kind == "record-array-copyout-carriage":
        record_array["carriage"] = "value"
    elif kind == "graph-row-copyout-carriage":
        graph_rows["carriage"] = "value"
    elif kind == "graph-row-copyout-missing-element":
        graph_rows["type"] = "Array<MissingAstExpressionGraphRows>"
        graph_rows["abi_type_name"] = "Array<MissingAstExpressionGraphRows>"
    elif kind == "graph-row-copyout-physical-abi":
        graph_rows["abi_layout_required"] = True
        graph_rows["abi_layout_id"] = 1
    elif kind == "record-array-missing-element":
        record_array["type"] = "Array<MissingInventoryRow>"
        record_array["abi_type_name"] = "Array<MissingInventoryRow>"
    elif kind == "record-array-physical-abi":
        record_array["abi_layout_required"] = True
        record_array["abi_layout_id"] = 1
    elif kind == "record-array-push-binding":
        push["local_ref"] = f"parameter:{routine['source_syntax_id']}:2"
    elif kind == "record-array-push-field-type":
        literal = next(
            (node for node in push["expr0_graph"]["nodes"]
             if node.get("kind") == "integer_literal"), None)
        if literal is None:
            raise SystemExit("fixture push has no integer field")
        literal["kind"] = "string_literal"
        literal["text"] = '"wrong"'
    elif kind == "record-array-push-source-kind":
        push["source_type"] = "AST_ASSIGNMENT"
    elif kind == "record-array-set-binding":
        record_set["local_ref"] = \
            f"parameter:{routine['source_syntax_id']}:2"
    elif kind == "record-array-set-index-type":
        node = record_set["expr1_graph"]["nodes"][0]
        node["kind"] = "string_literal"
        node["text"] = '"wrong"'
    elif kind == "record-array-set-value-type":
        replacement = next(
            row for row in document["routines"]
            if row.get("name") == "ReplacementRow")
        replacement["return"] = "String"
    elif kind == "record-array-local-missing-element":
        local = next(row for row in main["source_locals"]
                     if row.get("name") == "rows")
        local["type"] = "Array<MissingInventoryRow>"
        local_definition["expr1"] = "Array<MissingInventoryRow>"
        local_definition["abi_type_name"] = "Array<MissingInventoryRow>"
    elif kind == "record-array-local-literal-shape":
        root = local_definition["expr0_graph"]["root"]
        local_definition["expr0_graph"]["nodes"][root]["kind"] = \
            "integer_literal"
        local_definition["expr0_graph"]["nodes"][root]["text"] = "0"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
