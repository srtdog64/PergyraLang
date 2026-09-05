"""Check the fact preserved by each bounded source substitution."""

import json
import sys
from pathlib import Path

case, source = sys.argv[1:]
documents = [json.loads(line) for line in Path(source).read_text(encoding="utf-8").splitlines()
             if line.startswith('{"schema":"pgy.mir.v1"')]
assert len(documents) == 1, "expected one admitted MIR document"
document = documents[0]
if case in ("action_state", "function_state", "action_zone_contract"):
    counter = next(decl for decl in document["decls"] if decl["name"] == "Counter")
    method = next(method for method in counter["methods"] if method["name"] == "Advance")
    assert method["callable_kind"] == ("function" if case == "function_state" else "action")
    contract = method["contract"]
    assert contract["within"] == ("CounterZone" if case == "action_zone_contract" else None)
    assert contract["authorized_by"] == (["self"] if case == "action_zone_contract" else [])
if case in ("where_explicit", "where_inferred"):
    intent = next(row for row in document["routines"] if row["name"] == "AdvanceCounter")
    instructions = [row for block in intent["blocks"] for row in block["instructions"]]
    assert [(row["arg0"], row["arg1"]) for row in instructions
            if row["name"] == "IntentZoneWhere"] == [("CounterZone", "Advance")]
    assert [(row["arg0"], row["arg1"]) for row in instructions
            if row["name"] == "IntentZoneAlias"] == [("counter_zone", "Advance")]
