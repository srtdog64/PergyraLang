"""Typed falsifying MIR mutations for the bounded Array<Int> return gate."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
output_dir = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def set_unresolved_call_target(document):
    instruction = document["routines"][1]["blocks"][0]["instructions"][0]
    instruction["expr0"] = "MissingBuild()"
    nodes = instruction["expr0_graph"]["nodes"]
    nodes[0]["text"] = "MissingBuild"
    nodes[1]["text"] = "MissingBuild()"
    nodes[1]["call_target_name"] = "MissingBuild"


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


def set_repaired_bad_field_shape(document):
    instruction = document["routines"][1]["blocks"][0]["instructions"][0]
    layout = instruction["abi_layout"]
    layout["fields"][0]["size"] = 4
    layout["fields"][0]["align"] = 4
    instruction["abi_layout_id"] = layout_id(layout)


def emit_duplicate_producer_return():
    text = json.dumps(baseline, separators=(",", ":"))
    needle = '"return":"Array<Int>"'
    if text.count(needle) != 1:
        raise RuntimeError("expected one producer return field")
    (output_dir / "duplicate-producer-return.json").write_text(
        text.replace(needle, needle + ',"return":"Void"', 1),
        encoding="utf-8",
    )


emit("routine-order-swap", lambda d: d["routines"].reverse())
emit("entrypoint-name", lambda d: d["routines"][1].__setitem__("name", "NotMain"))
emit("call-target", set_unresolved_call_target)
emit("producer-return-kind", lambda d: d["routines"][0]["blocks"][0]["instructions"][0].__setitem__("kind", "stmt"))
emit("producer-return-type", lambda d: d["routines"][0].__setitem__("return", "Void"))
emit("missing-producer-return", lambda d: d["routines"][0].pop("return"))
emit("stale-result-definition", lambda d: d["routines"][1]["blocks"][0]["instructions"][0].__setitem__("result", "values.2"))
emit("stale-use", lambda d: d["routines"][1]["blocks"][0]["instructions"][1].__setitem__("uses", ["values.2"]))
emit("abi-offset", lambda d: d["routines"][1]["blocks"][0]["instructions"][0]["abi_layout"]["fields"][1].__setitem__("offset", 0))
emit("abi-field-shape-repaired-id", set_repaired_bad_field_shape)
emit("unreachable-main", lambda d: d["routines"][1]["blocks"][0].__setitem__("reachable", False))
emit("producer-successor", lambda d: d["routines"][0]["blocks"][0].__setitem__("succ_true", 0))
emit("forged-log-result", lambda d: d["routines"][1]["blocks"][0]["instructions"][1].__setitem__("result", "forged.1"))
emit_duplicate_producer_return()
