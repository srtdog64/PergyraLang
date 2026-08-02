"""Parity, permutations, and falsifiers for inferred generic scalar flow."""

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


def graph(document, name, instruction, expression=0):
    return instructions(document, name)[instruction][f"expr{expression}_graph"]


def receipt(row):
    return {key: copy.deepcopy(row[key]) for key in (
        "abi_type_name", "abi_layout_id", "abi_layout_required", "abi_layout"
    )}


def graph_shape(value):
    if value is None:
        return None
    return {
        "root": value["root"],
        "nodes": [{key: node.get(key) for key in (
            "kind", "text", "call_target_kind", "call_target_name",
            "left", "right"
        )} for node in value["nodes"]],
    }


if mode == "compare":
    oracle = json.loads(target.read_text(encoding="utf-8"))
    for document, label in ((baseline, "self"), (oracle, "native")):
        if document["decls"] != [] or len(document["routines"]) != 3:
            raise RuntimeError(f"{label} program envelope drifted")
    for name in ("Identity", "ReturnIdentity", "Main"):
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
            if name == "Main" and index == 1:
                if left_row["kind"] != "def" or left_row["name"] != "ssa-def" or \
                        not left_row["result"] or \
                        left_row["result"] == left_rows[0]["result"]:
                    raise RuntimeError("self assignment SSA identity drifted")
                if (right_row["kind"], right_row["name"],
                        right_row["result"]) != ("assign", "assign", None):
                    raise RuntimeError("native assignment residual drifted")
                for key in ("arg0", "arg1", "source_type"):
                    if left_row[key] != right_row[key]:
                        raise RuntimeError(
                            f"native/self Main assignment {key} drifted"
                        )
                if receipt(left_row) != receipt(right_row):
                    raise RuntimeError("native/self Main assignment ABI drifted")
                if graph_shape(left_row["expr0_graph"]) != \
                        graph_shape(right_row["expr1_graph"]):
                    raise RuntimeError("native/self assignment value graph drifted")
                if graph_shape(left_row["expr1_graph"]) != \
                        graph_shape(right_row["expr0_graph"]):
                    raise RuntimeError("native/self assignment target graph drifted")
                continue
            for key in ("kind", "name", "arg0", "arg1", "result",
                        "source_type"):
                if left_row[key] != right_row[key]:
                    raise RuntimeError(f"native/self {name} {key} {index} drifted")
            if receipt(left_row) != receipt(right_row):
                raise RuntimeError(f"native/self {name} ABI {index} drifted")
            for expression in (0, 1):
                key = f"expr{expression}_graph"
                if graph_shape(left_row[key]) != graph_shape(right_row[key]):
                    raise RuntimeError(
                        f"native/self {name} graph {index}/{expression} drifted"
                    )
            if name == "Main" and index == 2:
                if left_row["uses"] != [left_rows[1]["result"]] or \
                        right_row["uses"] != [right_rows[0]["result"]]:
                    raise RuntimeError("native/self latest-use ownership drifted")
            elif left_row["uses"] != right_row["uses"]:
                raise RuntimeError(f"native/self {name} uses {index} drifted")
    if len(baseline["generic_method_specializations"]) != 2:
        raise RuntimeError("self mixed-lane specialization count drifted")
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


def specialization(document, index=0):
    return document["generic_method_specializations"][index]


def renumber_specialization_owners(document):
    for row in document["generic_method_specializations"]:
        row["source_owner_syntax_id"] += 1000


def rotate_routines(document):
    document["routines"] = document["routines"][1:] + document["routines"][:1]


def combined_rotation(document):
    rotate_routines(document)
    document["generic_method_specializations"].reverse()


emit("routine-order-reverse",
     lambda d: d.__setitem__("routines", list(reversed(d["routines"]))))
emit("routine-order-rotate", rotate_routines)
emit("specialization-order-swap", lambda d: d[
    "generic_method_specializations"].reverse())
emit("combined-order-rotate", combined_rotation)
emit("specialization-owner-renumber", renumber_specialization_owners)
emit("initial-value-seven", lambda d: graph(d, "Main", 0)["nodes"][0].
     __setitem__("text", "7"))
emit("assigned-value-forty-three", lambda d: graph(d, "Main", 1)["nodes"][2].
     __setitem__("text", "43"))

emit("missing-generic-formal",
     lambda d: routine(d, "Identity").__setitem__("generics", []))
emit("duplicate-generic-formal",
     lambda d: routine(d, "Identity").__setitem__("generics", ["T", "U"]))
emit("generic-param-type-drift", lambda d: routine(d, "Identity")["params"][0].
     __setitem__("type", "Int"))
emit("generic-param-abi-drift", lambda d: routine(d, "Identity")["params"][0].
     __setitem__("abi_type_name", "Int"))
emit("generic-return-type-drift",
     lambda d: routine(d, "Identity").__setitem__("return", "Int"))
emit("generic-receiver-drift", lambda d: routine(d, "Identity").
     __setitem__("receiver_carriage", "value"))
emit("generic-body-drift", lambda d: graph(d, "Identity", 0)["nodes"][0].
     __setitem__("text", "other"))
emit("generic-return-abi-drift", lambda d: instructions(d, "Identity")[0].
     __setitem__("abi_layout_required", True))

