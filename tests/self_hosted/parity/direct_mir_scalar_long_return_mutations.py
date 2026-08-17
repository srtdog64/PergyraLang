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
         if row.get("name") == "LongModulus"),
        None,
    )
    if routine is None:
        raise SystemExit("fixture has no LongModulus routine")
    instruction = routine["blocks"][0]["instructions"][0]
    if kind == "long-return-type":
        routine["return"] = "Int"
    elif kind == "long-literal-kind":
        instruction["expr0_graph"]["nodes"][0]["kind"] = "integer_literal"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
