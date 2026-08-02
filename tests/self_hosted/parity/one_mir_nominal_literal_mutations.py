"""Semantic parity, variants, and falsifiers for method-free nominal literals."""

import copy
import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
mode = sys.argv[3] if len(sys.argv) > 3 else "mutate"
baseline = json.loads(source.read_text(encoding="utf-8"))


def declaration(document):
    if len(document["decls"]) != 1:
        raise RuntimeError("expected one declaration")
    return document["decls"][0]


def routine(document):
    if len(document["routines"]) != 1:
        raise RuntimeError("expected one routine")
    return document["routines"][0]


def instructions(document):
    return routine(document)["blocks"][0]["instructions"]


def graph(document, row):
    return instructions(document)[row]["expr0_graph"]


def semantic_kind(row, native=False):
    if native and row["kind"] == "class" and row["nominal_kind"] in (
            "tobject", "subject", "vessel"):
        return row["nominal_kind"]
    return row["kind"]


def graph_shape(value):
    return {
        "root": value["root"],
        "nodes": [{key: node.get(key) for key in (
            "kind", "text", "call_target_kind", "call_target_name",
            "left", "right"
        )} for node in value["nodes"]],
    }


def declaration_shape(document, native=False):
    row = declaration(document)
    return {
        "kind": semantic_kind(row, native),
        "nominal_kind": row["nominal_kind"],
        "name": row["name"],
        "fields": [{key: field[key] for key in (
            "name", "type", "field_kind"
        )} for field in row["fields"]],
        "methods": row["methods"],
    }


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    if declaration_shape(baseline) != declaration_shape(oracle, native=True):
        raise RuntimeError("native/self nominal literal declaration drifted")
    left = routine(baseline)
    right = routine(oracle)
    for key in ("name", "kind", "receiver_carriage", "generics", "params",
                "return", "source_locals"):
        if left[key] != right[key]:
            raise RuntimeError(f"native/self routine {key} drifted")
    if len(left["blocks"]) != 1 or len(right["blocks"]) != 1 or \
            left["blocks"][0]["id"] != right["blocks"][0]["id"] or \
            not left["blocks"][0]["reachable"] or \
            not right["blocks"][0]["reachable"]:
        raise RuntimeError("native/self block envelope drifted")
    left_rows = left["blocks"][0]["instructions"]
    right_rows = right["blocks"][0]["instructions"]
    if len(left_rows) != len(right_rows):
        raise RuntimeError("native/self instruction count drifted")
    for index, (lhs, rhs) in enumerate(zip(left_rows, right_rows)):
        for key in ("kind", "name", "result", "arg0", "arg1", "expr0",
                    "expr1", "source_type", "uses", "abi_type_name",
                    "abi_layout_id", "abi_layout_required", "abi_layout"):
            if lhs[key] != rhs[key]:
                raise RuntimeError(f"native/self instruction {index} {key} drifted")
        if graph_shape(lhs["expr0_graph"]) != graph_shape(rhs["expr0_graph"]):
            raise RuntimeError(f"native/self instruction {index} graph drifted")
    raise SystemExit(0)


output_dir = target


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def field(document):
    return declaration(document)["fields"][0]


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
    base = declaration(baseline)
    base_type = base["name"]
    base_field = base["fields"][0]["name"]
    base_local = routine(baseline)["source_locals"][0]["name"]
    replace_strings(document, [
        (base_type, "Packet"), (base_field, "value"),
        (base_local, "packet")
    ])


def literal_seventy_three(document):
    base = declaration(document)
    type_name = base["name"]
    field_name = base["fields"][0]["name"]
    nodes = graph(document, 0)["nodes"]
    nodes[3]["text"] = "73"
    nodes[4]["text"] = f"{field_name}: 73"
    nodes[5]["text"] = f"{type_name} {{ {field_name}: 73 }}"
    instructions(document)[0]["expr0"] = nodes[5]["text"]


def forge_abi(document):
    row = instructions(document)[0]
    row["abi_layout_id"] = 9
    row["abi_layout_required"] = True
    row["abi_layout"] = {
        "type_name": declaration(document)["name"], "layout_id": 9
    }


def append_instruction(document):
    row = copy.deepcopy(instructions(document)[1])
    row["id"] = 2
    instructions(document).append(row)


def append_identity_definition(document):
    row = copy.deepcopy(instructions(document)[0])
    row["id"] = 2
    row["result"] = f"{row['arg0']}.2"
    instructions(document).insert(1, row)


