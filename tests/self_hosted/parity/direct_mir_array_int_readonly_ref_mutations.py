"""Bounded readonly-array ABI/carriage falsifiers; no compiler fact authority."""
import copy
import json
import pathlib
import sys


def nodes(value):
    if isinstance(value, dict):
        yield value
        for child in value.values():
            yield from nodes(child)
    elif isinstance(value, list):
        for child in value:
            yield from nodes(child)


source, output = map(pathlib.Path, sys.argv[1:])
base = json.loads(source.read_text(encoding="utf-8"))
mutations = ["missing-abi", "layout-id", "abi-required", "storage-align",
             "element-width", "cross-family", "pass-shape", "resource",
             "owner-carriage", "binding-ordinal", "writer-readonly",
             "negate-child", "negate-kind", "negate-binding"]
for mutation in mutations:
    data = copy.deepcopy(base)
    routine = next(row for row in data["routines"] if row["name"] == "Count")
    param = routine["params"][0]
    assert param["type"] == "Array<Int>" and param["carriage"] == "readonly-ref"
    if mutation == "missing-abi":
        param.pop("abi_layout")
    elif mutation == "layout-id":
        param["abi_layout_id"] = 0
    elif mutation == "abi-required":
        param["abi_layout_required"] = False
    elif mutation == "storage-align":
        param["abi_layout"]["align"] = 4
    elif mutation == "element-width":
        param["abi_layout"]["inner_c_type"] = "int64_t"
    elif mutation == "cross-family":
        param["abi_type_name"] = "Array<String>"
        param["abi_layout_id"] = 703020034
        param["abi_layout"]["type"] = "Array<String>"
        param["abi_layout"]["runtime_fn"] = "pgy_array_new_String"
        param["abi_layout"]["inner_c_type"] = "const char*"
    elif mutation == "pass-shape":
        param["pass"] = "indirect"
    elif mutation == "resource":
        param["resource"] = "result"
    elif mutation == "owner-carriage":
        param["carriage"] = "owner-handle"
    elif mutation == "binding-ordinal":
        bindings = [row for row in nodes(routine["blocks"])
                    if row.get("binding_kind") == "formal_parameter"]
        assert bindings
        for binding in bindings:
            binding["binding_ordinal"] = 1
    elif mutation.startswith("negate-"):
        main = next(row for row in data["routines"] if row["name"] == "Main")
        negate = next(row for row in nodes(main) if row.get("kind") == "negate")
        if mutation == "negate-child":
            negate["left"] = 0
        elif mutation == "negate-kind":
            negate["kind"] = "logical_not"
        else:
            negate.update(binding_kind="formal_parameter", binding_ordinal=0,
                          binding_syntax_id=param["source_syntax_id"])
    else:
        writer = next(row for row in data["routines"] if row["name"] == "AppendAndCount")
        assert writer["params"][0]["carriage"] == "value-result"
        writer["params"][0]["carriage"] = "readonly-ref"
    (output / (mutation + ".mir.json")).write_text(
        json.dumps(data, separators=(",", ":")), encoding="utf-8")

negative_sources = {
    "push": "func Bad(ref values: Array<Int>) -> Void { ArrayPush(values, 4); }",
    "set": "func Bad(ref values: Array<Int>) -> Void { values[0] = 4; }",
    "return": "func Bad(ref values: Array<Int>) -> Array<Int> { return values; }",
    "copyout": "func Change(inout values: Array<Int>) -> Void { ArrayPush(values, 4); }\n"
               "func Bad(ref values: Array<Int>) -> Void { Change(values); }",
}
for name, program in negative_sources.items():
    call = "let escaped: Array<Int> = Bad(values);" if name == "return" else "Bad(values);"
    (output / (name + ".pgy")).write_text(
        program + "\nfunc Main() -> Void { let mut values: Array<Int> = [1]; "
        + call + " }\n", encoding="utf-8")
