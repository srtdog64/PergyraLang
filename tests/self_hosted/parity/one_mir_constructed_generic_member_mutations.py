"""Parity, permutations, variants, and falsifiers for constructed member flow."""

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


def declaration(document):
    return document["decls"][0]


def field(document):
    return declaration(document)["fields"][0]


def method(document, name):
    rows = [row for row in declaration(document)["methods"]
            if row["name"] == name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one declaration method named {name}")
    return rows[0]


def specialization(document, callable_name):
    rows = [row for row in document["generic_method_specializations"]
            if row.get("callable", row.get("method")) == callable_name]
    if len(rows) != 1:
        raise RuntimeError(f"expected one specialization for {callable_name}")
    return rows[0]


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
        if node.get("call_target_kind") == "member" and target_name:
            # Native records the callable while self-host records Owner_Callable.
            # Preserve Wrap/Echo identity without comparing target spellings.
            target_name = target_name.rsplit("_", 1)[-1]
        nodes.append({
            "kind": node.get("kind"), "text": node.get("text"),
            "call_target_kind": node.get("call_target_kind"),
            "call_target_name": target_name, "left": node.get("left"),
            "right": node.get("right"),
        })
    return {
        "root": value["root"],
        "nodes": nodes,
    }


def declaration_shape(document):
    row = declaration(document)
    methods = sorted((item["name"], item["return"], item["callable_kind"],
                      item["contract"]) for item in row["methods"])
    return {
        "kind": row["kind"], "nominal_kind": row["nominal_kind"],
        "name": row["name"],
        "fields": [{key: item[key] for key in ("name", "type", "field_kind")}
                   for item in row["fields"]],
        "methods": methods,
    }


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
    # Compare formal SyntaxNodeIds only after the owned cross-epoch join;
    # do not strip identity keys or mistake producer allocation for semantics.
    canonical_self = json.loads(pathlib.Path(sys.argv[4]).read_text(encoding="utf-8"))
    canonical_native = json.loads(pathlib.Path(sys.argv[5]).read_text(encoding="utf-8"))
    if declaration_shape(baseline) != declaration_shape(oracle):
        raise RuntimeError("native/self declaration semantics drifted")
    for name in ("Wrap", "Echo", "Main"):
        left = routine(baseline, name)
        right = routine(oracle, name)
        for key in ("name", "kind", "receiver_carriage", "generics",
                    "params", "return", "source_locals"):
            if key == "params":
                if routine(canonical_self, name)[key] != routine(canonical_native, name)[key]:
                    raise RuntimeError(f"canonical {name} parameter identity drifted")
                continue
            if left[key] != right[key]:
                raise RuntimeError(f"native/self {name} {key} drifted")
        if left.get("owner", "") != right.get("owner", ""):
            raise RuntimeError(f"native/self {name} owner drifted")
        left_rows = left["blocks"][0]["instructions"]
        right_rows = right["blocks"][0]["instructions"]
        if len(left_rows) != len(right_rows):
            raise RuntimeError(f"native/self {name} instruction count drifted")
        for index, (left_row, right_row) in enumerate(zip(left_rows, right_rows)):
            # expr1 is a debug rendering: native writes Option<Int: Int> while
            # self-host writes Option<Int>. ABI and specialization facts below
            # own the semantic type identity.
            for key in ("kind", "name", "result", "arg0", "arg1", "expr0",
                        "source_type", "uses"):
                if left_row[key] != right_row[key]:
                    raise RuntimeError(f"native/self {name} {key} {index} drifted")
            if receipt(left_row) != receipt(right_row):
                raise RuntimeError(f"native/self {name} ABI {index} drifted")
            if graph_shape(left_row["expr0_graph"]) != graph_shape(
                    right_row["expr0_graph"]):
                raise RuntimeError(f"native/self {name} graph {index} drifted")
    if specialization_tuples(baseline, True) != \
            specialization_tuples(oracle, False):
        raise RuntimeError("native/self semantic specialization tuples drifted")
    raise SystemExit(0)


output_dir = target


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


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
    document["routines"] = document["routines"][1:] + document["routines"][:1]
    document["generic_method_specializations"].reverse()
    declaration(document)["methods"].reverse()
    routine(document, "Main")["source_locals"].reverse()


def rename_generics(document):
    wrap = routine(document, "Wrap")
    wrap["generics"] = ["U"]
    wrap["params"][1]["type"] = "U"
    wrap["params"][1]["abi_type_name"] = "U"
    wrap["return"] = "Option<U>"
    instructions(document, "Wrap")[0]["abi_type_name"] = "Option<U>"
    method(document, "Wrap")["return"] = "Option<U>"
    specialization(document, "Wrap")["generic_params"] = ["U"]
    echo = routine(document, "Echo")
    echo["generics"] = ["V"]
    echo["params"][1]["type"] = "V"
    echo["params"][1]["abi_type_name"] = "V"
    echo["return"] = "V"
    instructions(document, "Echo")[0]["abi_type_name"] = "V"
    method(document, "Echo")["return"] = "V"
    specialization(document, "Echo")["generic_params"] = ["V"]


def semantic_rename(document):
    replace_strings(document, [
        ("Wrapper_Echo_Option_Int_", "Crate_Reflect_Option_Int_"),
        ("Wrapper_Wrap_Int", "Crate_Pack_Int"),
        ("Wrapper_Echo", "Crate_Reflect"),
        ("Wrapper_Wrap", "Crate_Pack"),
        ("Wrapper", "Crate"), ("Echo", "Reflect"), ("Wrap", "Pack"),
        ("marker", "tag"), ("wrapper", "crate"), ("result", "answer"),
    ])


def change_marker(document):
    nodes = graph(document, "Main", 0)["nodes"]
    nodes[2]["text"] = "7"
    nodes[3]["text"] = "Wrapper(7)"
    instructions(document, "Main")[0]["expr0"] = "Wrapper(7)"


def change_argument(document):
    nodes = graph(document, "Main", 1)["nodes"]
    nodes[8]["text"] = "73"
    nodes[9]["text"] = "wrapper.Wrap(73)"
    nodes[10]["text"] = "wrapper.Echo(wrapper.Wrap(73))"
    instructions(document, "Main")[1]["expr0"] = nodes[10]["text"]


def renumber_owners(document):
    for row in document["generic_method_specializations"]:
        row["source_owner_syntax_id"] += 1000


def forge_constructor_layout(document):
    row = instructions(document, "Main")[0]
    row["abi_layout_id"] = 77
    row["abi_layout_required"] = True
    row["abi_layout"] = {
        "type_name": "Wrapper", "kind": "struct", "layout_id": 77,
        "size": "4", "align": "4", "field_count": 1,
        "field_offsets": ["0"], "field_sizes": ["4"],
        "field_aligns": ["4"], "field_types": ["Int"],
    }


emit("routine-order-reverse", lambda d: d["routines"].reverse())
emit("routine-order-rotate", lambda d: d.__setitem__(
    "routines", d["routines"][1:] + d["routines"][:1]))
emit("source-local-order-swap", lambda d: routine(
    d, "Main")["source_locals"].reverse())
emit("specialization-order-swap", lambda d: d[
    "generic_method_specializations"].reverse())
emit("declaration-method-order-swap", lambda d: declaration(
    d)["methods"].reverse())
emit("combined-order", combined_order)
emit("specialization-owner-renumber", renumber_owners)
emit("generic-formal-rename", rename_generics)
emit("semantic-rename", semantic_rename)
emit("marker-value-seven", change_marker)
emit("argument-value-seventy-three", change_argument)
emit("collision-names", lambda d: replace_strings(
    d, [("result", "printf"), ("wrapper", "pgy_inner")]))
emit("field-local-same-name", lambda d: field(d).__setitem__("name", "wrapper"))

emit("declaration-kind-drift", lambda d: declaration(d).__setitem__(
    "kind", "struct"))
emit("nominal-kind-drift", lambda d: declaration(d).__setitem__(
    "nominal_kind", "struct"))
emit("missing-method", lambda d: declaration(d)["methods"].pop())
emit("method-kind-drift", lambda d: method(d, "Wrap").__setitem__(
    "callable_kind", "action"))
emit("method-contract-drift", lambda d: method(d, "Wrap")["contract"].
     __setitem__("within", "Heap"))
emit("field-type-drift", lambda d: field(d).__setitem__("type", "Long"))
emit("field-kind-drift", lambda d: field(d).__setitem__("field_kind", "let"))
emit("wrap-return-drift", lambda d: routine(d, "Wrap").__setitem__(
    "return", "T"))
emit("echo-return-drift", lambda d: routine(d, "Echo").__setitem__(
    "return", "Option<T>"))
emit("missing-generic-formal", lambda d: routine(d, "Wrap").__setitem__(
    "generics", []))
emit("receiver-carriage-drift", lambda d: routine(d, "Echo").__setitem__(
    "receiver_carriage", "ref"))
emit("value-param-type-drift", lambda d: routine(d, "Wrap")["params"][1].
     __setitem__("type", "Long"))
emit("method-return-abi-drift", lambda d: instructions(d, "Wrap")[0].
     __setitem__("abi_layout_required", True))
emit("missing-specialization", lambda d: d[
    "generic_method_specializations"].pop())
emit("extra-specialization", lambda d: d[
    "generic_method_specializations"].append(copy.deepcopy(
        specialization(d, "Wrap"))))
emit("duplicate-specialization-ordinal", lambda d: specialization(
    d, "Wrap").__setitem__("source_call_ordinal", 0))
emit("specialization-owner-disagreement", lambda d: specialization(
    d, "Wrap").__setitem__("source_owner_syntax_id", 99))
emit("specialization-lane-drift", lambda d: specialization(
    d, "Echo").__setitem__("source_lane", 0))
emit("specialization-target-drift", lambda d: specialization(
    d, "Echo").__setitem__("target_kind", "direct"))
emit("outer-actual-drift", lambda d: specialization(
    d, "Echo").__setitem__("generic_actuals", ["Option<Long>"]))
emit("inner-symbol-drift", lambda d: specialization(
    d, "Wrap").__setitem__("specialized_symbol", "Wrapper_Wrap_Long"))
emit("specialization-formal-drift", lambda d: specialization(
    d, "Wrap").__setitem__("generic_params", ["U"]))
emit("specialization-scalar-tail", lambda d: d[
    "generic_method_specializations"].append(7))
emit("some-target-drift", lambda d: graph(d, "Wrap", 0)["nodes"][1].
     __setitem__("call_target_name", "Other"))
emit("some-edge-drift", lambda d: graph(d, "Wrap", 0)["nodes"][3].
     __setitem__("right", 0))
emit("identity-body-drift", lambda d: graph(d, "Echo", 0)["nodes"][0].
     __setitem__("text", "other"))
emit("constructor-target-drift", lambda d: graph(d, "Main", 0)["nodes"][1].
     __setitem__("call_target_name", "Other"))
emit("inner-target-drift", lambda d: graph(d, "Main", 1)["nodes"][7].
     __setitem__("call_target_name", "Other"))
emit("outer-target-drift", lambda d: graph(d, "Main", 1)["nodes"][3].
     __setitem__("call_target_name", "Other"))
emit("inner-edge-drift", lambda d: graph(d, "Main", 1)["nodes"][9].
     __setitem__("right", 0))
emit("outer-edge-drift", lambda d: graph(d, "Main", 1)["nodes"][10].
     __setitem__("right", 8))
emit("unwrap-target-drift", lambda d: graph(d, "Main", 2)["nodes"][3].
     __setitem__("call_target_name", "Other"))
emit("unwrap-edge-drift", lambda d: graph(d, "Main", 2)["nodes"][5].
     __setitem__("right", 0))
emit("stale-output-use", lambda d: instructions(d, "Main")[2].
     __setitem__("uses", [instructions(d, "Main")[0]["result"]]))
emit("result-type-drift", lambda d: routine(d, "Main")["source_locals"][1].
     __setitem__("type", "Option<Long>"))
emit("result-abi-not-required", lambda d: instructions(d, "Main")[1].
     __setitem__("abi_layout_required", False))
emit("result-layout-drift", lambda d: instructions(d, "Main")[1][
    "abi_layout"]["fields"][1].__setitem__("offset", 0))
emit("constructor-physical-layout", forge_constructor_layout)
emit("unreachable-wrap", lambda d: routine(d, "Wrap")["blocks"][0].
     __setitem__("reachable", False))
emit("unreachable-main", lambda d: routine(d, "Main")["blocks"][0].
     __setitem__("reachable", False))
