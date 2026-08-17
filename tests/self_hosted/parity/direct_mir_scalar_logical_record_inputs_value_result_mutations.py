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
         if row.get("name") == "AppendStatementFacts"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 3:
        raise SystemExit("fixture has no exact AppendStatementFacts signature")
    params = routine["params"]
    if kind == "first-record-type":
        params[0]["type"] = "MissingRecord"
        params[0]["abi_type_name"] = "MissingRecord"
    elif kind == "first-record-carriage":
        params[0]["carriage"] = "readonly-ref"
    elif kind == "copyout-type":
        params[2]["type"] = "MissingRecord"
        params[2]["abi_type_name"] = "MissingRecord"
    elif kind == "copyout-carriage":
        params[2]["carriage"] = "readonly-ref"
    elif kind == "copyout-pass":
        params[2]["pass"] = "indirect"
    elif kind == "copyout-abi":
        params[2]["abi_layout_required"] = True
        params[2]["abi_layout_id"] = 1
    elif kind == "return-type":
        routine["return"] = "MissingReturn"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
