#!/usr/bin/env python3
import json
import sys


def populated_literal(document):
    main = next(row for row in document["routines"] if row["name"] == "Main")
    instruction = next(
        instruction
        for block in main["blocks"]
        for instruction in block["instructions"]
        if instruction.get("expr0") == "[true, false, true]"
    )
    return instruction, instruction["expr0_graph"]["nodes"]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MODE OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as handle:
        document = json.load(handle)
    instruction, nodes = populated_literal(document)
    mode = sys.argv[2]
    if mode == "wrong-literal-kind":
        literal = next(node for node in nodes if node["kind"] == "bool_literal")
        literal["kind"] = "integer_literal"
        literal["text"] = "1"
    elif mode == "wrong-literal-spelling":
        literal = next(node for node in nodes if node["kind"] == "bool_literal")
        literal["text"] = "True"
    elif mode == "broken-spine":
        nodes[-1]["left"] = nodes[-1]["right"]
    elif mode == "unexpected-use":
        instruction["uses"] = ["forged"]
    elif mode == "array-bool-abi":
        changed = False
        for block in next(
                row for row in document["routines"] if row["name"] == "Main"
        )["blocks"]:
            for row in block["instructions"]:
                if row.get("abi_type_name") == "Array<Bool>":
                    row["abi_layout"]["fields"][0]["offset"] = 8
                    changed = True
        if not changed:
            raise SystemExit("Main has no Array<Bool> ABI row")
    else:
        raise SystemExit(f"unknown mutation: {mode}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as handle:
        json.dump(document, handle, separators=(",", ":"))
        handle.write("\n")


if __name__ == "__main__":
    main()
