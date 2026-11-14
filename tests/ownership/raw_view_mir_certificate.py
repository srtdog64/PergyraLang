#!/usr/bin/env python3
"""Fail-closed projection of an admitted scalar Array/Slice MIR slice to Rocq.

This is a bounded consumer, not a production admission route or a proof of the
compiler. Names select a routine; every ownership join uses source binding IDs.
Unsupported instructions/graphs refuse instead of guessing a provenance fact.
"""
import argparse
import copy
import itertools
import json
from pathlib import Path


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def scalar(value):
    require(-(2**31) <= value < 2**31, "projection requires nonoverflowing Int arithmetic")
    return value


class Graph:
    def __init__(self, data):
        require(isinstance(data, dict), "missing expression graph")
        self.nodes = data["nodes"]
        self.root = data["root"]
        require(isinstance(self.root, int) and 0 <= self.root < len(self.nodes), "invalid graph root")
        for pos, node in enumerate(self.nodes):
            require(isinstance(node.get("source_binding_syntax_id"), int), "missing native binding identity")
            for edge in ("left", "right"):
                child = node[edge]
                require(child is None or isinstance(child, int) and 0 <= child < pos, "invalid graph edge")

    def binding(self, pos):
        node = self.nodes[pos]
        require(node["kind"] == "leaf" and node["source_binding_syntax_id"] > 0, "missing leaf provenance")
        return node["source_binding_syntax_id"]

    def integer(self, pos):
        require(self.nodes[pos]["kind"] == "integer_literal", "expected literal scalar index/value")
        return scalar(int(self.nodes[pos]["text"]))

    def array(self, pos):
        node = self.nodes[pos]
        if node["kind"] == "array_literal":
            return []
        require(node["kind"] == "array_element", "missing fresh array allocation")
        return self.array(node["left"]) + [self.integer(node["right"])]

    def slice(self):
        pos, args = self.root, []
        while self.nodes[pos]["kind"] == "call_argument":
            node = self.nodes[pos]
            args.insert(0, self.integer(node["right"]))
            pos = node["left"]
        node = self.nodes[pos]
        require(node["kind"] == "call" and node["call_target_kind"] == "member"
                and node["call_target_name"] == "Slice" and node["call_target_syntax_id"] == 0,
                "not the typed Array.Slice operation")
        member = self.nodes[node["left"]]
        require(member["kind"] == "member_access" and len(args) == 2, "invalid Slice receiver/arity")
        require(all(x >= 0 for x in args), "negative Slice range")
        return self.binding(member["left"]), *args

    def location(self, pos, state):
        node = self.nodes[pos]
        require(node["kind"] == "index", "expected checked indexed access")
        binding, index = self.binding(node["left"]), self.integer(node["right"])
        require(binding in state["ranges"], "unissued owner/view binding")
        start, count = state["ranges"][binding]
        require(0 <= index < count and start + count <= len(state["cells"]), "out-of-bounds view access")
        return binding, index, start + index

    def expr(self, pos, state):
        node = self.nodes[pos]
        kind = node["kind"]
        if kind == "integer_literal":
            value = self.integer(pos)
            return f"(MConst ({value})%Z)", value
        if kind == "index":
            binding, index, physical = self.location(pos, state)
            return f"(MLoad {binding} {index})", state["cells"][physical]
        require(kind in ("add", "subtract"), "unsupported or escaping scalar expression")
        left, x = self.expr(node["left"], state)
        right, y = self.expr(node["right"], state)
        value = scalar(x + y if kind == "add" else x - y)
        return f"({'MAdd' if kind == 'add' else 'MSub'} {left} {right})", value


