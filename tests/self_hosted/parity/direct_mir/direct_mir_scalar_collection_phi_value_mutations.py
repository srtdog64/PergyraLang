import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routines = {routine["name"]: routine for routine in document.get("routines", [])}
    instructions = [instruction for block in routines["SelectCollections"].get("blocks", [])
            for instruction in block.get("instructions", [])]
    phis = [instruction for instruction in instructions
            if instruction.get("kind") == "phi" and
            instruction.get("name") in ("names", "numbers")]
    if len(phis) != 2:
        raise SystemExit("fixture has no exact collection phi pair")
    update_instructions = [instruction
            for block in routines["UpdateNames"].get("blocks", [])
            for instruction in block.get("instructions", [])]
    value_result_phis = [instruction for instruction in update_instructions
                         if instruction.get("kind") == "phi" and
                         instruction.get("name") == "names"]
    if len(value_result_phis) != 1:
        raise SystemExit("fixture has no exact value-result Array<String> phi")
    if kind == "incoming-local":
        phis[0]["uses"][0] = phis[1]["uses"][0]
    elif kind == "result-type":
        definition = next(instruction for instruction in instructions
                          if instruction.get("result") == phis[0]["uses"][0])
        definition["abi_type_name"] = "Array<Int>"
    elif kind == "missing-incoming":
        phis[0]["uses"].pop()
    elif kind == "value-result-incoming":
        incoming = next(instruction["result"] for instruction in update_instructions
                        if instruction.get("result", "").startswith("index."))
        value_result_phis[0]["uses"][1] = incoming
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
