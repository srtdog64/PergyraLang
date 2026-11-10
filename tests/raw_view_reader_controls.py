#!/usr/bin/env python3
"""Corrupt emitted MIR rows to exercise the Pergyra cleanup consumer."""
import copy
import json
import sys
from pathlib import Path

work = Path(sys.argv[1])
original = json.loads((work / "input.mir.json").read_text())
for mode in ("missing_abi", "wrong_abi", "wrong_binding", "wrong_kind", "duplicate_edge", "missing_def_identity"):
    doc = copy.deepcopy(original)
    routine = next(r for r in doc["routines"] if r["name"] == "Work")
    block = next(b for b in routine["blocks"] if any(i["kind"] == "cleanup" for i in b["instructions"]))
    cleanup = next(i for i in block["instructions"] if i["kind"] == "cleanup")
    if mode == "missing_abi":
        del cleanup["runtime_call_abi"]
    elif mode == "wrong_abi":
        cleanup["runtime_call_abi"]["symbol"] = "pgy_array_drop_String"
    elif mode == "wrong_binding":
        cleanup["collection_cleanup"]["binding_syntax_id"] += 1
    elif mode == "wrong_kind":
        cleanup["collection_cleanup"]["kind"] = 2
    elif mode == "duplicate_edge":
        duplicate = copy.deepcopy(cleanup)
        duplicate["id"] = 1 + max(i["id"] for b in routine["blocks"] for i in b["instructions"])
        block["instructions"].append(duplicate)
    else:
        definition = next(i for b in routine["blocks"] for i in b["instructions"]
                          if i["kind"] == "def" and i["abi_type_name"] == "Array<Int>")
        del definition["binding_syntax_id"]
    (work / f"reader-{mode}.json").write_text(json.dumps(doc, separators=(",", ":")))
