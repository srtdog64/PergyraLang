#!/usr/bin/env python3
"""Falsify only the reached borrowed-fact and terminal Void admission claims."""
from copy import deepcopy
import json
from pathlib import Path
import sys


def main() -> None:
    source, directory = map(Path, sys.argv[1:])
    baseline = json.loads(source.read_text(encoding="utf-8-sig"))
    if len(baseline["routines"]) != 1:
        raise AssertionError("expected one current producer-issued foreach routine")
    routine = baseline["routines"][0]
    facts = routine["collection_ownership_facts"]
    if len(facts) != 1 or facts[0]["origin"] != "borrowed-literal" or \
            facts[0]["disposition"] != "live" or \
            facts[0]["element_ownership"] != "borrowed-elements":
        raise AssertionError("expected the existing live borrowed literal receipt")
    terminal = [(block_index, row) for block_index, block in enumerate(routine["blocks"])
                for row, instruction in enumerate(block["instructions"])
                if instruction["kind"] == "return"]
    if len(terminal) != 1:
        raise AssertionError("expected exactly one producer terminal Void return")
    block_index, row = terminal[0]
    return_source = routine["blocks"][block_index]["instructions"][row]
    if return_source["source_type"] != "AST_RETURN_VOID":
        raise AssertionError("expected the public Void return form")
    modes = ["missing-borrowed-row", "duplicate-borrowed-row", "borrowed-count",
             "borrowed-binding", "borrowed-function", "borrowed-origin",
             "borrowed-unknown", "borrowed-owned", "borrowed-retired",
             "void-operand", "void-use", "void-source", "void-graph-kind",
             "void-successor"]
    for mode in modes:
        document = deepcopy(baseline)
        current = document["routines"][0]
        fact = current["collection_ownership_facts"][0]
        block = current["blocks"][block_index]
        instruction = block["instructions"][row]
        if mode == "missing-borrowed-row":
            current["collection_ownership_facts"] = []
            current["collection_ownership_fact_count"] = 0
        elif mode == "duplicate-borrowed-row":
            current["collection_ownership_facts"].append(deepcopy(fact))
            current["collection_ownership_fact_count"] = 2
        elif mode == "borrowed-count":
            current["collection_ownership_fact_count"] = 0
        elif mode == "borrowed-binding":
            fact["binding_syntax_id"] += 100000
        elif mode == "borrowed-function":
            fact["function_syntax_id"] += 100000
        elif mode == "borrowed-origin":
            fact["origin_syntax_id"] += 100000
        elif mode == "borrowed-unknown":
            fact.update(origin="unknown", element_ownership="unknown")
        elif mode == "borrowed-owned":
            fact.update(origin="clone", element_ownership="owned-elements")
        elif mode == "borrowed-retired":
            fact["disposition"] = "retired"
        elif mode == "void-operand":
            instruction["expr0"] = "0"
        elif mode == "void-use":
            instruction["uses"] = ["names.1"]
        elif mode == "void-source":
            instruction["source_type"] = "AST_RETURN"
        elif mode == "void-graph-kind":
            instruction["expr0_graph"] = "not-a-null-graph"
        elif mode == "void-successor":
            block["succ_true"] = block["id"]
        (directory / f"{mode}.mir.json").write_text(
            json.dumps(document, separators=(",", ":")), encoding="utf-8")
    (directory / "mutations.list").write_text("\n".join(modes) + "\n", encoding="utf-8")
    print(f"[array-string-route-mutations] {len(modes)} ownership/terminal falsifiers")


if __name__ == "__main__":
    main()