def append_source_local(document):
    source_local = copy.deepcopy(routine(document)["source_locals"][0])
    source_local["name"] = f"{source_local['name']}_copy"
    routine(document)["source_locals"].append(source_local)


emit("semantic-rename", semantic_rename)
emit("literal-seventy-three", literal_seventy_three)
emit("host-object", lambda d: (
    declaration(d).__setitem__("kind", "object"),
    declaration(d).__setitem__("nominal_kind", "object")))
emit("host-class", lambda d: (
    declaration(d).__setitem__("kind", "class"),
    declaration(d).__setitem__("nominal_kind", "class")))
emit("host-tobject", lambda d: (
    declaration(d).__setitem__("kind", "tobject"),
    declaration(d).__setitem__("nominal_kind", "tobject")))
emit("kind-drift", lambda d: declaration(d).__setitem__("kind", "class"))
emit("nominal-kind-drift", lambda d: declaration(d).__setitem__(
    "nominal_kind", "class"))
emit("host-subject", lambda d: (
    declaration(d).__setitem__("kind", "subject"),
    declaration(d).__setitem__("nominal_kind", "subject")))
emit("host-vessel", lambda d: (
    declaration(d).__setitem__("kind", "vessel"),
    declaration(d).__setitem__("nominal_kind", "vessel")))
emit("declaration-name-drift", lambda d: declaration(d).__setitem__("name", "Other"))
emit("declaration-id-zero", lambda d: declaration(d).__setitem__("source_syntax_id", 0))
emit("field-name-drift", lambda d: field(d).__setitem__("name", "other"))
emit("field-type-drift", lambda d: field(d).__setitem__("type", "Long"))
emit("field-kind-drift", lambda d: field(d).__setitem__("field_kind", "let"))
emit("field-id-collision", lambda d: field(d).__setitem__(
    "source_syntax_id", declaration(d)["source_syntax_id"]))
emit("method-tail", lambda d: declaration(d)["methods"].append(7))
emit("entrypoint-drift", lambda d: routine(d).__setitem__("name", "Start"))
emit("return-drift", lambda d: routine(d).__setitem__("return", "Int"))
emit("source-local-name-drift", lambda d: routine(d)["source_locals"][0].__setitem__("name", "other"))
emit("source-local-type-drift", lambda d: routine(d)["source_locals"][0].__setitem__("type", "Long"))
emit("constructor-type-drift", lambda d: graph(d, 0)["nodes"][0].__setitem__("text", "Other"))
emit("constructor-field-drift", lambda d: graph(d, 0)["nodes"][2].__setitem__("text", "other"))
emit("constructor-edge-drift", lambda d: graph(d, 0)["nodes"][5].__setitem__("right", 3))
emit("constructor-noncanonical-int", lambda d: graph(d, 0)["nodes"][3].__setitem__("text", "012"))
emit("definition-result-drift", lambda d: instructions(d)[0].__setitem__("result", "other.1"))
emit("definition-local-drift", lambda d: instructions(d)[0].__setitem__("arg0", "other"))
emit("definition-arg-type-drift", lambda d: instructions(d)[0].__setitem__("arg1", "Other"))
emit("definition-expr-type-drift", lambda d: instructions(d)[0].__setitem__("expr1", "Other"))
emit("definition-abi-type-drift", lambda d: instructions(d)[0].__setitem__("abi_type_name", "Other"))
emit("definition-abi-forged", forge_abi)
emit("instruction-tail", append_instruction)
emit("duplicate-identity-definition", append_identity_definition)
emit("second-source-local", append_source_local)
emit("missing-use", lambda d: instructions(d)[1].__setitem__("uses", []))
emit("duplicate-use", lambda d: instructions(d)[1].__setitem__(
    "uses", [instructions(d)[0]["result"], instructions(d)[0]["result"]]))
emit("stale-use", lambda d: instructions(d)[1].__setitem__(
    "uses", [f"{instructions(d)[0]['arg0']}.2"]))
emit("member-receiver-drift", lambda d: graph(d, 1)["nodes"][0].__setitem__("text", "other"))
emit("member-name-drift", lambda d: graph(d, 1)["nodes"][1].__setitem__("text", "other"))
emit("member-edge-drift", lambda d: graph(d, 1)["nodes"][2].__setitem__("right", 0))
emit("unreachable", lambda d: routine(d)["blocks"][0].__setitem__("reachable", False))
