"""Semantic variants and falsifiers for collection return/parameter carriage."""

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


def routine(document, name):
    matches = [item for item in document["routines"] if item["name"] == name]
    if len(matches) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return matches[0]


def main_instruction(document, row):
    return routine(document, "Main")["blocks"][0]["instructions"][row]


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


def build_count(document, value):
    instruction = main_instruction(document, 0)
    instruction["expr0"] = f"Build({value})"
    instruction["expr0_graph"]["nodes"][2]["text"] = str(value)


def display_only(document):
    for item in document["routines"]:
        for block in item["blocks"]:
            for instruction in block["instructions"]:
                if instruction.get("expr0") is not None:
                    instruction["expr0"] = "display text is not authority"


def rotate_routines(document):
    rows = document["routines"]
    document["routines"] = rows[1:] + rows[:1]


def rename_main_collection(document):
    main = routine(document, "Main")
    definition, consumer, length = main["blocks"][0]["instructions"]
    definition["arg0"] = "r"
    definition["result"] = "r.1"
    for instruction in (consumer, length):
        instruction["uses"] = ["r.1"]
        nodes = instruction["expr0_graph"]["nodes"]
        nodes[4]["text"] = "r"
    consumer["expr0"] = "Log(ToString(SumAll(r)))"
    length["expr0"] = "Log(ToString(ArrayLength(r)))"
    if main.get("source_locals"):
        main["source_locals"][0]["name"] = "r"


def repaired_parameter_abi(document):
    param = routine(document, "SumAll")["params"][0]
    layout = param["abi_layout"]
    layout["fields"][1]["offset"] = 16
    layout["fields"][2]["offset"] = 8
    param["abi_layout_id"] = layout_id(layout)


def wrong_producer_target(document):
    instruction = main_instruction(document, 0)
    nodes = instruction["expr0_graph"]["nodes"]
    nodes[0]["text"] = "SumAll"
    nodes[1]["text"] = "SumAll()"
    nodes[1]["call_target_name"] = "SumAll"
    instruction["expr0"] = "SumAll(4)"


def stale_return_use(document):
    returned = routine(document, "Build")["blocks"][3]["instructions"][0]
    returned["uses"] = ["r.999"]


def cross_routine_raw_collision(document):
    for row in (1, 2):
        instruction = main_instruction(document, row)
        instruction["uses"] = ["r.1"]
        instruction["expr0_graph"]["nodes"][4]["text"] = "r"


emit("alternate", lambda document: build_count(document, 5))
emit("display-only", display_only)
emit("routine-order-cycle", rotate_routines)
emit("raw-value-collision", rename_main_collection)
emit("bad-repaired-param-abi", repaired_parameter_abi)
emit("bad-call-target", wrong_producer_target)
emit("bad-return-use", stale_return_use)
emit("bad-cross-routine-raw-collision", cross_routine_raw_collision)
