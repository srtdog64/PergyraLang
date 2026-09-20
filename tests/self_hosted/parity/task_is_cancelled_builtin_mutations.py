#!/usr/bin/env python3
"""Adversarial direct-MIR variants for the IsCancelled task ABI bridge."""

import copy
import json
from pathlib import Path
import sys


def is_cancelled_call(document):
    matches = []
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                graph = instruction.get("expr0_graph")
                if not isinstance(graph, dict):
                    continue
                for node in graph.get("nodes", []):
                    if (node.get("kind") == "call" and
                            node.get("call_target_name") == "IsCancelled"):
                        matches.append(node)
    if len(matches) != 1:
        raise ValueError(
            f"expected one IsCancelled call node, found {len(matches)}")
    return matches[0]


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: mutations.py INPUT OUTPUT_DIR")
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    document = json.loads(source.read_text(encoding="utf-8-sig"))
    call = is_cancelled_call(document)
    expected = {
        "call_target_kind": "direct",
        "call_target_syntax_id": 0,
        "runtime_call_abi_id": 0,
        "binding_syntax_id": 0,
        "binding_kind": "none",
        "right": None,
    }
    for field, value in expected.items():
        if call.get(field) != value:
            raise ValueError(
                f"IsCancelled {field} was {call.get(field)!r}, expected {value!r}")
    if not isinstance(call.get("left"), int):
        raise ValueError("IsCancelled lost its callee leaf edge")

    def write(name, mutate):
        changed = copy.deepcopy(document)
        mutate(is_cancelled_call(changed))
        (output / f"{name}.json").write_text(
            json.dumps(changed, separators=(",", ":")) + "\n",
            encoding="utf-8",
            newline="\n",
        )

    write("wrong-runtime-id", lambda node:
          node.__setitem__("runtime_call_abi_id", 1))
    write("wrong-target", lambda node:
          node.__setitem__("call_target_name", "IsCancelledRenamed"))
    write("missing-callee-edge", lambda node:
          node.__setitem__("left", None))


if __name__ == "__main__":
    main()
