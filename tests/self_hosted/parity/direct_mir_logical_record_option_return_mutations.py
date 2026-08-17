import copy
import json
import sys

source, mutation, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)
bad = copy.deepcopy(document)

if mutation == "return-carrier":
    changed = False
    for routine in bad["routines"]:
        if routine["name"] != "WrapProbe":
            continue
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                if instruction["kind"] == "return" and instruction["expr0"].startswith("Some("):
                    instruction["abi_type_name"] = "Option<Unknown>"
                    changed = True
    assert changed
elif mutation == "local-carrier":
    changed = False
    for routine in bad["routines"]:
        for block in routine["blocks"]:
            for instruction in block["instructions"]:
                if instruction.get("result") == "wrapped.1":
                    instruction["abi_type_name"] = "Option<Unknown>"
                    changed = True
                    break
            if changed:
                break
        if changed:
            break
    assert changed
elif mutation == "record-physical-abi":
    declaration = next(row for row in bad["decls"] if row["name"] == "ProbeFact")
    declaration["abi_layout_required"] = True
    assert declaration["abi_layout_id"] == 0 and declaration["abi_layout"] is None
else:
    raise SystemExit(f"unknown mutation: {mutation}")

with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(bad, stream, separators=(",", ":"))
    stream.write("\n")