emit("wrapper-param-type-drift", lambda d: routine(
    d, "ReturnIdentity")["params"][0].__setitem__("type", "Long"))
emit("wrapper-param-abi-drift", lambda d: routine(
    d, "ReturnIdentity")["params"][0].__setitem__("abi_type_name", "Long"))
emit("wrapper-return-drift", lambda d: routine(
    d, "ReturnIdentity").__setitem__("return", "Long"))
emit("wrapper-call-target-drift", lambda d: graph(
    d, "ReturnIdentity", 0)["nodes"][1].__setitem__("call_target_name", "Other"))
emit("wrapper-call-argument-edge-drift", lambda d: graph(
    d, "ReturnIdentity", 0)["nodes"][3].__setitem__("right", 0))
emit("wrapper-argument-name-drift", lambda d: graph(
    d, "ReturnIdentity", 0)["nodes"][2].__setitem__("text", "other"))
emit("wrapper-return-abi-drift", lambda d: instructions(
    d, "ReturnIdentity")[0].__setitem__("abi_layout_required", True))

emit("missing-specialization",
     lambda d: d["generic_method_specializations"].pop())
emit("extra-specialization", lambda d: d["generic_method_specializations"].
     append(copy.deepcopy(specialization(d))))
emit("duplicate-specialization-coordinate", lambda d: specialization(d, 1).
     __setitem__("source_lane", specialization(d, 0)["source_lane"]))
emit("specialization-lane-drift",
     lambda d: specialization(d).__setitem__("source_lane", 2))
emit("specialization-owner-equality", lambda d: specialization(d, 1).
     __setitem__("source_owner_syntax_id",
                 specialization(d, 0)["source_owner_syntax_id"]))
emit("specialization-owner-zero", lambda d: specialization(d).
     __setitem__("source_owner_syntax_id", 0))
emit("specialization-ordinal-drift", lambda d: specialization(d).
     __setitem__("source_call_ordinal", 1))
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

emit("missing-source-local", lambda d: routine(
    d, "Main").__setitem__("source_locals", []))
emit("source-local-type-drift", lambda d: routine(
    d, "Main")["source_locals"][0].__setitem__("type", "Long"))
emit("initial-kind-drift", lambda d: graph(d, "Main", 0)["nodes"][0].
     __setitem__("kind", "leaf"))
emit("initial-result-drift", lambda d: instructions(d, "Main")[0].
     __setitem__("result", "other.1"))
emit("initial-abi-drift", lambda d: instructions(d, "Main")[0].
     __setitem__("abi_layout_required", True))
emit("initial-type-text-drift", lambda d: instructions(d, "Main")[0].
     __setitem__("expr1", "Long"))
emit("assignment-source-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("source_type", "AST_LET_DECL"))
emit("assignment-result-alias", lambda d: instructions(d, "Main")[1].
     __setitem__("result", instructions(d, "Main")[0]["result"]))
emit("assignment-local-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("arg0", "other"))
emit("assignment-carriage-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("arg1", ""))
emit("assignment-target-text-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("expr1", "other"))
emit("assignment-target-drift", lambda d: graph(d, "Main", 1, 1)["nodes"][0].
     __setitem__("text", "other"))
emit("assignment-call-target-drift", lambda d: graph(d, "Main", 1)["nodes"][1].
     __setitem__("call_target_name", "Other"))
emit("assignment-call-edge-drift", lambda d: graph(d, "Main", 1)["nodes"][3].
     __setitem__("right", 0))
emit("assignment-literal-kind-drift", lambda d: graph(d, "Main", 1)["nodes"][2].
     __setitem__("kind", "leaf"))
emit("assignment-abi-drift", lambda d: instructions(d, "Main")[1].
     __setitem__("abi_layout_required", True))
emit("duplicate-result-definition", lambda d: (
    instructions(d, "Main")[1].__setitem__("result", instructions(d, "Main")[0]["result"]),
    instructions(d, "Main")[2].__setitem__("uses", [instructions(d, "Main")[0]["result"]])))
emit("foreign-latest-result", lambda d: (
    instructions(d, "Main")[1].__setitem__("result", "other.1"),
    instructions(d, "Main")[2].__setitem__("uses", ["other.1"])))
emit("output-wrapper-target-drift", lambda d: graph(d, "Main", 2)["nodes"][3].
     __setitem__("call_target_name", "Other"))
emit("output-local-drift", lambda d: graph(d, "Main", 2)["nodes"][4].
     __setitem__("text", "other"))
emit("output-call-edge-drift", lambda d: graph(d, "Main", 2)["nodes"][6].
     __setitem__("right", 4))
emit("stale-output-use", lambda d: instructions(d, "Main")[2].
     __setitem__("uses", [instructions(d, "Main")[0]["result"]]))
emit("missing-output-use", lambda d: instructions(d, "Main")[2].
     __setitem__("uses", []))
emit("output-abi-type-drift", lambda d: instructions(d, "Main")[2].
     __setitem__("abi_type_name", "Int"))
emit("output-abi-layout-drift", lambda d: instructions(d, "Main")[2].
     __setitem__("abi_layout_required", True))
emit("unreachable-main", lambda d: routine(d, "Main")["blocks"][0].
     __setitem__("reachable", False))
emit("unreachable-wrapper", lambda d: routine(d, "ReturnIdentity")["blocks"][0].
     __setitem__("reachable", False))
