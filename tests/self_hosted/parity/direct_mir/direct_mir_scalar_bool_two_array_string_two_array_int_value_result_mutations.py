import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    interleaved = kind.startswith("interleaved-")
    name = "ReadRequires" if interleaved else "GraphAddEdge"
    routine = next(row for row in document["routines"]
                   if row.get("name") == name)
    params = routine["params"]
    if kind == "interleaved-array-string-carriage":
        params[3]["carriage"] = "borrowed"
    elif kind == "interleaved-array-int-abi":
        params[4]["abi_layout_required"] = False
        params[4]["abi_layout_id"] = 0
        params[4]["abi_layout"] = None
    elif kind == "interleaved-scalar-carriage":
        params[0]["carriage"] = "readonly-ref"
    elif kind == "interleaved-unknown-family":
        params[6]["type"] = params[6]["abi_type_name"] = "MissingScalar"
        params[6]["carriage"] = "value"
        params[6]["abi_layout_required"] = False
        params[6]["abi_layout_id"] = 0
        params[6]["abi_layout"] = None
    elif kind == "array-string-type":
        params[0]["type"] = "Array<Int>"
    elif kind == "array-string-carriage":
        params[1]["carriage"] = "borrowed"
    elif kind == "array-string-abi":
        params[0]["abi_layout_required"] = False
        params[0]["abi_layout_id"] = 0
        params[0]["abi_layout"] = None
    elif kind == "array-string-layout-mismatch":
        params[1]["abi_layout_id"] += 1
    elif kind == "array-int-type":
        params[2]["type"] = "Array<String>"
    elif kind == "array-int-carriage":
        params[3]["carriage"] = "borrowed"
    elif kind == "array-int-abi":
        params[2]["abi_layout_required"] = False
        params[2]["abi_layout_id"] = 0
        params[2]["abi_layout"] = None
    elif kind == "array-int-layout-mismatch":
        params[3]["abi_layout_id"] += 1
    elif kind == "cross-family-layout":
        params[2]["abi_layout_id"] = params[0]["abi_layout_id"]
        params[3]["abi_layout_id"] = params[0]["abi_layout_id"]
    elif kind == "cross-family-layout-row":
        params[0]["abi_layout_id"] = params[2]["abi_layout_id"]
        params[0]["abi_layout"] = params[2]["abi_layout"]
        params[1]["abi_layout_id"] = params[2]["abi_layout_id"]
        params[1]["abi_layout"] = params[2]["abi_layout"]
    elif kind == "string-carriage":
        params[7]["carriage"] = "readonly-ref"
    elif kind == "unknown-return-type":
        routine["return"] = "MissingReturn"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
