import json
import sys


def target(document):
    return next(row for row in document["routines"] if row.get("name") == "Reachable")


def return_instruction(routine):
    return next(
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("kind") == "return"
    )


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = target(document)
    params = routine["params"]
    if kind == "record-type":
        params[0]["type"] = "String"
        params[0]["abi_type_name"] = "String"
    elif kind == "record-carriage":
        params[0]["carriage"] = "value"
    elif kind == "record-pass":
        params[0]["pass"] = "direct"
    elif kind == "record-abi":
        params[0]["abi_layout_required"] = True
        params[0]["abi_layout_id"] = 1
    elif kind == "int-type":
        params[1]["type"] = "Bool"
        params[1]["abi_type_name"] = "Bool"
    elif kind == "int-carriage":
        params[1]["carriage"] = "readonly-ref"
    elif kind == "bool-type":
        params[2]["type"] = "Int"
        params[2]["abi_type_name"] = "Int"
    elif kind == "bool-carriage":
        params[2]["carriage"] = "readonly-ref"
    elif kind == "return-type":
        routine["return"] = "Array<Int>"
    elif kind == "return-abi-missing":
        instruction = return_instruction(routine)
        instruction["abi_layout_required"] = False
        instruction["abi_layout_id"] = 0
        instruction["abi_layout"] = None
    elif kind == "return-abi-layout":
        fields = return_instruction(routine).get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<Bool> return ABI")
        fields[1]["offset"] = 0
    elif kind == "parameter-count":
        params.pop()
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
