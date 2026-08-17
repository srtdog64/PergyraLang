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
         if row.get("name") == "AppendGraphRow"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 2:
        raise SystemExit("fixture has no exact AppendGraphRow signature")
    record_array = routine["params"][0]
    record_value = routine["params"][1]
    if kind == "record-array-carriage":
        record_array["carriage"] = "value"
    elif kind == "record-array-physical-abi":
        record_array["abi_layout_required"] = True
        record_array["abi_layout_id"] = 1
    elif kind == "record-input-carriage":
        record_value["carriage"] = "readonly-ref"
        record_value["pass"] = "indirect"
    elif kind == "record-input-distinct-declaration":
        record_value["type"] = "OtherRow"
        record_value["abi_type_name"] = "OtherRow"
    elif kind == "record-array-missing-element":
        record_array["type"] = "Array<MissingGraphRow>"
        record_array["abi_type_name"] = "Array<MissingGraphRow>"
    elif kind == "return-type":
        routine["return"] = "Int"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
