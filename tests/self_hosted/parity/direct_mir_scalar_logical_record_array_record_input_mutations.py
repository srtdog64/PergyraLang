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
         if row.get("name") == "AppendGraphRows"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 4:
        raise SystemExit("fixture has no exact AppendGraphRows signature")
    record_array = routine["params"][0]
    record_value = routine["params"][3]
    if kind == "record-array-carriage":
        record_array["carriage"] = "value"
    elif kind == "record-array-physical-abi":
        record_array["abi_layout_required"] = True
        record_array["abi_layout_id"] = 1
    elif kind == "record-input-carriage":
        record_value["carriage"] = "borrowed"
        record_value["pass"] = "indirect"
    elif kind == "record-input-unknown-declaration":
        record_value["type"] = "MissingRecord"
        record_value["abi_type_name"] = "MissingRecord"
    elif kind == "unknown-input-type":
        routine["params"][1]["type"] = "MissingScalar"
        routine["params"][1]["abi_type_name"] = "MissingScalar"
    elif kind == "unknown-return-type":
        routine["return"] = "MissingReturn"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
