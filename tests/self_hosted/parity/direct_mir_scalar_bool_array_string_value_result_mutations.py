import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    if kind == "unsupported-return-type":
        routine = next(
            (row for row in document.get("routines", [])
             if row.get("name") == "CaptureFields"), None)
        if routine is None:
            raise SystemExit("fixture has no CaptureFields routine")
        routine["return"] = "Unknown"
    elif kind == "fourth-copyout-carriage":
        routine = next(
            (row for row in document.get("routines", [])
             if row.get("name") == "CaptureFields"), None)
        if routine is None:
            raise SystemExit("fixture has no CaptureFields routine")
        routine["params"][6]["carriage"] = "value"
    elif kind in ("single-copyout-carriage", "single-copyout-abi-layout",
                  "single-prefix-carriage"):
        routine = next(
            (row for row in document.get("routines", [])
             if row.get("name") == "ReadStringArray"), None)
        if routine is None:
            raise SystemExit("fixture has no ReadStringArray routine")
        if kind == "single-copyout-carriage":
            routine["params"][5]["carriage"] = "value"
        elif kind == "single-copyout-abi-layout":
            routine["params"][5]["abi_layout_id"] = 0
        else:
            routine["params"][3]["carriage"] = "value-result"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
