"""Parity and falsifying mutations for nominal aggregate value flow."""

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


def slot(document, routine_name, source_type, arg0=None):
    rows = [row for row in instructions(document, routine_name)
            if row["source_type"] == source_type and
            (arg0 is None or row.get("arg0") == arg0)]
    if len(rows) != 1:
        raise RuntimeError(f"expected one {routine_name}/{source_type}/{arg0} slot")
    return rows[0]


def returned_slot(document):
    rows = [row for row in instructions(document, "BuildPair")
            if row["kind"] == "return"]
    if len(rows) != 1:
        raise RuntimeError("expected one BuildPair return slot")
    return rows[0]


def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def declaration(document):
    if len(document["decls"]) != 1:
        raise RuntimeError("expected one declaration")
    return document["decls"][0]


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    left_decl, right_decl = copy.deepcopy(declaration(baseline)), copy.deepcopy(declaration(oracle))
    for row in (left_decl, right_decl):
        row.pop("source_syntax_id")
        for field in row["fields"]:
            field.pop("source_syntax_id")
    if left_decl != right_decl:
        raise RuntimeError("Pair declaration ABI receipt drifted")
    if receipt(returned_slot(baseline)) != receipt(returned_slot(oracle)):
        raise RuntimeError("BuildPair return ABI receipt drifted")
    semantic_slots = [
        ("Main", "AST_LET_DECL", "pair"),
        ("Main", "AST_ASSIGNMENT", "pair"),
        ("Main", "AST_LET_DECL", "built"),
    ]
    for owner, source_type, arg0 in semantic_slots:
        if receipt(slot(baseline, owner, source_type, arg0)) != \
                receipt(slot(oracle, owner, source_type, arg0)):
            raise RuntimeError(f"{owner}/{source_type}/{arg0} ABI receipt drifted")
    if routine(baseline, "Main")["source_locals"] != \
            routine(oracle, "Main")["source_locals"]:
        raise RuntimeError("Pair source-local type inventory drifted")
    self_main = instructions(baseline, "Main")
    native_main = instructions(oracle, "Main")
    if self_main[1]["kind"] != "def" or self_main[1]["result"] != "pair.2" or \
            self_main[2]["uses"] != ["pair.2"]:
        raise RuntimeError("self latest Pair SSA use is not explicit")
    if native_main[1]["kind"] != "assign" or native_main[1]["result"] is not None:
        raise RuntimeError("native residual assignment shape drifted")
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
    for key in ("runtime_fn", "inner_c_type"):
        value = string(value, layout.get(key))
    value = u32(value, layout["representation"])
    value = string(value, layout.get("discriminant"))
    value = u32(value, layout["primary_tag"])
    value = u32(value, layout["secondary_tag"])
    value = string(value, layout.get("niche_none_pattern"))
    return (1 << 29) + value


def remove_receipt(row):
    row.pop("abi_layout")


def repair_all_offsets(document):
    owners = [declaration(document)]
    owners.extend([
        returned_slot(document),
        slot(document, "Main", "AST_LET_DECL", "pair"),
        slot(document, "Main", "AST_ASSIGNMENT", "pair"),
        slot(document, "Main", "AST_LET_DECL", "built"),
    ])
    for owner in owners:
        owner["abi_layout"]["fields"][1]["offset"] = 0
        owner["abi_layout_id"] = layout_id(owner["abi_layout"])


emit("routine-order-swap", lambda d: d.__setitem__("routines", list(reversed(d["routines"]))))
emit("missing-return-receipt", lambda d: remove_receipt(returned_slot(d)))
emit("wrong-return-receipt-id", lambda d: returned_slot(d).__setitem__("abi_layout_id", 1))
emit("missing-initial-receipt", lambda d: remove_receipt(slot(d, "Main", "AST_LET_DECL", "pair")))
emit("missing-assignment-receipt", lambda d: remove_receipt(slot(d, "Main", "AST_ASSIGNMENT", "pair")))
emit("missing-result-receipt", lambda d: remove_receipt(slot(d, "Main", "AST_LET_DECL", "built")))
emit("stale-latest-use", lambda d: slot(d, "Main", "AST_LET_DECL", "built").__setitem__("uses", ["pair.1"]))
emit("repaired-all-layout-offsets", repair_all_offsets)
emit("producer-call-unresolved", lambda d: slot(d, "Main", "AST_LET_DECL", "built")["expr0_graph"]["nodes"][1].__setitem__("call_target_name", "MissingPair"))
emit("producer-call-member", lambda d: slot(d, "Main", "AST_LET_DECL", "built")["expr0_graph"]["nodes"][3].__setitem__("text", "left"))
emit("result-member-path", lambda d: slot(d, "Main", "AST_CALL")["expr0_graph"]["nodes"][6].__setitem__("text", "left"))
emit("pair-field-order", lambda d: declaration(d).__setitem__("fields", list(reversed(declaration(d)["fields"]))) )
emit("pair-field-type", lambda d: declaration(d)["fields"][0].__setitem__("type", "Long"))
emit("extra-call-target", lambda d: slot(d, "Main", "AST_CALL")["expr0_graph"]["nodes"][2].update({"call_target_kind": "direct", "call_target_name": "built"}))