def project(document, routine_name, expected):
    routines = [r for r in document["routines"] if r["name"] == routine_name]
    require(len(routines) == 1, "ambiguous selected routine")
    routine = routines[0]
    blocks = {b["id"]: b for b in routine["blocks"]}
    require(len(blocks) == len(routine["blocks"]) and 0 in blocks, "invalid CFG identities")
    locals_by_id = {}
    for local in routine["source_locals"]:
        binding = local["binding_syntax_id"]
        require(binding > 0 and binding not in locals_by_id, "ambiguous local binding")
        locals_by_id[binding] = local["type"]
    params = routine["params"]
    require(len(params) <= 4 and all(p["type"] == "Bool" for p in params), "unsupported branch parameter")
    choices = list(itertools.product((True, False), repeat=len(params)))
    require(len(choices) == len(expected), "each boolean execution must have an external expected result")
    paths = []
    for choice, wanted in zip(choices, expected):
        state = {"cells": [], "ranges": {}, "owner": None, "initial": None, "ssa": None}
        bools = {p["source_syntax_id"]: v for p, v in zip(params, choice)}
        code, visited, current, answer = [], set(), 0, None
        while current is not None:
            require(current in blocks and current not in visited, "unsupported cyclic or missing CFG block")
            visited.add(current)
            block = blocks[current]
            require(block["reachable"], "unreachable execution block")
            cleanups = [i for i in block["instructions"] if i["kind"] == "cleanup"]
            next_block, returned, branched = None, False, False
            for inst in block["instructions"]:
                kind = inst["kind"]
                if kind == "cleanup":
                    continue  # MIR retirement is evaluated after its return expression.
                require(not returned and not branched, "instruction follows a terminator")
                binding = inst["binding_syntax_id"]
                if kind == "def":
                    typ = inst["abi_type_name"]
                    require(binding > 0 and locals_by_id.get(binding) == typ and binding not in state["ranges"],
                            "DEF is missing its typed binding owner")
                    graph = Graph(inst["expr0_graph"])
                    layout = inst["abi_layout"]
                    require(inst["abi_layout_id"] > 0 and layout["type"] == typ, "missing typed ABI layout")
                    if typ == "Array<Int>":
                        require(state["owner"] is None and layout["runtime_fn"] == "pgy_array_new_Int"
                                and layout["inner_c_type"] == "int32_t", "unsupported allocation layout")
                        state["cells"] = graph.array(graph.root)
                        state["initial"] = list(state["cells"])
                        state["owner"], state["ssa"] = binding, inst["result"]
                        state["ranges"][binding] = (0, len(state["cells"]))
                        code.append(f"(MControl (RIEnterRaw {binding} true))")
                    else:
                        require(typ == "Slice<Int>" and layout["runtime_fn"] == "pgy_slice_get_Int", "unsupported DEF")
                        source, start, count = graph.slice()
                        require(source == state["owner"], "Slice origin is not the issued allocation owner")
                        require(start + count <= len(state["cells"]), "Slice exceeds owner backing")
                        state["ranges"][binding] = (start, count)
                        code.append(f"(MSlice {source} {binding} {start} {count})")
                elif kind == "assign":
                    left, right = Graph(inst["expr0_graph"]), Graph(inst["expr1_graph"])
                    view, index, physical = left.location(left.root, state)
                    expr, value = right.expr(right.root, state)
                    state["cells"][physical] = value
                    code.append(f"(MStore {view} {index} {expr})")
                elif kind == "branch":
                    graph = Graph(inst["expr0_graph"])
                    param = graph.binding(graph.root)
                    require(param in bools, "branch has no admitted formal identity")
                    next_block = block["succ_true"] if bools[param] else block["succ_false"]
                    branched = True
                elif kind == "return":
                    graph = Graph(inst["expr0_graph"])
                    answer, actual = graph.expr(graph.root, state)
                    require(actual == wanted, "projected memory result differs from expected execution")
                    require(len(cleanups) == 1, "return requires exactly one automatic retirement")
                    cleanup = cleanups[0]
                    require(cleanup["collection_cleanup"] == {
                        "binding_syntax_id": state["owner"], "kind": 3, "target": None}, "wrong retirement owner/edge")
                    require(cleanup["binding_syntax_id"] == state["owner"] and cleanup["arg0"] == state["ssa"]
                            and cleanup["uses"] == [state["ssa"]], "retirement lost its owner SSA")
                    abi = cleanup["runtime_call_abi"]
                    require(abi == {"owner": "MIRResource", "id": 1147550603, "domain": "native-resource",
                        "type": "Array<Int>", "operation": "ReleaseStorage", "symbol": "pgy_array_drop_Int",
                        "target_kind": "function", "materialization": "mir_abi_resource_row",
                        "call_shape": "container_ptr_to_void"}, "unadmitted retirement ABI")
                    returned = True
                else:
                    raise ValueError(f"unsupported MIR instruction: {kind}")
            require(returned or branched, "unterminated projected block")
            require(returned or not cleanups, "unsupported premature retirement")
            current = None if returned else next_block
        require(answer is not None and state["owner"] is not None, "missing closed memory execution")
        paths.append({"code": code, "initial": state["initial"], "owner": state["owner"],
                      "answer": answer, "expected": wanted, "blocks": sorted(visited)})
    require(set().union(*(set(p["blocks"]) for p in paths)) ==
            {b["id"] for b in blocks.values() if b["reachable"]}, "unprojected reachable CFG path")
    return paths


