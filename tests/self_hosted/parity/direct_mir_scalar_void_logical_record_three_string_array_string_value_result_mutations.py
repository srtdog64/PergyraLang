import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(row for row in document["routines"] if row.get("name") == "AppendBinding")
    params = routine["params"]
    if kind == "copyout-carriage":
        params[0]["carriage"] = "borrowed"
    elif kind == "copyout-type":
        params[0]["type"] = "Array<Int>"
        params[0]["abi_type_name"] = "Array<Int>"
    elif kind == "copyout-pass":
        params[0]["pass"] = "indirect"
    elif kind == "copyout-abi":
        params[0]["abi_layout_required"] = False
        params[0]["abi_layout_id"] = 0
        params[0]["abi_layout"] = None
    elif kind == "copyout-layout":
        fields = params[0].get("abi_layout", {}).get("fields", [])
        if len(fields) != 4:
            raise SystemExit("fixture has no four-field Array<String> ABI")
        fields[2]["offset"] = 0
    elif kind == "record-type":
        params[1]["type"] = "MissingValueEnv"
        params[1]["abi_type_name"] = "MissingValueEnv"
    elif kind == "record-carriage":
        params[1]["carriage"] = "borrowed"
        params[1]["pass"] = "indirect"
    elif kind == "record-abi":
        params[1]["abi_layout_required"] = True
        params[1]["abi_layout_id"] = params[0]["abi_layout_id"]
        params[1]["abi_layout"] = params[0]["abi_layout"]
    elif kind == "unknown-scalar-type":
        params[2]["type"] = "MissingScalar"
        params[2]["abi_type_name"] = "MissingScalar"
    elif kind == "string-carriage":
        params[3]["carriage"] = "readonly-ref"
        params[3]["pass"] = "indirect"
    elif kind == "unknown-return-type":
        routine["return"] = "MissingReturn"
    elif kind == "parameter-count":
        extra = dict(params[4])
        extra["name"] = "extra"
        params.append(extra)
        extra_two = dict(params[4])
        extra_two["name"] = "extra_two"
        params.append(extra_two)
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
