import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        row for row in document["routines"]
        if row.get("name") == "CaptureNullableNames"
    )
    params = routine["params"]
    if kind == "record-type":
        params[0]["type"] = "String"
    elif kind == "record-carriage":
        params[0]["carriage"] = "value"
    elif kind == "record-pass":
        params[0]["pass"] = "direct"
    elif kind == "string-type":
        params[1]["type"] = "Int"
    elif kind == "copyout-type":
        params[3]["type"] = "Array<Int>"
    elif kind == "copyout-carriage":
        params[4]["carriage"] = "value"
    elif kind == "copyout-abi":
        params[3]["abi_layout_required"] = False
        params[3]["abi_layout_id"] = 0
        params[3]["abi_layout"] = None
    elif kind == "copyout-layout-mismatch":
        params[4]["abi_layout_id"] += 1
    elif kind == "return-type":
        routine["return"] = "Int"
    elif kind == "parameter-count":
        params.pop()
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