def certificate(paths):
    out = ["Require Import RawViewMemoryBridge RawPointerOwnershipClean RawPointerOwnershipTrace.",
           "Require Import Stdlib.Lists.List Stdlib.ZArith.ZArith.", "Import ListNotations."]
    for i, p in enumerate(paths):
        cells = "; ".join(f"({v})%Z" for v in p["initial"])
        out += [f"Definition initial_{i} := initial_memory 4096 1 {p['owner']} [{cells}].",
                f"Definition code_{i} := [{'; '.join(p['code'])}].",
                f"Example mir_memory_result_{i} : exists after,",
                f"  run_memory code_{i} initial_{i} = Some after /\\",
                f"  memory_eval after {p['answer']} = Some ({p['expected']})%Z.",
                "Proof. eexists; split; vm_compute; reflexivity. Qed.",
                f"Example mir_memory_retirement_{i} : exists after retired,",
                f"  run_memory code_{i} initial_{i} = Some after /\\",
                "  automatic_raw_exit Return (memory_raw after) = Some retired /\\ RawRetired retired.",
                f"Proof. destruct mir_memory_result_{i} as [after [Hr Hvalue]].",
                f"  destruct (checked_memory_trace_has_one_release 4096 1 {p['owner']} [{cells}] code_{i} after Return Hr)",
                "    as [retired [He [Hret Hrest]]].",
                "  exists after, retired. auto. Qed."]
    return "\n".join(out) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mir", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--routine", default="Work")
    parser.add_argument("--expected", default="75,100")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    document = json.loads(args.mir.read_text(encoding="utf-8"))
    expected = list(map(int, args.expected.split(",")))
    paths = project(document, args.routine, expected)
    if args.self_test:
        def work(doc):
            return next(r for r in doc["routines"] if r["name"] == args.routine)

        mutations = []
        for mode in ("missing_source", "wrong_source", "missing_def", "bad_range", "missing_cleanup", "duplicate_cleanup", "wrong_abi"):
            bad = copy.deepcopy(document)
            rows = [i for b in work(bad)["blocks"] for i in b["instructions"]]
            view = next(i for i in rows if i.get("abi_type_name") == "Slice<Int>" and i["kind"] == "def")
            if mode in ("missing_source", "wrong_source"):
                leaf = view["expr0_graph"]["nodes"][0]
                leaf["source_binding_syntax_id"] = 0 if mode == "missing_source" else view["binding_syntax_id"]
            elif mode == "missing_def":
                view["binding_syntax_id"] = 0
            elif mode == "bad_range":
                view["expr0_graph"]["nodes"][-2]["text"] = "999"
            else:
                block = next(b for b in work(bad)["blocks"] if any(i["kind"] == "cleanup" for i in b["instructions"]))
                cleanup = next(i for i in block["instructions"] if i["kind"] == "cleanup")
                if mode == "missing_cleanup":
                    block["instructions"].remove(cleanup)
                elif mode == "duplicate_cleanup":
                    block["instructions"].append(copy.deepcopy(cleanup))
                else:
                    cleanup["runtime_call_abi"]["symbol"] = "pgy_array_drop_String"
            mutations.append((mode, bad))
        for label, bad in mutations:
            try:
                project(bad, args.routine, expected)
            except ValueError:
                continue
            raise ValueError(f"mutation was admitted: {label}")
        print(f"certificate projection: {len(paths)} CFG paths; {len(mutations)} corruptions refused")
    args.output.write_text(certificate(paths), encoding="utf-8")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, IndexError, TypeError) as error:
        raise SystemExit(f"raw-view-certificate: {error}") from error
