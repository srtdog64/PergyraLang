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
        params[1]["carriage"] = "borrowed"
    elif kind == "array-string-abi":
        params[5]["abi_layout_required"] = False
        params[5]["abi_layout_id"] = 0
        params[5]["abi_layout"] = None
    elif kind == "array-int-unknown-type":
        params[4]["type"] = "MissingCollection"
        params[4]["abi_type_name"] = "MissingCollection"
        params[4]["abi_layout_required"] = False
        params[4]["abi_layout_id"] = 0
        params[4]["abi_layout"] = None
    elif kind == "scalar-unknown-type":
        params[7]["type"] = "MissingScalar"
        params[7]["abi_type_name"] = "MissingScalar"
    elif kind == "scalar-carriage":
        params[8]["carriage"] = "borrowed"
    elif kind == "unknown-return-type":
        routine["return"] = "MissingReturn"
    elif kind == "compact-array-string-carriage":
        compact["params"][1]["carriage"] = "borrowed"
    elif kind == "compact-scalar-unknown-type":
        compact["params"][6]["type"] = "MissingScalar"
        compact["params"][6]["abi_type_name"] = "MissingScalar"
    elif kind == "compact-extra-unknown-type":
        extra = dict(compact["params"][6])
        extra["name"] = "extra"
        extra["type"] = "MissingExtra"
        extra["abi_type_name"] = "MissingExtra"
        compact["params"].append(extra)
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
