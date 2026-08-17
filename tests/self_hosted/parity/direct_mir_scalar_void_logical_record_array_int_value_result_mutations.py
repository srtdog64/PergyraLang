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
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "AppendOperatorRows"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 3:
        raise SystemExit("fixture has no exact AppendOperatorRows signature")
    params = routine["params"]
    if kind == "array-int-carriage":
        params[0]["carriage"] = "readonly-ref"
    elif kind == "array-int-abi":
        clear_abi(params[0])
    elif kind == "array-int-type":
        params[0]["type"] = "Array<Bool>"
        params[0]["abi_type_name"] = "Array<Bool>"
    elif kind == "record-carriage":
        params[1]["carriage"] = "readonly-ref"
    elif kind == "record-type":
        params[1]["type"] = "MissingRecord"
        params[1]["abi_type_name"] = "MissingRecord"
    elif kind == "string-type":
        params[2]["type"] = "MissingScalar"
        params[2]["abi_type_name"] = "MissingScalar"
    elif kind == "string-carriage":
        params[2]["carriage"] = "value-result"
    elif kind == "return-type":
        routine["return"] = "MissingReturn"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
