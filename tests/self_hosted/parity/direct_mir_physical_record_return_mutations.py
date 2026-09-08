#!/usr/bin/env python3
"""Alter one reached physical record receipt; refuse a missing test target."""
import json
import pathlib
import sys
import copy


def layout_id(layout):
    """Mirror the existing MIR receipt ID, including every nominal-row field."""
    modulus = 1 << 28
    value = 60621699

    def byte(item):
        nonlocal value
        value = ((value ^ item) * 435) % modulus

    def string(text):
        for item in (text or "").encode("utf-8"):
            byte(item)
        byte(255)

    def number(integer):
        for shift in (0, 8, 16, 24):
            byte((integer >> shift) & 255)

    string(layout["type"])
    for value_field in (layout["size"], layout["align"], len(layout["fields"])):
        number(value_field)
    for field in layout["fields"]:
        string(field["name"])
        for key in ("offset", "size", "align"):
            number(field[key])
    string(layout.get("runtime_fn")); string(layout.get("inner_c_type"))
    number(layout["representation"]); string(layout.get("discriminant"))
    number(layout["primary_tag"]); number(layout["secondary_tag"])
    string(layout.get("niche_none_pattern"))
    return (1 << 29) + value


def replace_receipts(value, name, layout):
    if isinstance(value, dict):
        if value.get("abi_type_name") == name or (
                value.get("kind") == "struct" and value.get("name") == name):
            value.update(abi_layout_required=True, abi_layout_id=layout_id(layout),
                         abi_layout=copy.deepcopy(layout))
        for child in value.values():
            replace_receipts(child, name, layout)
    elif isinstance(value, list):
        for child in value:
            replace_receipts(child, name, layout)


def unique(rows, predicate):
    found = [row for row in rows if predicate(row)]
    if len(found) != 1:
        raise ValueError(f"expected one mutation target, found {len(found)}")
    return found[0]


def mutate(document, mode):
    declarations = document.get("decls", document.get("declarations", []))
    name = "SemanticAstEntrypointSelectionAccumulator"
    declaration = unique(declarations, lambda row: row.get("name") == name)
    if mode in ("coherent-wide-layout", "coherent-logical-physical"):
        layout = copy.deepcopy(declaration["abi_layout"])
        width = 8
        if mode == "coherent-logical-physical":
            name = "SemanticAstEntrypointSelectionFact"
            declaration = unique(declarations, lambda row: row.get("name") == name)
            width = 4
        layout.update(type=name, size=width * len(declaration["fields"]), align=width)
        layout["fields"] = [dict(name=field["name"], offset=index * width,
                                 size=width, align=width)
                            for index, field in enumerate(declaration["fields"])]
        replace_receipts(document, name, layout)
        return
    if mode.startswith("declaration-"):
        if mode == "declaration-missing-layout":
            del declaration["abi_layout"]
        elif mode == "declaration-absent-layout":
            declaration.update(abi_layout_required=False, abi_layout_id=0, abi_layout=None)
        elif mode == "declaration-field-identity":
            declaration["fields"][0]["source_syntax_id"] = 0
        elif mode == "declaration-layout-width":
            declaration["abi_layout"]["fields"][0]["size"] = 8
        else:
            raise ValueError(mode)
        return
    if mode.startswith("parameter-"):
        routine = unique(document["routines"], lambda row: row["name"] == "PhysicalRecordAdjust")
        parameter = routine["params"][1]
        if parameter["type"] != "PhysicalRecordTriple":
            raise ValueError("second-parameter control changed")
        if mode == "parameter-missing-layout":
            del parameter["abi_layout"]
        elif mode == "parameter-absent-layout":
            parameter.update(abi_layout_required=False, abi_layout_id=0, abi_layout=None)
        elif mode == "parameter-layout-width":
            parameter["abi_layout"]["fields"][0]["size"] = 8
        else:
            raise ValueError(mode)
        return
    instructions = [instruction for routine in document["routines"]
                    for block in routine.get("blocks", [])
                    for instruction in block.get("instructions", [])
                    if instruction.get("abi_type_name") == name]
    if not instructions:
        raise ValueError("physical instruction control missing")
    target = instructions[0]
    if mode == "instruction-missing-layout":
        del target["abi_layout"]
    elif mode == "instruction-absent-layout":
        target.update(abi_layout_required=False, abi_layout_id=0, abi_layout=None)
    elif mode == "instruction-layout-id":
        target["abi_layout_id"] += 1
    else:
        raise ValueError(mode)


if __name__ == "__main__":
    source, mode, output = sys.argv[1:]
    document = json.loads(pathlib.Path(source).read_text(encoding="utf-8"))
    mutate(document, mode)
    pathlib.Path(output).write_text(json.dumps(document, separators=(",", ":")), encoding="utf-8")
