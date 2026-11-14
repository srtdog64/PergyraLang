"""Parity, permutations, and falsifiers for inferred generic Pair flow."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else "mutate"
baseline = json.loads(source.read_text(encoding="utf-8"))


def routine(document, name):
    rows = [row for row in document["routines"] if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return rows[0]


def instructions(document, name):
    return routine(document, name)["blocks"][0]["instructions"]


def declaration(document):
    if len(document["decls"]) != 1:
        raise RuntimeError("expected one Pair declaration")
    return document["decls"][0]


def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def normalized_declaration(document):
    row = copy.deepcopy(declaration(document))
    row.pop("source_syntax_id")
    for field in row["fields"]:
        field.pop("source_syntax_id")
    return row


def graph_shape(graph):
    return {
        "root": graph["root"],
        "nodes": [{key: node.get(key) for key in (
            "kind", "text", "call_target_kind", "call_target_name",
            "left", "right"
        )} for node in graph["nodes"]],
    }


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    if normalized_declaration(baseline) != normalized_declaration(oracle):
        raise RuntimeError("Pair declaration ABI receipt drifted")
    for document, label in ((baseline, "self"), (oracle, "native")):
        rows = [row for row in instructions(document, "Main")
                if row.get("abi_type_name") == "Pair"]
        if len(rows) != 1:
            raise RuntimeError(f"{label} Pair receipt count drifted")
        expected = {
            "abi_type_name": declaration(document)["name"],
            "abi_layout_id": declaration(document)["abi_layout_id"],
            "abi_layout_required": declaration(document)["abi_layout_required"],
            "abi_layout": declaration(document)["abi_layout"],
        }
        if receipt(rows[0]) != expected:
            raise RuntimeError(f"{label} Pair receipt disagrees")
    for name in ("Identity", "Main"):
        left = routine(baseline, name)
        right = routine(oracle, name)
        for key in ("name", "kind", "receiver_carriage", "generics",
                    "params", "return", "source_locals"):
            if left[key] != right[key]:
                raise RuntimeError(f"native/self {name} {key} drifted")
        left_rows = left["blocks"][0]["instructions"]
        right_rows = right["blocks"][0]["instructions"]
        if len(left_rows) != len(right_rows):
            raise RuntimeError(f"native/self {name} instruction count drifted")
        for index, (left_row, right_row) in enumerate(zip(left_rows, right_rows)):
            if graph_shape(left_row["expr0_graph"]) != \
                    graph_shape(right_row["expr0_graph"]):
                raise RuntimeError(f"native/self {name} graph {index} drifted")
            if left_row["uses"] != right_row["uses"]:
                raise RuntimeError(f"native/self {name} uses {index} drifted")
    if len(baseline["generic_method_specializations"]) != 2:
        raise RuntimeError("self inferred specialization count drifted")
    if oracle["generic_method_specializations"] != []:
        raise RuntimeError("native oracle unexpectedly owns specialization rows")
    raise SystemExit(0)


output_dir = target


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def layout_id(layout):
    modulus = 1 << 28

    def byte(value, item):
        return ((value ^ item) * 435) % modulus

    def string(value, text):
        for item in (text or "").encode("utf-8"):
            value = byte(value, item)
        return byte(value, 255)

    def u32(value, number):
        for shift in (0, 8, 16, 24):
            value = byte(value, (number >> shift) & 255)
        return value

    value = string(60621699, layout["type"])
    value = u32(value, layout["size"])
    value = u32(value, layout["align"])
    value = u32(value, len(layout["fields"]))
    for field in layout["fields"]:
        value = string(value, field["name"])
        value = u32(value, field["offset"])
        value = u32(value, field["size"])
        value = u32(value, field["align"])
    value = string(value, layout.get("runtime_fn"))
    value = string(value, layout.get("inner_c_type"))
    value = u32(value, layout["representation"])
    value = string(value, layout.get("discriminant"))
    value = u32(value, layout["primary_tag"])
    value = u32(value, layout["secondary_tag"])
    value = string(value, layout.get("niche_none_pattern"))
    return (1 << 29) + value


def specialization(document, index=0):
    return document["generic_method_specializations"][index]


def identity_graph(document):
    return instructions(document, "Identity")[0]["expr0_graph"]


def initial_graph(document):
    return instructions(document, "Main")[0]["expr0_graph"]


def output_graph(document):
    return instructions(document, "Main")[1]["expr0_graph"]


def repair_pair_offset(document):
    owners = [declaration(document), instructions(document, "Main")[0]]
    for owner in owners:
        owner["abi_layout"]["fields"][1]["offset"] = 0
        owner["abi_layout_id"] = layout_id(owner["abi_layout"])


def renumber_specialization_owner(document):
    for row in document["generic_method_specializations"]:
        row["source_owner_syntax_id"] += 1000


emit("routine-order-swap",
     lambda d: d.__setitem__("routines", list(reversed(d["routines"]))))
emit("specialization-order-swap", lambda d: d.__setitem__(
    "generic_method_specializations",
    list(reversed(d["generic_method_specializations"]))))
emit("combined-order-swap", lambda d: (
    d.__setitem__("routines", list(reversed(d["routines"]))),
    d.__setitem__("generic_method_specializations",
                  list(reversed(d["generic_method_specializations"])))) )
emit("specialization-owner-renumber", renumber_specialization_owner)
emit("missing-generic-formal",
     lambda d: routine(d, "Identity").__setitem__("generics", []))
emit("duplicate-generic-formal",
     lambda d: routine(d, "Identity").__setitem__("generics", ["T", "U"]))
emit("generic-param-drift", lambda d: routine(d, "Identity")["params"][0].
     __setitem__("type", "Int"))
emit("generic-param-abi-drift", lambda d: routine(d, "Identity")["params"][0].
     __setitem__("abi_type_name", "Int"))
emit("generic-return-drift",
     lambda d: routine(d, "Identity").__setitem__("return", "Int"))
emit("generic-receiver-drift", lambda d: routine(d, "Identity").
     __setitem__("receiver_carriage", "value"))
emit("generic-body-drift",
     lambda d: identity_graph(d)["nodes"][0].__setitem__("text", "other"))
emit("generic-return-abi-drift", lambda d: instructions(d, "Identity")[0].
     __setitem__("abi_layout_required", True))
emit("missing-specialization", lambda d: d["generic_method_specializations"].pop())
emit("extra-specialization", lambda d: d["generic_method_specializations"].
     append(copy.deepcopy(specialization(d))))
emit("duplicate-specialization-coordinate", lambda d: specialization(d, 1).
     __setitem__("source_call_ordinal", specialization(d, 0)["source_call_ordinal"]))
emit("specialization-lane-drift",
     lambda d: specialization(d).__setitem__("source_lane", 0))
emit("specialization-owner-id-disagreement", lambda d: specialization(d, 1).
     __setitem__("source_owner_syntax_id",
                 specialization(d, 0)["source_owner_syntax_id"] + 1))
emit("specialization-target-drift",
     lambda d: specialization(d).__setitem__("target_kind", "member"))
emit("specialization-owner-drift",
     lambda d: specialization(d).__setitem__("owner", "Box"))
emit("specialization-callable-drift",
     lambda d: specialization(d).__setitem__("callable", "Other"))
emit("specialization-formal-drift", lambda d: specialization(d).
     __setitem__("generic_params", ["U"]))
emit("specialization-actual-drift", lambda d: specialization(d).
     __setitem__("generic_actuals", ["Long"]))
emit("specialization-symbol-drift", lambda d: specialization(d).
     __setitem__("specialized_symbol", "Identity_Long"))
emit("graph-call-target-drift", lambda d: initial_graph(d)["nodes"][11].
     __setitem__("call_target_name", "Other"))
emit("flattened-generic-call", lambda d: initial_graph(d)["nodes"][4].
     __setitem__("kind", "leaf"))
emit("call-argument-edge-drift", lambda d: initial_graph(d)["nodes"][13].
     __setitem__("right", 5))
emit("literal-kind-drift", lambda d: initial_graph(d)["nodes"][12].
     __setitem__("kind", "leaf"))
emit("missing-pair-receipt", lambda d: instructions(d, "Main")[0].
     pop("abi_layout"))
emit("repaired-pair-offset", repair_pair_offset)
emit("pair-field-order", lambda d: declaration(d).__setitem__(
    "fields", list(reversed(declaration(d)["fields"]))))
emit("pair-field-type",
     lambda d: declaration(d)["fields"][0].__setitem__("type", "Long"))
emit("stale-output-use", lambda d: instructions(d, "Main")[1].
     __setitem__("uses", ["pair.2"]))
emit("missing-output-use", lambda d: instructions(d, "Main")[1].
     __setitem__("uses", []))
emit("output-member-drift",
     lambda d: output_graph(d)["nodes"][6].__setitem__("text", "left"))
emit("unreachable-main", lambda d: routine(d, "Main")["blocks"][0].
     __setitem__("reachable", False))
