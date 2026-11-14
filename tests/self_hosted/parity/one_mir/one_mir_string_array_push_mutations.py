"""Structured falsifiers for the bounded String ArrayPush receipt."""

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


def pushes(routine):
    rows = [item for item in instructions(routine) if item.get("arg0") == "ArrayPush"]
    if len(rows) != 3:
        raise RuntimeError(f"expected three pushes, got {len(rows)}")
    return rows


def while_branch(routine):
    return unique(
        routine,
        lambda row: row.get("kind") == "branch"
        and row.get("source_type") == "AST_BINARY",
        "while branch",
    )


def concat_assignment(routine):
    return unique(
        routine,
        lambda row: row.get("source_type") == "AST_ASSIGNMENT"
        and row.get("arg0") == "acc",
        "concat assignment",
    )


def length_log(routine):
    return unique(
        routine,
        lambda row: row.get("arg0") == "Log"
        and "ArrayLength" in (row.get("expr0") or ""),
        "length log",
    )


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
    array_definition(routine)["expr0"] = "display cannot own empty storage"
    for row in pushes(routine):
        row["expr0"] = "display cannot own push value"
    while_branch(routine)["expr0"] = "display cannot own ArrayLength"
    concat_assignment(routine)["expr0"] = "display cannot own indexed concat"
    length_log(routine)["expr0"] = "display cannot own final length"


def graph_value(routine):
    pushes(routine)[1]["expr0_graph"]["nodes"][0]["text"] = '"B"'


def push_order(routine):
    block = routine["blocks"][0]["instructions"]
    block[1], block[3] = block[3], block[1]


def bad_layout(routine):
    row = array_definition(routine)
    fields = row["abi_layout"]["fields"]
    fields[1]["offset"], fields[2]["offset"] = (
        fields[2]["offset"],
        fields[1]["offset"],
    )
    row["abi_layout_id"] = layout_id(row["abi_layout"])


def push_before_definition(routine):
    block = routine["blocks"][0]["instructions"]
    block[0], block[1] = block[1], block[0]


def move_last_push(routine, block):
    entry = routine["blocks"][0]["instructions"]
    row = entry.pop(3)
    routine["blocks"][block]["instructions"].insert(0, row)


def intermediate_length(routine):
    row = routine["blocks"][3]["instructions"].pop(1)
    routine["blocks"][0]["instructions"].insert(2, row)


emit("display-only", display_only)
emit("graph-value", graph_value)
emit("push-order", push_order)
emit("bad-empty-graph", lambda r: array_definition(r).__setitem__("expr0_graph", None))
emit("bad-empty-kind", lambda r: array_definition(r)["expr0_graph"]["nodes"][0].__setitem__("kind", "leaf"))
emit("bad-push-receiver", lambda r: pushes(r)[0].__setitem__("uses", ["acc.1"]))
emit("bad-push-use-missing", lambda r: pushes(r)[0].__setitem__("uses", []))
emit("bad-push-use-duplicate", lambda r: pushes(r)[0].__setitem__("uses", ["words.1", "words.1"]))
emit("bad-push-value-kind", lambda r: pushes(r)[1]["expr0_graph"]["nodes"][0].update({"kind": "integer_literal", "text": "7"}))
emit("bad-push-route", lambda r: pushes(r)[0].__setitem__("arg0", "ArraySet"))
emit("bad-push-before-definition", push_before_definition)
emit("bad-entry-backedge", lambda r: r["blocks"][3].__setitem__("succ_true", 0))
emit("bad-intermediate-length", intermediate_length)
emit("bad-loop-push", lambda r: move_last_push(r, 2))
emit("bad-branch-push", lambda r: move_last_push(r, 3))
emit("bad-branch-length-target", lambda r: while_branch(r)["expr0_graph"]["nodes"][2].__setitem__("call_target_name", "ArrayCapacity"))
emit("bad-branch-length-edge", lambda r: while_branch(r)["expr0_graph"]["nodes"][4].__setitem__("right", 0))
emit("bad-final-length-target", lambda r: length_log(r)["expr0_graph"]["nodes"][3].__setitem__("call_target_name", "ArrayCapacity"))
emit("bad-final-length-edge", lambda r: length_log(r)["expr0_graph"]["nodes"][6].__setitem__("right", 4))
emit("bad-final-length-use", lambda r: length_log(r).__setitem__("uses", ["acc.7"]))
emit("bad-array-layout", bad_layout)
emit("stale-collection", lambda r: array_definition(r).__setitem__("result", "words.9"))
