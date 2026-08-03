"""Structured falsifiers for the ArrayLength-bounded String index receipt."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
output = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document["routines"][0])
    (output / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def find_instruction(routine, kind, source_type):
    matches = [
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction["kind"] == kind
        and instruction.get("source_type") == source_type
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one {kind}/{source_type}, got {len(matches)}")
    return matches[0]


def array_definition(routine):
    return next(
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("abi_type_name") == "Array<String>"
    )


def branch(routine):
    return find_instruction(routine, "branch", "AST_FOR_LOOP")


def assignment(routine):
    return find_instruction(routine, "def", "AST_ASSIGNMENT")


def layout_id(layout):
    modulus = 1 << 28

    def byte(value, item):
        return ((value ^ item) * 435) % modulus

    def string(value, text):
        for item in (text or "").encode("utf-8"):
            value = byte(value, item)
        return byte(value, 255)

    def number(value, item):
        item &= 0xFFFFFFFF
        for shift in (0, 8, 16, 24):
            value = byte(value, (item >> shift) & 0xFF)
        return value

    value = string(60621699, layout["type"])
    value = number(value, layout["size"])
    value = number(value, layout["align"])
    value = number(value, len(layout["fields"]))
    for field in layout["fields"]:
        value = string(value, field["name"])
        value = number(value, field["offset"])
        value = number(value, field["size"])
        value = number(value, field["align"])
    value = string(value, layout.get("runtime_fn"))
    value = string(value, layout.get("inner_c_type"))
    value = number(value, layout["representation"])
    value = string(value, layout.get("discriminant"))
    value = number(value, layout["primary_tag"])
    value = number(value, layout["secondary_tag"])
    value = string(value, layout.get("niche_none_pattern"))
    return (1 << 29) + value


def graph_values(routine):
    nodes = array_definition(routine)["expr0_graph"]["nodes"]
    nodes[1]["text"] = '"a"'
    nodes[3]["text"] = '"bb"'
    nodes[5]["text"] = '"ccc"'


def display_only(routine):
    array_definition(routine)["expr0"] = "display text cannot own storage"
    loop_init = find_instruction(routine, "loop-init", "AST_FOR_LOOP")
    loop_init["expr0"] = "display start cannot own the range"
    loop_init["expr1"] = "display stop cannot own the range"
    branch(routine)["expr0"] = "display branch cannot own the bound"
    branch(routine)["expr1"] = "display branch cannot own the bound"
    assignment(routine)["expr0"] = "display assignment cannot own the index"


def stale_collection(routine):
    array_definition(routine)["result"] = "parts.9"


def bad_capacity_layout(routine):
    row = array_definition(routine)
    fields = row["abi_layout"]["fields"]
    fields[1]["offset"], fields[2]["offset"] = (
        fields[2]["offset"],
        fields[1]["offset"],
    )
    row["abi_layout_id"] = layout_id(row["abi_layout"])


emit("graph-values", graph_values)
emit("display-only", display_only)
emit(
    "bad-range-start",
    lambda r: find_instruction(r, "loop-init", "AST_FOR_LOOP")[
        "expr0_graph"
    ]["nodes"][0].__setitem__("text", "-1"),
)
emit("bad-branch-use", lambda r: branch(r).__setitem__("uses", []))
emit(
    "bad-bound-target",
    lambda r: branch(r)["expr0_graph"]["nodes"][1].__setitem__(
        "call_target_name", "Other"
    ),
)
emit(
    "bad-bound-edge",
    lambda r: branch(r)["expr0_graph"]["nodes"][3].__setitem__("right", 0),
)
emit("bad-body-use", lambda r: assignment(r).__setitem__("uses", ["acc.4"]))
emit(
    "bad-collection-leaf",
    lambda r: assignment(r)["expr0_graph"]["nodes"][4].__setitem__(
        "text", "other"
    ),
)
emit(
    "bad-index-local",
    lambda r: assignment(r)["expr0_graph"]["nodes"][5].__setitem__(
        "text", "j"
    ),
)
emit(
    "bad-index-edge",
    lambda r: assignment(r)["expr0_graph"]["nodes"][6].__setitem__(
        "right", 4
    ),
)
emit(
    "bad-concat-target",
    lambda r: assignment(r)["expr0_graph"]["nodes"][1].__setitem__(
        "call_target_name", "Other"
    ),
)
emit(
    "bad-literal-spine",
    lambda r: array_definition(r)["expr0_graph"]["nodes"][6].__setitem__(
        "left", 0
    ),
)
emit("stale-collection", stale_collection)
emit("bad-capacity-layout", bad_capacity_layout)
emit(
    "bad-iteration-binding",
    lambda r: r["iteration_type_facts"][0].__setitem__(
        "binding_type", "String"
    ),
)
