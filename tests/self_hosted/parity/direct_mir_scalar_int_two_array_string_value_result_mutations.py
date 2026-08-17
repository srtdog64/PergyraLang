import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    production = kind.startswith("production-")
    routine_name = "EmitDeclFields" if production else "OwnerGraphNodeIndexOrAppend"
    routine = next(row for row in document["routines"]
                   if row.get("name") == routine_name)
    params = routine["params"]
    if kind == "first-copyout-type":
        params[0]["type"] = "Unknown"
    elif kind == "second-copyout-carriage":
        params[1]["carriage"] = "readonly-ref"
    elif kind == "copyout-pass":
        params[0]["pass"] = "indirect"
    elif kind == "copyout-abi":
        params[1]["abi_layout_required"] = False
        params[1]["abi_layout_id"] = 0
        params[1]["abi_layout"] = None
    elif kind == "copyout-layout-mismatch":
        params[1]["abi_layout_id"] += 1
    elif kind == "string-carriage":
        params[3]["carriage"] = "readonly-ref"
    elif kind == "return-type":
        routine["return"] = "Unknown"
    elif kind == "production-copyout-carriage":
        params[7]["carriage"] = "readonly-ref"
    elif kind == "production-copyout-abi":
        params[7]["abi_layout_required"] = False
        params[7]["abi_layout_id"] = 0
        params[7]["abi_layout"] = None
    elif kind == "production-copyout-layout":
        params[7]["abi_layout_id"] += 1
    elif kind == "production-scalar-carriage":
        params[4]["carriage"] = "readonly-ref"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
