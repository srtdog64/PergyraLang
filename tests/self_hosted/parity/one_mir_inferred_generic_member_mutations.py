"""Parity, permutations, and falsifiers for inferred generic member flow."""

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

def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def graph_shape(value):
    if value is None:
        return None
    nodes = []
    for node in value["nodes"]:
        target_name = node.get("call_target_name")
        if node.get("call_target_kind") == "member":
            target_name = "<owned-member-target>"
        nodes.append({
            "kind": node.get("kind"), "text": node.get("text"),
            "call_target_kind": node.get("call_target_kind"),
            "call_target_name": target_name, "left": node.get("left"),
            "right": node.get("right"),
        })
    return {"root": value["root"], "nodes": nodes}


def native_semantic_declaration_kind(row):
    if row["kind"] == "class" and row["nominal_kind"] == "vessel":
        return "vessel"
    return row["kind"]
def declaration_shape(document, native=False):
    if len(document["decls"]) != 1:
        raise RuntimeError("expected one declaration")
    row = document["decls"][0]
    return {
        "kind": native_semantic_declaration_kind(row) if native else row["kind"],
        "nominal_kind": row["nominal_kind"],
        "name": row["name"],
        "fields": [{key: field[key] for key in ("name", "type", "field_kind")}
                   for field in row["fields"]],
        "methods": row["methods"],
    }


def specialization_tuples(document, self_owned):
    rows = []
    for row in document["generic_method_specializations"]:
        if self_owned:
            rows.append((row["owner"], row["callable"],
                         row["specialized_symbol"], tuple(row["generic_params"]),
                         tuple(row["generic_actuals"])))
        else:
            rows.append((row["owner"], row["method"], row["symbol"],
                         tuple(row["generic_params"]),
                         tuple(row["actual_types"])))
    return sorted(rows)


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    if declaration_shape(baseline) != declaration_shape(oracle, native=True):
        raise RuntimeError("native/self member nominal semantics drifted")
    for name in ("Echo", "Main"):
        left = routine(baseline, name)
        right = routine(oracle, name)
        for key in ("name", "kind", "receiver_carriage", "generics",
                    "params", "return", "source_locals"):
            if left[key] != right[key]:
                raise RuntimeError(f"native/self {name} {key} drifted")
        if left.get("owner", "") != right.get("owner", ""):
            raise RuntimeError(f"native/self {name} owner drifted")
        if len(left["blocks"]) != 1 or len(right["blocks"]) != 1 or \
                left["blocks"][0]["id"] != right["blocks"][0]["id"] or \
                not left["blocks"][0]["reachable"] or \
                not right["blocks"][0]["reachable"]:
            raise RuntimeError(f"native/self {name} block envelope drifted")
        left_rows = left["blocks"][0]["instructions"]
        right_rows = right["blocks"][0]["instructions"]
        if len(left_rows) != len(right_rows):
            raise RuntimeError(f"native/self {name} instruction count drifted")
        for index, (left_row, right_row) in enumerate(zip(left_rows, right_rows)):
            for key in ("kind", "name", "result", "arg0", "arg1",
                        "expr0", "expr1", "source_type", "uses"):
                if left_row[key] != right_row[key]:
                    raise RuntimeError(f"native/self {name} {key} {index} drifted")
            if receipt(left_row) != receipt(right_row):
                raise RuntimeError(f"native/self {name} ABI {index} drifted")
            if graph_shape(left_row["expr0_graph"]) != \
                    graph_shape(right_row["expr0_graph"]):
                raise RuntimeError(f"native/self {name} graph {index} drifted")
    left_specs = specialization_tuples(baseline, True)
    right_specs = specialization_tuples(oracle, False)
    if len(left_specs) != 2 or left_specs != right_specs:
        raise RuntimeError("native/self semantic specialization tuples drifted")
    if len({row["source_call_syntax_id"] for row in
            oracle["generic_method_specializations"]}) != 2:
        raise RuntimeError("native specialization call identities drifted")
    raise SystemExit(0)


output_dir = target


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def specialization(document, index=0):
    return document["generic_method_specializations"][index]


def declaration(document):
    return document["decls"][0]


def method(document):
    return declaration(document)["methods"][0]


def field(document):
    return declaration(document)["fields"][0]


def renumber_owners(document):
    for row in document["generic_method_specializations"]:
        row["source_owner_syntax_id"] += 1000


def combined_order(document):
    document["routines"].reverse()
    document["generic_method_specializations"].reverse()


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


def semantic_rename(document):
    replace_strings(document, [
        ("Box_Echo_Int", "Crate_Reflect_Int"),
        ("Box_Echo", "Crate_Reflect"), ("Box", "Crate"),
        ("Echo", "Reflect"), ("marker", "tag"),
        ("result", "answer"), ("box", "crate"),
    ])


def collision_names(document):
    replace_strings(document, [("result", "printf"),
                               ("box", "pgy_inner")])


