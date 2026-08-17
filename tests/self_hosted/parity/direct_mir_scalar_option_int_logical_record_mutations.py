import json
import sys


def main():
    if len(sys.argv) not in (4, 5):
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT [ROUTINE]")
    source, kind, output = sys.argv[1:4]
    routine_name = sys.argv[4] if len(sys.argv) == 5 else "FindField"
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == routine_name), None)
    if routine is None:
        raise SystemExit(f"fixture has no {routine_name} routine")
    param = routine["params"][0]
    if kind == "carriage":
        param["carriage"] = "value"
    elif kind == "pass-shape":
        param["pass"] = "direct"
    elif kind == "resource":
        param["resource"] = "own"
    elif kind == "abi-required":
        param["abi_layout_required"] = True
        param["abi_layout_id"] = 1
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
