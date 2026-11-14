#!/usr/bin/env python3
"""Check this fixture's loop-carried edges, not allocation-order SSA suffixes."""

import copy
import json
import sys


def require(condition, message):
    if not condition:
        raise ValueError(message)


def loop_facts(document):
    routines = [row for row in document["routines"] if row["name"] == "Simulate"]
    require(len(routines) == 1, "expected one Simulate routine")
    blocks = routines[0]["blocks"]
    by_id = {block["id"]: block for block in blocks}
    require(len(by_id) == len(blocks), "duplicate block identity")
    definitions = {}
    for block in blocks:
        for row in block["instructions"]:
            result = row.get("result")
            if result:
                require(result not in definitions, "duplicate SSA definition")
                definitions[result] = (block, row)
    # This fixture's single loop follows its initialization block directly.
    # The backedge is an exact successor edge, never an ordering of SSA uses.
    entry = blocks[0]
    require(entry["reachable"] and entry.get("succ_false") is None,
            "initialization block is not one reachable entry")
    require(entry.get("succ_true") in by_id, "missing loop header edge")
    header = by_id[entry["succ_true"]]
    require(header is not entry and header["reachable"], "invalid loop header")
    predecessors = [block for block in blocks
                    if header["id"] in (block.get("succ_true"), block.get("succ_false"))]
    require(len(predecessors) == 2 and entry in predecessors,
            "loop header needs entry and exactly one backedge")
    latch = next(block for block in predecessors if block is not entry)
    require(latch is not header and latch["reachable"]
            and latch.get("succ_true") == header["id"]
            and latch.get("succ_false") is None, "invalid loop latch")
    body_id, exit_id = header.get("succ_true"), header.get("succ_false")
    require(body_id in by_id and exit_id in by_id and body_id != exit_id,
            "loop condition needs body and exit edges")
    pending, visited = [body_id], set()
    while pending:
        block_id = pending.pop()
        if block_id == header["id"] or block_id in visited:
            continue
        require(block_id in by_id and by_id[block_id]["reachable"],
                "invalid reachable body edge")
        visited.add(block_id)
        pending.extend(successor for successor in
                       (by_id[block_id].get("succ_true"), by_id[block_id].get("succ_false"))
                       if successor is not None)
    require(latch["id"] in visited and exit_id not in visited,
            "latch is not on the loop body continuation")
    phis = [row for row in header["instructions"] if row["kind"] == "phi"]
    require(len(phis) == 3 and {row["name"] for row in phis} == {"cash", "shares", "i"}
            and header["instructions"][:3] == phis, "loop state phi inventory drifted")
    facts = {}
    for phi in phis:
        name = phi["name"]
        require(isinstance(phi.get("result"), str) and phi["result"] in definitions
                and definitions[phi["result"]][1] is phi,
                f"missing unique {name} header definition")
        initial = [row for row in entry["instructions"]
                   if row["kind"] == "def" and row.get("arg0") == name]
        carried = [row for row in latch["instructions"]
                   if (row["kind"] == "phi" and row["name"] == name)
                   or (row["kind"] == "def" and row.get("arg0") == name)]
        require(len(initial) == len(carried) == 1, f"ambiguous {name} edge definition")
        initial, carried = initial[0], carried[0]
        uses = phi["uses"]
        require(len(uses) == 2 and len(set(uses)) == 2,
                f"{name} header must retain two distinct incoming values")
        require(set(uses) == {initial["result"], carried["result"]},
                f"{name} header lost its exact initialization/backedge definition")
        require(all(value in definitions for value in uses), f"undefined {name} incoming")
        require(carried["kind"] == ("def" if name == "i" else "phi"),
                f"{name} latch lost its match merge/increment")
        if name == "i":
            require(carried["uses"] == [phi["result"]], "increment lost loop index input")
        facts[name] = (phi, initial, carried)
    return header, latch, facts


def mechanics(document):
    # The checker must survive renumbering and incoming-array reordering.
    changed = copy.deepcopy(document)
    routine = next(row for row in changed["routines"] if row["name"] == "Simulate")
    rows = [row for block in routine["blocks"] for row in block["instructions"]]
    renames = {row["result"]: f"{row['result'].rsplit('.', 1)[0]}.{10000 + index}"
               for index, row in enumerate(rows) if row.get("result")}
    for row in rows:
        if row.get("result"):
            row["result"] = renames[row["result"]]
        row["uses"] = [renames.get(value, value) for value in row["uses"]]
        if row["kind"] == "phi":
            row["uses"].reverse()
    loop_facts(changed)
    for label in ("missing-input", "wrong-local", "duplicate-result", "missing-result",
                  "missing-backedge", "unreachable-latch", "duplicate-phi"):
        changed = copy.deepcopy(document)
        header, latch, facts = loop_facts(changed)
        phi, initial, carried = facts["cash"]
        if label == "missing-input":
            phi["uses"].remove(carried["result"])
        elif label == "wrong-local":
            phi["uses"] = [initial["result"], facts["shares"][2]["result"]]
        elif label == "duplicate-result":
            carried["result"] = initial["result"]
        elif label == "missing-result":
            phi["result"] = None
        elif label == "missing-backedge":
            latch["succ_true"] = None
        elif label == "unreachable-latch":
            latch["reachable"] = False
        else:
            header["instructions"].insert(0, copy.deepcopy(phi))
        try:
            loop_facts(changed)
        except ValueError:
            continue
        raise ValueError(f"checker accepted its {label} negative control")


def main():
    require(len(sys.argv) in (3, 4), "usage: phi_facts.py INPUT check|drop-backedge [OUTPUT]")
    source, mode = sys.argv[1:3]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    _, _, facts = loop_facts(document)
    if mode == "check" and len(sys.argv) == 3:
        mechanics(document)
        print("[collection-enum-phi] exact three loop-carried pairs; renumber/reorder and seven checker negatives: PASS")
    elif mode == "drop-backedge" and len(sys.argv) == 4:
        phi, _, carried = facts["cash"]
        phi["uses"].remove(carried["result"])
        with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as stream:
            json.dump(document, stream, separators=(",", ":"))
            stream.write("\n")
    else:
        raise ValueError("invalid mode or argument count")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, KeyError, TypeError, OSError) as error:
        raise SystemExit(f"[collection-enum-phi] {error}")