def forge_constructor_layout(document):
    row = instructions(document, "Main")[0]
    row["abi_layout_id"] = 77
    row["abi_layout_required"] = True
    row["abi_layout"] = {
        "type_name": "Box", "kind": "struct", "layout_id": 77,
        "size": "4", "align": "4", "field_count": 1,
        "field_offsets": ["0"], "field_sizes": ["4"],
        "field_aligns": ["4"], "field_types": ["Int"],
    }


def change_marker_value(document):
    nodes = graph(document, "Main", 0)["nodes"]
    nodes[2]["text"] = "7"
    nodes[3]["text"] = "Box(7)"
    instructions(document, "Main")[0]["expr0"] = "Box(7)"


def change_argument_value(document):
    nodes = graph(document, "Main", 1)["nodes"]
    nodes[8]["text"] = "73"
    nodes[9]["text"] = "box.Echo(73)"
    nodes[10]["text"] = "box.Echo(box.Echo(73))"
    instructions(document, "Main")[1]["expr0"] = nodes[10]["text"]


emit("routine-order-swap", lambda d: d["routines"].reverse())
emit("source-local-order-swap", lambda d: routine(
    d, "Main")["source_locals"].reverse())
emit("specialization-order-swap", lambda d: d[
    "generic_method_specializations"].reverse())
emit("combined-order-swap", combined_order)
emit("specialization-owner-renumber", renumber_owners)
emit("marker-value-seven", change_marker_value)
emit("argument-value-seventy-three", change_argument_value)
emit("semantic-rename", semantic_rename)
emit("collision-names", collision_names)
emit("field-local-same-name", lambda d: field(d).__setitem__("name", "box"))
emit("host-kind-subject", lambda d: (
    declaration(d).__setitem__("kind", "subject"),
    declaration(d).__setitem__("nominal_kind", "subject")))
emit("declaration-kind-drift", lambda d: declaration(d).
     __setitem__("kind", "struct"))
emit("nominal-kind-drift", lambda d: declaration(d).
     __setitem__("nominal_kind", "struct"))
emit("declaration-name-drift", lambda d: declaration(d).
     __setitem__("name", "Other"))
emit("declaration-physical-layout", lambda d: declaration(d).
     __setitem__("abi_layout_id", 9))
emit("field-name-empty", lambda d: field(d).__setitem__("name", ""))
emit("field-type-drift", lambda d: field(d).__setitem__("type", "Long"))
emit("field-kind-drift", lambda d: field(d).__setitem__("field_kind", "let"))
emit("method-name-drift", lambda d: method(d).__setitem__("name", "Other"))
emit("method-return-drift", lambda d: method(d).__setitem__("return", "Int"))
emit("method-kind-drift", lambda d: method(d).
     __setitem__("callable_kind", "action"))
emit("method-contract-drift", lambda d: method(d)["contract"].
     __setitem__("within", "Heap"))
emit("method-source-id-collision", lambda d: routine(d, "Echo").
     __setitem__("source_syntax_id", declaration(d)["source_syntax_id"]))
emit("field-source-id-collision", lambda d: field(d).
     __setitem__("source_syntax_id", routine(d, "Echo")["source_syntax_id"]))

emit("routine-owner-drift", lambda d: routine(d, "Echo").
     __setitem__("owner", "Other"))
emit("receiver-carriage-drift", lambda d: routine(d, "Echo").
     __setitem__("receiver_carriage", "ref"))
emit("receiver-carriage-value-drift", lambda d: routine(d, "Echo").
     __setitem__("receiver_carriage", "value"))
emit("missing-generic-formal", lambda d: routine(d, "Echo").
     __setitem__("generics", []))
emit("duplicate-generic-formal", lambda d: routine(d, "Echo").
     __setitem__("generics", ["T", "U"]))
emit("receiver-name-drift", lambda d: routine(d, "Echo")["params"][0].
     __setitem__("name", "this"))
emit("receiver-type-forged", lambda d: routine(d, "Echo")["params"][0].
     __setitem__("type", "Box"))
emit("receiver-pass-drift", lambda d: routine(d, "Echo")["params"][0].
     __setitem__("pass", "borrow"))
emit("receiver-abi-forged", lambda d: routine(d, "Echo")["params"][0].
     __setitem__("abi_type_name", "Box"))
emit("value-param-type-drift", lambda d: routine(d, "Echo")["params"][1].
     __setitem__("type", "Int"))
emit("value-param-carriage-drift", lambda d: routine(
    d, "Echo")["params"][1].__setitem__("carriage", "ref"))
emit("value-param-abi-drift", lambda d: routine(d, "Echo")["params"][1].
     __setitem__("abi_type_name", "Int"))
emit("routine-return-drift", lambda d: routine(d, "Echo").
     __setitem__("return", "Int"))
emit("method-body-drift", lambda d: graph(d, "Echo", 0)["nodes"][0].
     __setitem__("text", "other"))
