"""Parity, permutations, variants, and falsifiers for Array<Point> flow."""

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


def graph(document, name, instruction):
    return instructions(document, name)[instruction]["expr0_graph"]


def declaration(document, name):
    rows = [row for row in document["decls"] if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one declaration named {name}")
    return rows[0]


def record(document):
    return declaration(document, "Point")


def wrapper(document):
    return declaration(document, "RecordArrayWrapper")


def method(document, name):
    rows = [row for row in wrapper(document)["methods"] if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one method named {name}")
    return rows[0]


def specialization(document, name):
    rows = [row for row in document["generic_method_specializations"]
            if row.get("callable", row.get("method")) == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one specialization for {name}")
    return rows[0]


def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def graph_shape(value):
    nodes = []
    for node in value["nodes"]:
        target_name = node.get("call_target_name")
        if node.get("call_target_kind") == "member" and target_name:
            target_name = target_name.rsplit("_", 1)[-1]
        nodes.append({
            "kind": node.get("kind"), "text": node.get("text"),
            "call_target_kind": node.get("call_target_kind"),
            "call_target_name": target_name, "left": node.get("left"),
            "right": node.get("right"),
        })
    return {"root": value["root"], "nodes": nodes}


def declaration_shapes(document):
    shapes = []
    for row in document["decls"]:
        shape = {
            "kind": row["kind"], "nominal_kind": row["nominal_kind"],
            "name": row["name"],
            "fields": [{key: item[key] for key in
                        ("name", "type", "field_kind")}
                       for item in row["fields"]],
        }
        if "methods" in row:
            shape["methods"] = sorted((item["name"], item["return"],
                                        item["callable_kind"], item["contract"])
                                       for item in row["methods"])
        shapes.append(shape)
    return sorted(shapes, key=lambda item: item["name"])


def specialization_tuples(document, self_owned):
    rows = []
    for row in document["generic_method_specializations"]:
        if self_owned:
            rows.append((row["owner"], row["callable"],
                         row["specialized_symbol"],
                         tuple(row["generic_params"]),
                         tuple(row["generic_actuals"])))
        else:
            rows.append((row["owner"], row["method"], row["symbol"],
                         tuple(row["generic_params"]),
                         tuple(row["actual_types"])))
    return sorted(rows)


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    if declaration_shapes(baseline) != declaration_shapes(oracle):
        raise RuntimeError("native/self declaration semantics drifted")
    for name in ("Wrap", "Echo", "Main"):
        left = routine(baseline, name)
        right = routine(oracle, name)
        for key in ("name", "kind", "receiver_carriage", "generics",
                    "params", "return", "source_locals"):
            if left[key] != right[key]:
                raise RuntimeError(f"native/self {name} {key} drifted")
        if left.get("owner", "") != right.get("owner", ""):
            raise RuntimeError(f"native/self {name} owner drifted")
        left_rows = left["blocks"][0]["instructions"]
        right_rows = right["blocks"][0]["instructions"]
        if len(left_rows) != len(right_rows):
            raise RuntimeError(f"native/self {name} instruction count drifted")
        for index, (left_row, right_row) in enumerate(zip(left_rows, right_rows)):
            for key in ("kind", "name", "result", "arg0", "expr0",
                        "source_type", "uses"):
                if left_row[key] != right_row[key]:
                    raise RuntimeError(f"native/self {name} {key} {index} drifted")
            if name == "Main" and index == 3:
                if left_row["arg1"] != "Point" or right_row["arg1"] is not None:
                    raise RuntimeError("native/self Point index type contract drifted")
            elif left_row["arg1"] != right_row["arg1"]:
                raise RuntimeError(f"native/self {name} arg1 {index} drifted")
            if receipt(left_row) != receipt(right_row):
                raise RuntimeError(f"native/self {name} ABI {index} drifted")
            if graph_shape(left_row["expr0_graph"]) != graph_shape(
                    right_row["expr0_graph"]):
                raise RuntimeError(f"native/self {name} graph {index} drifted")
    if specialization_tuples(baseline, True) != \
            specialization_tuples(oracle, False):
        raise RuntimeError("native/self specialization semantics drifted")
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


def rename_generics(document):
    for name, generic in (("Wrap", "U"), ("Echo", "V")):
        row = routine(document, name)
        row["generics"] = [generic]
        row["params"][1]["type"] = generic
        row["params"][1]["abi_type_name"] = generic
        row["return"] = f"Array<{generic}>" if name == "Wrap" else generic
        instructions(document, name)[0]["abi_type_name"] = row["return"]
        method(document, name)["return"] = row["return"]
        specialization(document, name)["generic_params"] = [generic]


def change_field_value(document):
    nodes = graph(document, "Main", 1)["nodes"]
    nodes[3]["text"] = "75"
    nodes[4]["text"] = "x: 75"
    nodes[5]["text"] = "Point { x: 75 }"
    instructions(document, "Main")[1]["expr0"] = nodes[5]["text"]


def collide_with_hidden_storage(document):
    wrap = routine(document, "Wrap")
    wrap["params"][1]["name"] = "pgy_array_storage"
    row = instructions(document, "Wrap")[0]
    row["expr0"] = "[pgy_array_storage]"
    row["expr0_graph"]["nodes"][1]["text"] = "pgy_array_storage"
    row["expr0_graph"]["nodes"][2]["text"] = "[pgy_array_storage]"


def replace_strings(value, replacements):
    if isinstance(value, dict):
        for key in list(value):
            value[key] = replace_strings(value[key], replacements)
    elif isinstance(value, list):
        for index in range(len(value)):
            value[index] = replace_strings(value[index], replacements)
    elif isinstance(value, str):
        for old, new in replacements:
            value = value.replace(old, new)
    return value


def combined_order(document):
    document["decls"].reverse()
    document["routines"].reverse()
    document["generic_method_specializations"].reverse()
    wrapper(document)["methods"].reverse()
    routine(document, "Main")["source_locals"].reverse()


def mutate_all_point_receipts(document, mutate):
    owners = [record(document), instructions(document, "Main")[1],
              instructions(document, "Main")[3]]
    for owner in owners:
        mutate(owner["abi_layout"])
        owner["abi_layout_id"] = layout_id(owner["abi_layout"])


def mutate_one_point_receipt(document, instruction_index, mutate):
    owner = instructions(document, "Main")[instruction_index]
    mutate(owner["abi_layout"])
    owner["abi_layout_id"] = layout_id(owner["abi_layout"])


def forge_array_receipt(document):
    row = instructions(document, "Main")[2]
    layout = copy.deepcopy(record(document)["abi_layout"])
    layout["type"] = "Array<Point>"
    row["abi_layout"] = layout
    row["abi_layout_id"] = layout_id(layout)
    row["abi_layout_required"] = True


def forge_wrapper_receipt(document):
    row = instructions(document, "Main")[0]
    layout = copy.deepcopy(record(document)["abi_layout"])
    layout["type"] = "RecordArrayWrapper"
    layout["fields"][0]["name"] = "marker"
    row["abi_layout"] = layout
    row["abi_layout_id"] = layout_id(layout)
    row["abi_layout_required"] = True


emit("routine-order-reverse", lambda d: d["routines"].reverse())
emit("specialization-order-swap", lambda d: d[
    "generic_method_specializations"].reverse())
emit("declaration-order-swap", lambda d: d["decls"].reverse())
emit("declaration-method-order-swap", lambda d: wrapper(d)["methods"].reverse())
emit("source-local-order-swap", lambda d: routine(
    d, "Main")["source_locals"].reverse())
emit("combined-order", combined_order)
emit("generic-formal-rename", rename_generics)
emit("field-value-seventy-five", change_field_value)
emit("hidden-storage-collision", collide_with_hidden_storage)
emit("collision-names", lambda d: replace_strings(
    d, [("wrapper", "printf"), ("point", "pgy_result"),
        ("result", "pgy_receiver"), ("first", "pgy_inner")]))

emit("missing-record", lambda d: d["decls"].remove(record(d)))
emit("record-kind-drift", lambda d: record(d).__setitem__("kind", "class"))
emit("wrapper-kind-drift", lambda d: wrapper(d).__setitem__("kind", "struct"))
emit("missing-method", lambda d: wrapper(d)["methods"].pop())
emit("field-type-drift", lambda d: record(d)["fields"][0].__setitem__(
    "type", "Long"))
emit("missing-specialization", lambda d: d[
    "generic_method_specializations"].pop())
emit("duplicate-specialization-ordinal", lambda d: specialization(
    d, "Wrap").__setitem__("source_call_ordinal", 0))
emit("specialization-owner-drift", lambda d: specialization(
    d, "Echo").__setitem__("owner", "Other"))
emit("outer-actual-drift", lambda d: specialization(
    d, "Echo").__setitem__("generic_actuals", ["Array<Int>"]))
emit("inner-actual-drift", lambda d: specialization(
    d, "Wrap").__setitem__("generic_actuals", ["Int"]))
emit("inner-symbol-drift", lambda d: specialization(
    d, "Wrap").__setitem__("specialized_symbol", "Other"))
emit("array-element-edge-drift", lambda d: graph(
    d, "Wrap", 0)["nodes"][2].__setitem__("right", 0))
emit("identity-body-drift", lambda d: graph(
    d, "Echo", 0)["nodes"][0].__setitem__("text", "other"))
emit("point-literal-edge-drift", lambda d: graph(
    d, "Main", 1)["nodes"][5].__setitem__("right", 3))
emit("nested-point-kind-drift", lambda d: graph(
    d, "Main", 2)["nodes"][8].__setitem__("kind", "integer_literal"))
emit("inner-target-drift", lambda d: graph(
    d, "Main", 2)["nodes"][7].__setitem__("call_target_name", "Other"))
emit("index-value-drift", lambda d: graph(
    d, "Main", 3)["nodes"][1].__setitem__("text", "1"))
emit("output-field-drift", lambda d: graph(
    d, "Main", 4)["nodes"][3].__setitem__("text", "y"))
emit("stale-result-use", lambda d: instructions(d, "Main")[2].__setitem__(
    "uses", [instructions(d, "Main")[0]["result"]]))
emit("stale-first-use", lambda d: instructions(d, "Main")[3].__setitem__(
    "uses", [instructions(d, "Main")[1]["result"]]))
emit("forged-array-receipt", forge_array_receipt)
emit("forged-wrapper-receipt", forge_wrapper_receipt)
emit("point-receipt-id-drift", lambda d: instructions(
    d, "Main")[1].__setitem__("abi_layout_id", 1))
emit("first-receipt-missing", lambda d: instructions(
    d, "Main")[3].__setitem__("abi_layout_required", False))
emit("point-offset-drift", lambda d: mutate_all_point_receipts(
    d, lambda layout: layout["fields"][0].__setitem__("offset", 4)))
emit("point-runtime-drift", lambda d: mutate_all_point_receipts(
    d, lambda layout: layout.__setitem__("runtime_fn", "other")))
emit("point-representation-drift", lambda d: mutate_all_point_receipts(
    d, lambda layout: layout.__setitem__("representation", 1)))
emit("point-only-offset-drift", lambda d: mutate_one_point_receipt(
    d, 1, lambda layout: layout["fields"][0].__setitem__("offset", 4)))
emit("first-only-field-name-drift", lambda d: mutate_one_point_receipt(
    d, 3, lambda layout: layout["fields"][0].__setitem__("name", "y")))
emit("source-identity-collision", lambda d: routine(
    d, "Echo").__setitem__("source_syntax_id", routine(d, "Wrap")[
        "source_syntax_id"]))
emit("main-name-drift", lambda d: instructions(
    d, "Main")[2].__setitem__("name", "other"))
emit("main-expr1-drift", lambda d: instructions(
    d, "Main")[2].__setitem__("expr1", "Array<Int>"))
emit("output-envelope-drift", lambda d: instructions(
    d, "Main")[4].__setitem__("machine_contact_kind", "host"))
emit("extra-root-fact", lambda d: d.__setitem__("extra", 0))
emit("unreachable-wrap", lambda d: routine(d, "Wrap")["blocks"][0].
     __setitem__("reachable", False))
