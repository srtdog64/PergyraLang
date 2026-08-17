import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        row for row in document["routines"] if row.get("name") == "AdvancePresealRows"
    )
    params = routine["params"]
    if kind == "copyout-type":
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
        params[1]["type"] = "OtherTypeEnv"
        params[1]["abi_type_name"] = "OtherTypeEnv"
    elif kind == "record-carriage":
        params[1]["carriage"] = "owner-handle"
        params[1]["pass"] = "direct"
    elif kind == "string-type":
        params[2]["type"] = "Int"
        params[2]["abi_type_name"] = "Int"
    elif kind == "return-type":
        routine["return"] = "OtherTypeEnv"
    elif kind == "parameter-count":
        params.pop()
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
