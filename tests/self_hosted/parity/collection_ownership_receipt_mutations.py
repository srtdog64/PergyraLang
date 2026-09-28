"""Bounded falsifiers for exact collection-ownership instruction receipts."""

import copy
import json
import pathlib
import sys


source, output = map(pathlib.Path, sys.argv[1:])
base = json.loads(source.read_text(encoding="utf-8"))
output.mkdir(parents=True, exist_ok=True)


def instructions(program):
    for routine in program["routines"]:
        for block in routine["blocks"]:
            yield from block["instructions"]


def receipt_rows(program):
    rows = list(instructions(program))
    push = next(row for row in rows
                if (row.get("collection_ownership_receipt") or {}).get("kind")
                == "owned-string-push")
    drop = next(row for row in rows
                if (row.get("collection_ownership_receipt") or {}).get("kind")
                == "drop")
    plain = next(row for row in rows
                 if row.get("collection_ownership_receipt") is None)
    return push, drop, plain


mutations = (
    "missing-push",
    "missing-drop",
    "all-missing",
    "wrong-binding",
    "wrong-kind",
    "moved-receipt",
    "duplicate-receipt",
    "wrong-source-binding",
)

for mutation in mutations:
    data = copy.deepcopy(base)
    push, drop, plain = receipt_rows(data)
    if mutation == "missing-push":
        push["collection_ownership_receipt"] = None
    elif mutation == "missing-drop":
        drop["collection_ownership_receipt"] = None
    elif mutation == "all-missing":
        push["collection_ownership_receipt"] = None
        drop["collection_ownership_receipt"] = None
    elif mutation == "wrong-binding":
        push["collection_ownership_receipt"]["receiver_binding_syntax_id"] += 1
    elif mutation == "wrong-kind":
        push["collection_ownership_receipt"]["kind"] = "owned-string-pop"
    elif mutation == "moved-receipt":
        plain["collection_ownership_receipt"] = copy.deepcopy(
            push["collection_ownership_receipt"])
        push["collection_ownership_receipt"] = None
    elif mutation == "duplicate-receipt":
        plain["collection_ownership_receipt"] = copy.deepcopy(
            push["collection_ownership_receipt"])
    else:
        push["collection_ownership_receipt"]["source_binding_syntax_id"] = (
            push["collection_ownership_receipt"]["receiver_binding_syntax_id"])
    destination = output / (mutation + ".mir.json")
    destination.write_text(
        json.dumps(data, separators=(",", ":")), encoding="utf-8")
