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
         if row.get("name") == "AppendNode"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 10:
        raise SystemExit("fixture has no exact AppendNode signature")
    compact = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "BuildCompactGraph"),
        None,
    )
    if compact is None or len(compact.get("params", [])) != 7:
        raise SystemExit("fixture has no exact BuildCompactGraph signature")
    params = routine["params"]
    if kind == "array-string-carriage":
        params[1]["carriage"] = "value"
    elif kind == "array-string-abi":
        params[5]["abi_layout_required"] = False
        params[5]["abi_layout_id"] = 0
        params[5]["abi_layout"] = None
    elif kind == "array-int-count":
        params[4]["type"] = "Int"
        params[4]["abi_type_name"] = "Int"
        params[4]["abi_layout_required"] = False
        params[4]["abi_layout_id"] = 0
        params[4]["abi_layout"] = None
    elif kind == "scalar-order":
        params[7]["type"] = "Int"
        params[7]["abi_type_name"] = "Int"
    elif kind == "scalar-carriage":
        params[8]["carriage"] = "value-result"
    elif kind == "return-type":
        routine["return"] = "Int"
    elif kind == "compact-array-string-carriage":
        compact["params"][1]["carriage"] = "value"
    elif kind == "compact-scalar-type":
        compact["params"][6]["type"] = "Int"
        compact["params"][6]["abi_type_name"] = "Int"
    elif kind == "compact-parameter-count":
        extra = dict(compact["params"][6])
        extra["name"] = "extra"
        compact["params"].append(extra)
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
