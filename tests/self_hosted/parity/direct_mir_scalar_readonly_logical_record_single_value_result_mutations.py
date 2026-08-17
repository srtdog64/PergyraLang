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
        if row.get("name") == "MergeBranchOrder"
    )
    params = routine["params"]
    if kind == "readonly-value":
        params[0]["carriage"] = "value"
        params[0]["pass"] = "direct"
    elif kind == "merged-copyout-value":
        params[4]["carriage"] = "value"
    elif kind == "readonly-carriage":
        params[0]["carriage"] = "shared-owner"
    elif kind == "readonly-pass":
        params[0]["pass"] = "direct"
    elif kind == "merged-copyout-carriage":
        params[4]["carriage"] = "shared-owner"
    elif kind == "merged-copyout-pass":
        params[4]["pass"] = "indirect"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
