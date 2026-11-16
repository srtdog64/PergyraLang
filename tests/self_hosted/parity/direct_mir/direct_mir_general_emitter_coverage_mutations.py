"""MIR mutations that must keep the general direct-MIR emitter refusing.

never-callee-returns: the Never routine `Fail` returns normally instead of
    exiting, so a non-Void routine that ends by calling it has a path with no
    value. The mutation swaps Fail's `Exit` statement for the Void return
    statement of `Note`, which the fixture declares for this purpose.
"""
import copy
import json
import sys


def routine(document, name):
    found = [row for row in document["routines"] if row.get("name") == name]
    if len(found) != 1:
        raise SystemExit(f"fixture has {len(found)} routines named {name}")
    return found[0]


def never_callee_returns(document):
    fail = routine(document, "Fail")
    note = routine(document, "Note")
    exits = [row for row in fail["blocks"][-1]["instructions"]
             if (row.get("expr0") or "").startswith("Exit(")]
    returns = [row for row in note["blocks"][-1]["instructions"]
               if row.get("kind") == "return"]
    if len(exits) != 1 or len(returns) != 1:
        raise SystemExit("fixture lost the Exit or the Void return")
    instructions = fail["blocks"][-1]["instructions"]
    position = instructions.index(exits[0])
    replacement = copy.deepcopy(returns[0])
    replacement["id"] = exits[0]["id"]
    instructions[position] = replacement


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:4]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    if kind == "never-callee-returns":
        never_callee_returns(document)
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8") as stream:
        json.dump(document, stream, separators=(",", ":"))


if __name__ == "__main__":
    main()
