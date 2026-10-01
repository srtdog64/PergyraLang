#!/usr/bin/env python3
"""Coherently rewrite every reached String ABI row, including its wire ID.

No graph edge/text changes: a stale graph digest cannot impersonate physical
layout refusal. Reuse the existing gate-owned wire-ID counterpart.
"""
from copy import deepcopy
import json
from pathlib import Path
import sys

from direct_mir_physical_record_return_mutations import layout_id


def receipts(value):
    if isinstance(value, dict):
        if value.get("abi_type_name") == "Array<String>" and "abi_layout_id" in value:
            yield value
        for child in value.values():
            yield from receipts(child)
    elif isinstance(value, list):
        for child in value:
            yield from receipts(child)


def main() -> None:
    source, directory = map(Path, sys.argv[1:])
    baseline = json.loads(source.read_text(encoding="utf-8-sig"))
    rows = list(receipts(baseline))
    if not rows or any(not row.get("abi_layout_required") or
                       layout_id(row["abi_layout"]) != row["abi_layout_id"] for row in rows):
        raise AssertionError("expected complete producer-issued String layout receipts")
    int_layouts = [row["abi_layout"] for routine in baseline["routines"]
                   for row in routine.get("params", [])
                   if row.get("abi_type_name") == "Array<Int>" and row.get("abi_layout")]
    modes = ["missing", "offset", "size", "alignment", "field-size",
             "field-alignment", "field-order", "coherent-wide", "runtime", "element"]
    if int_layouts:
        modes.append("complete-cross-family")
    for mode in modes:
        document = deepcopy(baseline)
        for row in receipts(document):
            if mode == "missing":
                row.update(abi_layout_required=False, abi_layout_id=0, abi_layout=None)
                continue
            layout = row["abi_layout"]
            if mode == "offset":
                layout["fields"][1]["offset"] = 16
            elif mode == "size":
                layout["size"] = 40
            elif mode == "alignment":
                layout["align"] = 16
            elif mode == "field-size":
                layout["fields"][0]["size"] = 16
            elif mode == "field-alignment":
                layout["fields"][0]["align"] = 4
            elif mode == "field-order":
                layout["fields"][1], layout["fields"][2] = layout["fields"][2], layout["fields"][1]
            elif mode == "coherent-wide":
                layout.update(size=64, align=16)
                for index, field in enumerate(layout["fields"]):
                    field.update(offset=index * 16, size=16, align=16)
            elif mode == "runtime":
                layout["runtime_fn"] = "pgy_array_new_Int"
            elif mode == "element":
                layout["inner_c_type"] = "int32_t"
            elif mode == "complete-cross-family":
                layout = deepcopy(int_layouts[0])
                row["abi_layout"] = layout
            row["abi_layout_id"] = layout_id(layout)
        (directory / f"{mode}.mir.json").write_text(
            json.dumps(document, separators=(",", ":")), encoding="utf-8")
    (directory / "mutations.list").write_text("\n".join(modes) + "\n", encoding="utf-8")
    print(f"[array-string-layout-mutations] {len(rows)} coherent receipt rows; {len(modes)} falsifiers")


if __name__ == "__main__":
    main()
