#!/usr/bin/env python3
"""Mutate one Result<Bool> MIR receipt without changing the positive input."""

import copy
import json
import pathlib
import sys


def result_bool_instructions(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if instruction.get("abi_type_name") == "Result<Bool>":
                    yield instruction


def mutate(document, kind):
    result = copy.deepcopy(document)
    instructions = list(result_bool_instructions(result))
    if not instructions:
        raise SystemExit("Result<Bool> instruction is missing")
    instruction = instructions[0]
    if kind == "missing-layout":
        instruction["abi_layout"] = None
    elif kind == "layout-id":
        instruction["abi_layout_id"] += 1
    elif kind == "payload-width":
        layout = instruction.get("abi_layout")
        if not isinstance(layout, dict):
            raise SystemExit("Result<Bool> layout is missing")
        layout["fields"][1]["size"] = 4
        layout["fields"][1]["align"] = 4
    elif kind == "runtime-symbol":
        layout = instruction.get("abi_layout")
        if not isinstance(layout, dict):
            raise SystemExit("Result<Bool> layout is missing")
        layout["runtime_fn"] = "pgy_result_ok_Int"
    elif kind == "return-type":
        targets = [
            routine for routine in result.get("routines", [])
            if routine.get("return") == "Result<Bool>"
        ]
        if len(targets) != 1:
            raise SystemExit("Result<Bool> routine identity is ambiguous")
        targets[0]["return"] = "Result<Int>"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    return result


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source = pathlib.Path(sys.argv[1])
    output = pathlib.Path(sys.argv[3])
    document = json.loads(source.read_text(encoding="utf-8"))
    mutated = mutate(document, sys.argv[2])
    output.write_text(
        json.dumps(mutated, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
