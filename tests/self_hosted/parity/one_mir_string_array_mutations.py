"""Structured falsifiers for one while/read/static-set String-array plan."""

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


def instructions(routine):
    return [item for block in routine["blocks"] for item in block["instructions"]]


def unique(routine, predicate, label):
    matches = [item for item in instructions(routine) if predicate(item)]
    if len(matches) != 1:
        raise RuntimeError(f"expected one {label}, got {len(matches)}")
    return matches[0]


def array_definition(routine):
    return unique(
        routine, lambda row: row.get("abi_type_name") == "Array<String>", "array"
    )


def while_branch(routine):
    return unique(
        routine,
        lambda row: row["kind"] == "branch" and row.get("source_type") == "AST_BINARY",
        "while branch",
    )


def dynamic_log(routine):
    return unique(
        routine,
        lambda row: row.get("arg0") == "Log" and len(row.get("uses", [])) == 2,
        "dynamic Log",
    )


def literal_log(routine):
    return unique(
        routine,
        lambda row: row.get("arg0") == "Log" and len(row.get("uses", [])) == 1,
        "literal Log",
    )


def array_set(routine):
    return unique(routine, lambda row: row.get("arg0") == "ArraySet", "ArraySet")


def index_definition(routine, result):
    return unique(routine, lambda row: row.get("result") == result, result)


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


def display_only(routine):
    array_definition(routine)["expr0"] = "display cannot own the array"
    while_branch(routine)["expr0"] = "display cannot own ArrayLength"
    dynamic_log(routine)["expr0"] = "display cannot own indexed read"
    array_set(routine)["expr0"] = "display cannot own mutation"
    array_set(routine)["expr1"] = "display cannot own mutation index"
    literal_log(routine)["expr0"] = "display cannot own post-set read"


def graph_values(routine):
    nodes = array_definition(routine)["expr0_graph"]["nodes"]
    nodes[1]["text"] = '"ant"'
    nodes[3]["text"] = '"bee"'
    nodes[5]["text"] = '"cat"'


def empty_set_value(routine):
    array_set(routine)["expr0_graph"]["nodes"][0]["text"] = '""'


def graph_set_value(routine):
    array_set(routine)["expr0_graph"]["nodes"][0]["text"] = '"Bobby"'


def set_log_reordered(routine):
    block = routine["blocks"][3]
    block["instructions"][0], block["instructions"][1] = (
        block["instructions"][1],
        block["instructions"][0],
    )


def bad_layout(routine):
    row = array_definition(routine)
    fields = row["abi_layout"]["fields"]
    fields[1]["offset"], fields[2]["offset"] = fields[2]["offset"], fields[1]["offset"]
    row["abi_layout_id"] = layout_id(row["abi_layout"])


emit("display-only", display_only)
emit("graph-values", graph_values)
emit("empty-set-value", empty_set_value)
emit("graph-set-value", graph_set_value)
emit("set-log-reordered", set_log_reordered)
emit("bad-branch-index-use", lambda r: while_branch(r).__setitem__("uses", ["i.1", "names.1"]))
emit("bad-branch-collection-use", lambda r: while_branch(r).__setitem__("uses", ["i.3", "i.1"]))
emit("bad-length-target", lambda r: while_branch(r)["expr0_graph"]["nodes"][2].__setitem__("call_target_name", "ArrayCapacity"))
emit("bad-length-edge", lambda r: while_branch(r)["expr0_graph"]["nodes"][4].__setitem__("right", 0))
emit("bad-log-index-use", lambda r: dynamic_log(r).__setitem__("uses", ["names.1", "i.1"]))
emit("bad-log-index-edge", lambda r: dynamic_log(r)["expr0_graph"]["nodes"][2].__setitem__("right", 0))
emit("bad-while-init", lambda r: index_definition(r, "i.1")["expr0_graph"]["nodes"][0].__setitem__("text", "-1"))
emit("bad-while-step", lambda r: index_definition(r, "i.6")["expr0_graph"]["nodes"][1].__setitem__("text", "-1"))
emit("bad-set-receiver", lambda r: array_set(r).__setitem__("uses", ["i.3"]))
emit("bad-set-index-oob", lambda r: array_set(r)["expr1_graph"]["nodes"][0].__setitem__("text", "3"))
emit("bad-set-index-negative", lambda r: array_set(r)["expr1_graph"]["nodes"][0].__setitem__("text", "-1"))
emit("bad-set-value-kind", lambda r: array_set(r)["expr0_graph"]["nodes"][0].update({"kind": "integer_literal", "text": "7"}))
emit("bad-post-read-oob", lambda r: literal_log(r)["expr0_graph"]["nodes"][1].__setitem__("text", "3"))
emit("bad-guard-edge", lambda r: r["blocks"][1].__setitem__("succ_true", 3))
emit("bad-guard-bypass", lambda r: r["blocks"][3].__setitem__("succ_true", 2))
emit("bad-array-layout", bad_layout)
emit("stale-collection", lambda r: array_definition(r).__setitem__("result", "names.9"))
