"""Parity and falsifiers for Option<nominal> aggregate value flow."""

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


def result_slot(document, result):
    rows = [row for row in instructions(document, "Main")
            if row.get("result") == result]
    if len(rows) != 1:
        raise RuntimeError(f"expected one result slot {result}")
    return rows[0]


def returned_slot(document):
    rows = [row for row in instructions(document, "BuildPair")
            if row["kind"] == "return"]
    if len(rows) != 1:
        raise RuntimeError("expected one BuildPair return")
    return rows[0]


def declaration(document):
    if len(document["decls"]) != 1:
        raise RuntimeError("expected one Pair declaration")
    return document["decls"][0]


def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def rows_for_type(document, type_name):
    rows = []
    for owner in document["routines"]:
        for row in owner["blocks"][0]["instructions"]:
            if row.get("abi_type_name") == type_name:
                rows.append(row)
    return rows


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    left_decl = copy.deepcopy(declaration(baseline))
    right_decl = copy.deepcopy(declaration(oracle))
    expected_module = \
        "src/self_hosted/mir_lower/fixture/option_struct_value_flow.pgy"
    for row in (left_decl, right_decl):
        row.pop("source_syntax_id")
        module_path = row.pop("source_module_path", "").replace("\\", "/")
        if not module_path.endswith(expected_module):
            raise RuntimeError("Pair declaration source module drifted")
        for field in row["fields"]:
            field.pop("source_syntax_id")
    if left_decl != right_decl:
        raise RuntimeError("Pair declaration ABI receipt drifted")
    for document, label in ((baseline, "self"), (oracle, "native")):
        outer = rows_for_type(document, "Option<Pair>")
        inner = rows_for_type(document, "Pair")
        if len(outer) != 5 or len(inner) != 2:
            raise RuntimeError(f"{label} ABI receipt count drifted")
        if any(receipt(row) != receipt(outer[0]) for row in outer[1:]):
            raise RuntimeError(f"{label} Option<Pair> receipts disagree")
        expected_inner = {
            "abi_type_name": left_decl["name"],
            "abi_layout_id": left_decl["abi_layout_id"],
            "abi_layout_required": left_decl["abi_layout_required"],
            "abi_layout": left_decl["abi_layout"],
        }
        if any(receipt(row) != expected_inner for row in inner):
            raise RuntimeError(f"{label} Pair receipts disagree")
    if receipt(rows_for_type(baseline, "Option<Pair>")[0]) != \
            receipt(rows_for_type(oracle, "Option<Pair>")[0]):
        raise RuntimeError("native/self Option<Pair> receipt drifted")
    if routine(baseline, "Main")["source_locals"] != \
            routine(oracle, "Main")["source_locals"]:
        raise RuntimeError("Option<Pair> source-local inventory drifted")
    self_results = [row.get("result") for row in instructions(baseline, "Main")]
    for expected in ("picked.1", "picked.2", "picked.3",
                     "pair.1", "built.1", "unwrapped.1"):
        if self_results.count(expected) != 1:
            raise RuntimeError(f"self SSA result missing: {expected}")
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


def remove_receipt(row):
    row.pop("abi_layout")


def mutate_all_outer(document, mutate):
    for row in rows_for_type(document, "Option<Pair>"):
        mutate(row["abi_layout"])
        row["abi_layout_id"] = layout_id(row["abi_layout"])


def repair_option_tag_geometry(document):
    def mutate(layout):
        layout["align"] = 8
        layout["fields"][0]["align"] = 8
    mutate_all_outer(document, mutate)


def repair_option_payload_offset(document):
    mutate_all_outer(
        document, lambda layout: layout["fields"][1].__setitem__("offset", 0)
    )


def repair_option_tags(document):
    def mutate(layout):
        layout["primary_tag"] = 1
        layout["secondary_tag"] = 0
    mutate_all_outer(document, mutate)


def repair_pair_offset(document):
    owners = [declaration(document)]
    owners.extend(rows_for_type(document, "Pair"))
    for owner in owners:
        owner["abi_layout"]["fields"][1]["offset"] = 0
        owner["abi_layout_id"] = layout_id(owner["abi_layout"])


def producer_graph(document):
    return returned_slot(document)["expr0_graph"]


def built_graph(document):
    return result_slot(document, "built.1")["expr0_graph"]


def chained_slot(document):
    rows = [row for row in instructions(document, "Main")
            if row.get("uses") == ["built.1"] and
            row.get("expr0_graph") is not None and
            len(row["expr0_graph"]["nodes"]) == 9]
    if len(rows) != 1:
        raise RuntimeError("expected one chained built unwrap")
    return rows[0]


emit("routine-order-swap",
     lambda d: d.__setitem__("routines", list(reversed(d["routines"]))))
emit("missing-return-receipt", lambda d: remove_receipt(returned_slot(d)))
emit("missing-initial-receipt",
     lambda d: remove_receipt(result_slot(d, "picked.1")))
emit("missing-none-receipt",
     lambda d: remove_receipt(result_slot(d, "picked.2")))
emit("missing-latest-receipt",
     lambda d: remove_receipt(result_slot(d, "picked.3")))
emit("missing-built-receipt",
     lambda d: remove_receipt(result_slot(d, "built.1")))
emit("missing-pair-receipt",
     lambda d: remove_receipt(result_slot(d, "pair.1")))
emit("missing-unwrapped-receipt",
     lambda d: remove_receipt(result_slot(d, "unwrapped.1")))
emit("repaired-option-tag-geometry", repair_option_tag_geometry)
emit("repaired-option-payload-offset", repair_option_payload_offset)
emit("repaired-option-tags", repair_option_tags)
emit("repaired-pair-offset", repair_pair_offset)
emit("pair-field-order",
     lambda d: declaration(d).__setitem__(
         "fields", list(reversed(declaration(d)["fields"]))))
emit("pair-field-type",
     lambda d: declaration(d)["fields"][0].__setitem__("type", "Long"))
emit("missing-declaration-source-module-path",
     lambda d: declaration(d).pop("source_module_path"))
emit("empty-declaration-source-module-path",
     lambda d: declaration(d).__setitem__("source_module_path", ""))
emit("malformed-producer-some",
     lambda d: producer_graph(d)["nodes"][-1].__setitem__("kind", "leaf"))
emit("stale-picked-use",
     lambda d: result_slot(d, "pair.1").__setitem__("uses", ["picked.2"]))
emit("producer-call-unresolved",
     lambda d: built_graph(d)["nodes"][1].__setitem__(
         "call_target_name", "MissingPair"))
emit("stale-built-use",
     lambda d: result_slot(d, "unwrapped.1").__setitem__(
         "uses", ["picked.3"]))
emit("flattened-chained-use",
     lambda d: chained_slot(d).__setitem__("uses", ["unwrapped.1"]))
emit("unsupported-return",
     lambda d: routine(d, "BuildPair").__setitem__("return", "Result<Pair>"))
