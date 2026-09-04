"""Falsifying mutations for the nested value-struct argument gate."""

import copy
import json
import os
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


def declaration(document, name):
    rows = [row for row in document["decls"] if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one declaration named {name}")
    return rows[0]


def normalized_source_module_path(value):
    if not isinstance(value, str) or not value:
        raise RuntimeError("declaration source module path is invalid")
    value = value.replace("\\", "/")
    root = os.environ.get("PGY_SELFHOST_REPO_ROOT", "").replace("\\", "/").rstrip("/")
    if root:
        prefix = root + "/"
        windows_root = len(root) > 1 and root[1] == ":"
        matches = value.casefold().startswith(prefix.casefold()) if windows_root else value.startswith(prefix)
        if matches:
            value = value[len(prefix):]
    if value.startswith("/") or (len(value) > 1 and value[1] == ":"):
        raise RuntimeError("declaration source module path escapes repository")
    parts = pathlib.PurePosixPath(value).parts
    if not parts or any(part in (".", "..") for part in parts):
        raise RuntimeError("declaration source module path escapes repository")
    return "/".join(parts)


def consume_source_syntax_id(row, seen):
    value = row.pop("source_syntax_id")
    if type(value) is not int or value <= 0:
        raise RuntimeError("compared source identity is not a positive integer")
    if value in seen:
        raise RuntimeError("compared source identity is duplicated")
    seen.add(value)


def normalized_declaration(row, seen):
    result = copy.deepcopy(row)
    result["source_module_path"] = normalized_source_module_path(result.get("source_module_path"))
    consume_source_syntax_id(result, seen)
    for field in result["fields"]:
        consume_source_syntax_id(field, seen)
    return result


def normalized_parameters(rows, seen):
    result = copy.deepcopy(rows)
    for row in result:
        consume_source_syntax_id(row, seen)
    return result


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    baseline_ids, oracle_ids = set(), set()
    for name in ("Vec2", "Line"):
        if normalized_declaration(declaration(baseline, name), baseline_ids) != \
                normalized_declaration(declaration(oracle, name), oracle_ids):
            raise RuntimeError(f"{name} declaration ABI receipt drifted")
    for name in ("Twice", "Width"):
        actual = normalized_parameters(routine(baseline, name)["params"], baseline_ids)
        expected = normalized_parameters(routine(oracle, name)["params"], oracle_ids)
        if actual != expected:
            raise RuntimeError(f"{name} parameter ABI receipt drifted")
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


def repair_line_offset(document):
    decl = declaration(document, "Line")
    param = routine(document, "Width")["params"][0]
    for owner in (decl, param):
        owner["abi_layout"]["fields"][1]["offset"] = 4
        owner["abi_layout_id"] = layout_id(owner["abi_layout"])


def repair_vec2_offset(document):
    decl = declaration(document, "Vec2")
    decl["abi_layout"]["fields"][1]["offset"] = 0
    decl["abi_layout_id"] = layout_id(decl["abi_layout"])


def crosswire_param(document):
    source_decl = declaration(document, "Vec2")
    param = routine(document, "Width")["params"][0]
    param["abi_layout"] = copy.deepcopy(source_decl["abi_layout"])
    param["abi_layout_id"] = source_decl["abi_layout_id"]
    param["abi_layout"]["type"] = "Line"
    param["abi_layout_id"] = layout_id(param["abi_layout"])


emit("routine-order-cycle", lambda d: d.__setitem__(
    "routines", [d["routines"][2], d["routines"][0], d["routines"][1]]
))
emit("declaration-order-cycle", lambda d: d.__setitem__(
    "decls", [d["decls"][1], d["decls"][0]]
))
emit("routine-declaration-order-cycle", lambda d: (
    d.__setitem__("routines", [d["routines"][2], d["routines"][0], d["routines"][1]]),
    d.__setitem__("decls", [d["decls"][1], d["decls"][0]])
))
emit("declaration-id-duplicate", lambda d: declaration(d, "Line").__setitem__(
    "source_syntax_id", declaration(d, "Vec2")["source_syntax_id"]
))
emit("missing-declaration-abi", lambda d: declaration(d, "Line").pop("abi_layout"))
emit("line-field-order", lambda d: declaration(d, "Line").__setitem__(
    "fields", list(reversed(declaration(d, "Line")["fields"]))
))
emit("vec2-field-type", lambda d: declaration(d, "Vec2")["fields"][0].__setitem__("type", "Long"))
emit("repaired-line-layout-offset", repair_line_offset)
emit("repaired-vec2-layout-offset", repair_vec2_offset)
emit("missing-line-param-abi", lambda d: routine(d, "Width")["params"][0].pop("abi_layout"))
emit("line-param-receipt-crosswire", crosswire_param)
emit("line-param-carriage", lambda d: routine(d, "Width")["params"][0].__setitem__("carriage", "value-result"))
emit("line-param-pass", lambda d: routine(d, "Width")["params"][0].__setitem__("pass", "indirect"))
emit("width-call-unresolved", lambda d: instruction(d, "Main")["expr0_graph"]["nodes"][3].__setitem__("call_target_name", "MissingWidth"))
emit("twice-call-argument-edge", lambda d: instruction(d, "Main")["expr0_graph"]["nodes"][13].__setitem__("right", 17))
emit("width-call-argument-edge", lambda d: instruction(d, "Main")["expr0_graph"]["nodes"][37].__setitem__("right", 21))
emit("width-member-path", lambda d: instruction(d, "Width")["expr0_graph"]["nodes"][1].__setitem__("text", "start"))
emit("twice-operation", lambda d: instruction(d, "Twice")["expr0_graph"]["nodes"][2].__setitem__("kind", "add"))
