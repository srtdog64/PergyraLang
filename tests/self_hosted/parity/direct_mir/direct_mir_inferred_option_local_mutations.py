import copy
import json
import sys


def routine(program, name):
    for row in program.get("routines", []):
        if row.get("name") == name:
            return row
    raise SystemExit(f"missing routine: {name}")


def instructions(row):
    for block in row.get("blocks", []):
        for instruction in block.get("instructions", []):
            yield instruction


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MUTATION OUTPUT")
    with open(sys.argv[1], "r", encoding="utf-8") as source:
        program = json.load(source)

    mutation = sys.argv[2]
    target = routine(program, "ResolveOptionLocal")
    selected = [
        row for row in instructions(target)
        if str(row.get("result") or "").startswith("selected.")
        and row.get("kind") == "def"
    ]
    if not selected:
        raise SystemExit("missing selected definitions")

    if mutation == "source-type-mismatch":
        for local in target.get("source_locals", []):
            if local.get("name") == "selected":
                local["type"] = "Option<Bool>"
                break
        else:
            raise SystemExit("missing selected source local")
    elif mutation == "foreign-local-identity":
        selected[-1]["arg0"] = "foreign_selected"
    elif mutation == "mixed-concrete-types":
        option_string = next(
            row for row in instructions(target)
            if row.get("abi_type_name") == "Option<String>"
        )
        for key in (
            "abi_type_name", "abi_layout_id", "abi_layout_required",
            "abi_layout",
        ):
            selected[0][key] = copy.deepcopy(option_string.get(key))
    elif mutation in (
        "wrong-phi-incoming-type", "duplicate-phi-incoming",
        "missing-phi-incoming",
    ):
        phi = next(
            row for row in instructions(target)
            if row.get("kind") == "phi" and row.get("name") == "selected"
        )
        if mutation == "wrong-phi-incoming-type":
            option_string = next(
                row for row in instructions(target)
                if row.get("abi_type_name") == "Option<String>"
            )
            phi["uses"][1] = option_string["result"]
        elif mutation == "duplicate-phi-incoming":
            phi["uses"][1] = phi["uses"][0]
        else:
            phi["uses"].pop()
    elif mutation == "string-mixed-concrete-types":
        string_target = routine(program, "ResolveOptionString")
        chosen = [
            row for row in instructions(string_target)
            if str(row.get("result") or "").startswith("chosen.")
            and row.get("kind") == "def"
        ]
        option_int = next(
            row for row in instructions(target)
            if row.get("abi_type_name") == "Option<Int>"
        )
        for key in (
            "abi_type_name", "abi_layout_id", "abi_layout_required",
            "abi_layout",
        ):
            chosen[0][key] = copy.deepcopy(option_int.get(key))
    else:
        raise SystemExit(f"unknown mutation: {mutation}")

    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(program, output, ensure_ascii=False, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
