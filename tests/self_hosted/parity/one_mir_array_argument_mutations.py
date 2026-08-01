"""Falsifying MIR mutations for the bounded Array<Int> argument gate."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else "mutate"
baseline = json.loads(source.read_text(encoding="utf-8"))


def routine(document, name):
    matches = [row for row in document["routines"] if row["name"] == name]
    if len(matches) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return matches[0]


if mode == "compare-param":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    keys = (
        "name", "type", "carriage", "resource", "pass", "abi_type_name",
        "abi_layout_id", "abi_layout_required", "abi_layout",
    )
    for routine_name in ("Double", "SumPair"):
        actual = routine(baseline, routine_name)["params"]
        expected = routine(oracle, routine_name)["params"]
        if len(actual) != 1 or len(expected) != 1:
            raise RuntimeError(f"{routine_name} parameter cardinality drifted")
        if set(actual[0]) != set(keys) or set(expected[0]) != set(keys):
            raise RuntimeError(f"{routine_name} parameter schema drifted")
        if actual[0] != expected[0]:
            raise RuntimeError(f"{routine_name} parameter ABI receipt drifted")
    raise SystemExit(0)


output_dir = target


def instruction(document, name):
    return routine(document, name)["blocks"][0]["instructions"][0]


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def layout_id(layout):
    modulus = 1 << 28

    def hash_byte(value, byte):
        return ((value ^ byte) * 435) % modulus

    def hash_string(value, text):
        for byte in (text or "").encode("utf-8"):
            value = hash_byte(value, byte)
        return hash_byte(value, 255)

    def hash_u32(value, number):
        number &= 0xFFFFFFFF
        for shift in (0, 8, 16, 24):
            value = hash_byte(value, (number >> shift) & 0xFF)
        return value

    value = hash_string(60621699, layout["type"])
    value = hash_u32(value, layout["size"])
    value = hash_u32(value, layout["align"])
    value = hash_u32(value, len(layout["fields"]))
    for field in layout["fields"]:
        value = hash_string(value, field["name"])
        value = hash_u32(value, field["offset"])
        value = hash_u32(value, field["size"])
        value = hash_u32(value, field["align"])
    value = hash_string(value, layout.get("runtime_fn"))
    value = hash_string(value, layout.get("inner_c_type"))
    value = hash_u32(value, layout["representation"])
    value = hash_string(value, layout.get("discriminant"))
    value = hash_u32(value, layout["primary_tag"])
    value = hash_u32(value, layout["secondary_tag"])
    value = hash_string(value, layout.get("niche_none_pattern"))
    return (1 << 29) + value


def set_unresolved_nested_call(document):
    nodes = instruction(document, "Main")["expr0_graph"]["nodes"]
    nodes[9]["text"] = "MissingDouble"
    nodes[10]["text"] = "MissingDouble()"
    nodes[10]["call_target_name"] = "MissingDouble"


def set_repaired_param_layout(document):
    param = routine(document, "SumPair")["params"][0]
    param["abi_layout"]["fields"][1]["offset"] = 0
    param["abi_layout_id"] = layout_id(param["abi_layout"])


emit(
    "routine-order-cycle",
    lambda d: d.__setitem__(
        "routines", [d["routines"][2], d["routines"][0], d["routines"][1]]
    ),
)
emit("entrypoint-name", lambda d: routine(d, "Main").__setitem__("name", "NotMain"))
emit("array-param-type", lambda d: routine(d, "SumPair")["params"][0].__setitem__("type", "Array<Long>"))
emit("array-param-carriage", lambda d: routine(d, "SumPair")["params"][0].__setitem__("carriage", "value-result"))
emit("array-param-resource", lambda d: routine(d, "SumPair")["params"][0].__setitem__("resource", "borrowed"))
emit("array-param-pass", lambda d: routine(d, "SumPair")["params"][0].__setitem__("pass", "indirect"))
emit("missing-param-abi", lambda d: routine(d, "SumPair")["params"][0].pop("abi_layout"))
emit("repaired-param-abi", set_repaired_param_layout)
emit("nested-call-unresolved", set_unresolved_nested_call)
emit("nested-call-argument-edge", lambda d: instruction(d, "Main")["expr0_graph"]["nodes"][12].__setitem__("right", 6))
emit("array-call-argument-edge", lambda d: instruction(d, "Main")["expr0_graph"]["nodes"][14].__setitem__("right", 8))
emit("parameter-use", lambda d: instruction(d, "SumPair")["expr0_graph"]["nodes"][3].__setitem__("text", "other"))
emit("unexpected-instruction-use", lambda d: instruction(d, "SumPair").__setitem__("uses", ["values.0"]))
emit("forged-return-result", lambda d: instruction(d, "Double").__setitem__("result", "forged.1"))
emit("unreachable-callee", lambda d: routine(d, "SumPair")["blocks"][0].__setitem__("reachable", False))
emit("callee-successor", lambda d: routine(d, "Double")["blocks"][0].__setitem__("succ_true", 0))
emit("stray-instruction-abi", lambda d: instruction(d, "Main").__setitem__("abi_layout_id", 1))
