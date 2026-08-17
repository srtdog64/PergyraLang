import json
import sys


def clear_abi(param):
    param["abi_layout_required"] = False
    param["abi_layout_id"] = 0
    param["abi_layout"] = None


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    single = kind.startswith("single-")
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") ==
            ("ReadRequiredBool" if single else "AppendLaneRows")),
        None,
    )
    expected_count = 5 if single else 11
    if routine is None or len(routine.get("params", [])) != expected_count:
        raise SystemExit("fixture has no expected Bool copyout signature")
    params = routine["params"]
    instructions = [
        instruction
        for block in routine.get("blocks", [])
        for instruction in block.get("instructions", [])
    ]
    bool_push = next(
        (instruction for instruction in instructions
         if instruction.get("arg0") == "ArrayPush"), None)
    bool_set = next(
        (instruction for instruction in instructions
         if instruction.get("arg0") == "ArraySet"), None)
    if kind == "single-array-bool-carriage":
        params[4]["carriage"] = "value"
    elif kind == "single-array-bool-abi":
        clear_abi(params[4])
    elif kind == "single-prefix-carriage":
        params[3]["carriage"] = "value-result"
    elif kind == "array-bool-carriage":
        params[1]["carriage"] = "value"
    elif kind == "array-bool-abi":
        clear_abi(params[1])
    elif kind == "array-bool-type":
        params[1]["type"] = "Array<Int>"
        params[1]["abi_type_name"] = "Array<Int>"
    elif kind == "bool-carriage":
        params[9]["carriage"] = "value-result"
    elif kind == "return-type":
        routine["return"] = "Int"
    elif kind in {
        "array-bool-push-receiver-missing",
        "array-bool-push-receiver-foreign",
        "array-bool-push-value-type",
        "array-bool-set-receiver-missing",
        "array-bool-set-receiver-foreign",
        "array-bool-set-value-type",
    }:
        target = bool_push if "push" in kind else bool_set
        if target is None:
            raise SystemExit("fixture has no Array<Bool> value-result mutation")
        if kind.endswith("receiver-missing"):
            target["local_ref"] = None
        elif kind.endswith("receiver-foreign"):
            target["local_ref"] = (
                f"parameter:{routine['source_syntax_id']}:0")
        else:
            graph = target["expr0_graph"]
            root = graph["root"]
            graph["nodes"][root]["kind"] = "integer_literal"
            graph["nodes"][root]["text"] = "1"
            target["expr0"] = "1"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
