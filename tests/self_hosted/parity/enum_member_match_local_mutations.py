"""Falsify nominal match-local admission; never execute a mutated artifact."""

import json
import sys

source, mutation, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)
routine = next(row for row in document["routines"] if row["name"] == "EnumMemberScore")
locals_ = [row for row in routine["source_locals"] if row["type"] == "Signal"]
assert len(locals_) == 1, "fixture must materialize exactly one nominal match subject"
local = locals_[0]
definitions = [ins for block in routine["blocks"] for ins in block["instructions"]
               if ins.get("kind") == "def" and ins.get("arg0") == local["name"]]
assert len(definitions) == 1 and definitions[0]["abi_type_name"] == "Signal"
assert any(parameter["type"] == "Signal"
           for row in document["routines"] if row["name"] != "EnumMemberScore"
           for parameter in row["params"]), "Signal reference must survive local mutations"

if mutation == "enum-local-roundtrip":
    pass  # Positive serialization control; no facts are changed.
elif mutation == "enum-local-unknown-type":
    local["type"] = "MissingSignal"
    definitions[0]["abi_type_name"] = "MissingSignal"
    definitions[0]["expr1"] = "MissingSignal"
elif mutation == "enum-local-erased-type":
    local["type"] = "Int"
    definitions[0]["abi_type_name"] = "Int"
    definitions[0]["expr1"] = "Int"
elif mutation == "enum-local-other-enum":
    assert any(row["name"] == "OtherSignal" for row in document["decls"])
    assert any(row["name"] == "OtherMark" and row["params"][0]["type"] == "OtherSignal"
               for row in document["routines"])
    local["type"] = "OtherSignal"
    definitions[0]["abi_type_name"] = "OtherSignal"
    definitions[0]["expr1"] = "OtherSignal"
elif mutation == "enum-local-missing-declaration":
    declarations = [row for row in document["decls"] if row["name"] == "Signal"]
    assert len(declarations) == 1
    document["decls"].remove(declarations[0])
else:
    raise SystemExit(f"unknown mutation: {mutation}")

with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(document, stream, separators=(",", ":"))
    stream.write("\n")
