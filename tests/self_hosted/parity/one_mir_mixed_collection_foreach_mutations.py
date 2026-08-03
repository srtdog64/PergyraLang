"""Structured falsifiers for mixed Int/String collection foreach."""

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


def instruction(routine, block, row):
    return routine["blocks"][block]["instructions"][row]


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


def graph_strings(routine):
    graph = instruction(routine, 3, 1)["expr0_graph"]
    graph["nodes"][1]["text"] = '"x"'
    graph["nodes"][3]["text"] = '"yy"'
    graph["nodes"][5]["text"] = '"zzz"'


def display_only(routine):
    instruction(routine, 3, 1)["expr0"] = "display text must not own values"
    instruction(routine, 5, 0)["expr0"] = "display text must not own concat"
    instruction(routine, 6, 0)["expr0"] = "display text must not own log"


def repaired_runtime(routine):
    row = instruction(routine, 3, 1)
    row["abi_layout"]["runtime_fn"] = "pgy_array_new_Int"
    row["abi_layout_id"] = layout_id(row["abi_layout"])


def repaired_inner_type(routine):
    row = instruction(routine, 3, 1)
    row["abi_layout"]["inner_c_type"] = "int32_t"
    row["abi_layout_id"] = layout_id(row["abi_layout"])


emit("iteration-order", lambda r: r["iteration_type_facts"].reverse())
emit("graph-strings", graph_strings)
emit("display-only", display_only)
emit("bad-binding", lambda r: r["iteration_type_facts"][1].__setitem__("binding_type", "Int"))
emit("bad-iterable", lambda r: r["iteration_type_facts"][1].__setitem__("iterable_type", "Array<Int>"))
emit("bad-source-local", lambda r: r["source_locals"][5].__setitem__("type", "Int"))
emit("bad-abi-type", lambda r: instruction(r, 3, 1).__setitem__("abi_type_name", "Array<Int>"))
emit("bad-abi-offset", lambda r: instruction(r, 3, 1)["abi_layout"]["fields"][1].__setitem__("offset", 16))
emit("bad-runtime", repaired_runtime)
emit("bad-inner-type", repaired_inner_type)
emit("bad-string-spine", lambda r: instruction(r, 3, 1)["expr0_graph"]["nodes"][6].__setitem__("left", 0))
emit("bad-concat-target", lambda r: instruction(r, 5, 0)["expr0_graph"]["nodes"][1].__setitem__("call_target_name", "Other"))
emit("bad-concat-edge", lambda r: instruction(r, 5, 0)["expr0_graph"]["nodes"][5].__setitem__("left", 1))
emit("bad-local-ref-id", lambda r: instruction(r, 5, 0)["expr0_local_refs"][0].__setitem__("ref", "iteration:999:0"))
emit("bad-local-ref-node", lambda r: instruction(r, 5, 0)["expr0_local_refs"][0].__setitem__("node", 2))
emit("stale-log-use", lambda r: instruction(r, 6, 0).__setitem__("uses", ["acc.1"]))