emit("method-return-abi-drift", lambda d: instructions(d, "Echo")[0].
     __setitem__("abi_layout_required", True))
emit("method-generic-scalar-tail", lambda d: routine(d, "Echo")["generics"].
     append(7))
emit("method-param-scalar-tail", lambda d: routine(d, "Echo")["params"].
     append(7))
emit("main-generic-scalar-tail", lambda d: routine(d, "Main")["generics"].
     append(7))
emit("main-param-scalar-tail", lambda d: routine(d, "Main")["params"].append(7))
emit("source-local-scalar-tail", lambda d: routine(
    d, "Main")["source_locals"].append(7))
emit("field-scalar-tail", lambda d: declaration(d)["fields"].append(7))
emit("method-scalar-tail", lambda d: declaration(d)["methods"].append(7))
emit("parallel-scalar-tail", lambda d: d["parallel_capture_boundaries"].append(7))

emit("missing-specialization", lambda d: d[
    "generic_method_specializations"].pop())
emit("extra-specialization", lambda d: d[
    "generic_method_specializations"].append(copy.deepcopy(specialization(d))))
emit("duplicate-specialization-coordinate", lambda d: specialization(d, 1).
     __setitem__("source_call_ordinal", 0))
emit("specialization-owner-zero", lambda d: specialization(d).
     __setitem__("source_owner_syntax_id", 0))
emit("specialization-owner-disagreement", lambda d: specialization(d, 1).
     __setitem__("source_owner_syntax_id", 99))
emit("specialization-lane-drift", lambda d: specialization(d).
     __setitem__("source_lane", 0))
emit("specialization-target-drift", lambda d: specialization(d).
     __setitem__("target_kind", "direct"))
emit("specialization-owner-drift", lambda d: specialization(d).
     __setitem__("owner", "Other"))
emit("specialization-callable-drift", lambda d: specialization(d).
     __setitem__("callable", "Other"))
emit("specialization-symbol-drift", lambda d: specialization(d).
     __setitem__("specialized_symbol", "Box_Echo_Long"))
emit("specialization-formal-drift", lambda d: specialization(d).
     __setitem__("generic_params", ["U"]))
emit("specialization-actual-drift", lambda d: specialization(d).
     __setitem__("generic_actuals", ["Long"]))
emit("specialization-scalar-tail", lambda d: d[
    "generic_method_specializations"].append(7))
emit("specialization-formal-tail", lambda d: specialization(d)[
    "generic_params"].append(7))
emit("specialization-actual-tail", lambda d: specialization(d)[
    "generic_actuals"].append(7))

emit("constructor-target-drift", lambda d: graph(d, "Main", 0)["nodes"][1].
     __setitem__("call_target_name", "Other"))
emit("constructor-edge-drift", lambda d: graph(d, "Main", 0)["nodes"][3].
     __setitem__("right", 0))
emit("constructor-result-drift", lambda d: instructions(d, "Main")[0].
     __setitem__("result", "other.1"))
emit("constructor-physical-layout", forge_constructor_layout)
emit("nested-receiver-drift", lambda d: graph(d, "Main", 1)["nodes"][0].
     __setitem__("text", "other"))
emit("nested-method-drift", lambda d: graph(d, "Main", 1)["nodes"][1].
     __setitem__("text", "Other"))
emit("inner-target-kind-drift", lambda d: graph(d, "Main", 1)["nodes"][7].
     __setitem__("call_target_kind", "direct"))
emit("outer-target-name-drift", lambda d: graph(d, "Main", 1)["nodes"][3].
     __setitem__("call_target_name", "Other"))
emit("inner-argument-edge-drift", lambda d: graph(d, "Main", 1)["nodes"][9].
     __setitem__("right", 0))
emit("outer-argument-edge-drift", lambda d: graph(d, "Main", 1)["nodes"][10].
     __setitem__("right", 8))
emit("nested-root-drift", lambda d: graph(d, "Main", 1).
     __setitem__("root", 9))
emit("nested-use-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("uses", []))
emit("nested-result-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("result", "other.1"))
emit("output-local-drift", lambda d: graph(d, "Main", 2)["nodes"][2].
     __setitem__("text", "other"))
emit("output-edge-drift", lambda d: graph(d, "Main", 2)["nodes"][3].
     __setitem__("right", 0))
emit("stale-output-use", lambda d: instructions(d, "Main")[2].
     __setitem__("uses", [instructions(d, "Main")[0]["result"]]))
emit("output-abi-drift", lambda d: instructions(d, "Main")[2].
     __setitem__("abi_type_name", "Int"))
emit("source-local-type-drift", lambda d: routine(
    d, "Main")["source_locals"][0].__setitem__("type", "Long"))
emit("unreachable-main", lambda d: routine(d, "Main")["blocks"][0].
     __setitem__("reachable", False))
emit("unreachable-method", lambda d: routine(d, "Echo")["blocks"][0].
     __setitem__("reachable", False))
