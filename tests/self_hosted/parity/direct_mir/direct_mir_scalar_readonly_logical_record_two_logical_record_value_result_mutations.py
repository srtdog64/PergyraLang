import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        row for row in document["routines"] if row.get("name") == "BuildExecutionPlan"
    )
    params = routine["params"]
    if kind == "string-type":
        params[0]["type"] = params[1]["type"]
        params[0]["abi_type_name"] = params[1]["type"]
    elif kind == "string-carriage":
        params[0]["carriage"] = "readonly-ref"
    elif kind == "readonly-type":
        params[1]["type"] = "String"
        params[1]["abi_type_name"] = "String"
    elif kind == "readonly-carriage":
        params[1]["carriage"] = "value"
    elif kind == "readonly-pass":
        params[1]["pass"] = "direct"
    elif kind == "first-copyout-value":
        params[2]["carriage"] = "value"
    elif kind == "first-copyout-carriage":
        params[2]["carriage"] = "shared-owner"
    elif kind == "first-copyout-pass":
        params[2]["pass"] = "indirect"
    elif kind == "first-copyout-abi":
        params[2]["abi_layout_required"] = True
        params[2]["abi_layout_id"] = 7
    elif kind == "second-copyout-carriage":
        params[3]["carriage"] = "shared-owner"
    elif kind == "second-copyout-pass":
        params[3]["pass"] = "indirect"
    elif kind == "second-copyout-abi":
        params[3]["abi_layout_required"] = True
        params[3]["abi_layout_id"] = 8
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
