"""Validation-only raw-envelope faults; never emits or executes supplied MIR."""
import json
import sys

source, mode, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)
routines = document["routines"]
build = next(row for row in routines if row["name"] == "BuildMapForSnapshot")
read = next(row for row in routines if row["name"] == "ReadMapValue")
assert len(build["params"]) == 0 and len(read["params"]) == 1
if mode == "unknown-return":
    build["return"] = "Unknown"
elif mode == "parameter-abi-mismatch":
    parameter = read["params"][0]
    assert parameter["type"] == parameter["abi_type_name"]
    parameter["abi_type_name"] = "HashMap<Int,Int>"
elif mode == "signature-mismatch":
    # The indexed routine still resolves, but it cannot satisfy the exact
    # named projection contract. No guessed name/row fallback is permitted.
    read["name"] = "ForeignReadMapValue"
else:
    raise ValueError(f"unsupported projection fault: {mode}")
with open(output, "w", encoding="utf-8") as stream:
    json.dump(document, stream, separators=(",", ":"))
