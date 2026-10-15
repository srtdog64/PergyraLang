"""Bounded malformed-MIR fixtures for admission only, never emission/runtime.

The caller must first admit the exact supplied real typed lowering control.
Origin syntax numbers are preserved: native and Pgy use distinct namespaces.
"""
import json
import sys


source_path, mode, output_path = sys.argv[1:]
with open(source_path, encoding="utf-8") as source:
    document = json.load(source)
routine = next(row for row in document["routines"] if row["name"] == "Main")
facts = routine["collection_ownership_facts"]
birth = next(row for row in facts if row["origin"] in ("map-keys", "clone"))
destination = next((row for row in facts if row["origin"] == "binding"), birth)
locals_by_id = {row["binding_syntax_id"]: row for row in routine["source_locals"]}
birth_local = locals_by_id[birth["binding_syntax_id"]]
instructions = [instruction for block in routine["blocks"] for instruction in block["instructions"]]
definition = next(row for row in instructions if row["kind"] == "def" and row["arg0"] == birth_local["name"])

if mode == "flip-final":
    destination["disposition"] = "live" if destination["disposition"] == "retired" else "retired"
elif mode in ("delete-drop", "duplicate-drop", "prior-live-final-drop", "missing-receipt", "wrong-receipt-op", "wrong-receipt-root", "wrong-receipt-source"):
    matches = [(block, ordinal, row) for block in routine["blocks"]
               for ordinal, row in enumerate(block["instructions"])
               if (row.get("collection_ownership_receipt") or {}).get("kind") == "drop"]
    assert len(matches) == 1, "real control must have one published deep-drop witness"
    block, ordinal, drop = matches[0]
    if mode == "delete-drop":
        del block["instructions"][ordinal]
    elif mode == "prior-live-final-drop":
        terminals = [row for row in routine["blocks"]
                     if row.get("reachable", False) and "succ_true" not in row and "succ_false" not in row]
        assert len(terminals) == 1 and terminals[0] is not block, "requires balanced-loop control with distinct exit"
        # Assert the real serialized control excludes the old weak zero-birth
        # refusal. Do not rewrite CFG reachability to manufacture this premise.
        blocks = routine["blocks"]
        birth_block = next(index for index, row in enumerate(blocks)
                           if any(instruction is definition for instruction in row["instructions"]))
        terminal_index = next(index for index, row in enumerate(blocks) if row is terminals[0])
        pending = [0]
        seen = set()
        while pending:
            current = pending.pop()
            if current in seen or current == birth_block:
                continue
            seen.add(current)
            assert current != terminal_index, "control has an UNBORN exit; not a prior-live-generation falsifier"
            for edge in ("succ_true", "succ_false"):
                if edge in blocks[current] and blocks[blocks[current][edge]].get("reachable", False):
                    pending.append(blocks[current][edge])
        pending = [blocks[birth_block][edge] for edge in ("succ_true", "succ_false") if edge in blocks[birth_block]]
        seen = set()
        repeated_birth = False
        while pending:
            current = pending.pop()
            if current == birth_block:
                repeated_birth = True
                break
            if current in seen or current == terminal_index:
                continue
            seen.add(current)
            for edge in ("succ_true", "succ_false"):
                if edge in blocks[current] and blocks[blocks[current][edge]].get("reachable", False):
                    pending.append(blocks[current][edge])
        assert repeated_birth, "control has no repeated static birth path"
        del block["instructions"][ordinal]
        terminal = terminals[0]
        slot = len(terminal["instructions"])
        if slot and terminal["instructions"][-1]["kind"] == "return":
            slot -= 1
        terminal["instructions"].insert(slot, drop)
    elif mode == "duplicate-drop":
        block["instructions"].insert(ordinal, json.loads(json.dumps(drop)))
    elif mode == "missing-receipt":
        drop["collection_ownership_receipt"] = None
    elif mode == "wrong-receipt-op":
        drop["collection_ownership_receipt"]["kind"] = "shallow-mutation"
    elif mode == "wrong-receipt-root":
        drop["collection_ownership_receipt"]["receiver_binding_syntax_id"] = 2147483647
    else:
        drop["collection_ownership_receipt"]["source_binding_syntax_id"] = birth["binding_syntax_id"]
elif mode == "missing-declaration":
    definition["local_ref"] = None
elif mode == "wrong-declaration":
    definition["local_ref"] = "declaration:2147483647:0"
elif mode == "wrong-declaration-role":
    definition["local_ref"] = "parameter:0:0"
elif mode == "duplicate-definition":
    block = next(block for block in routine["blocks"] if any(row is definition for row in block["instructions"]))
    block["instructions"].insert(0, json.loads(json.dumps(definition)))
elif mode == "wrong-producer":
    nodes = definition["expr0_graph"]["nodes"]
    changed = 0
    for node in nodes:
        if node.get("call_target_name") in ("MapKeys", "Clone"):
            node["call_target_name"] = "MapSize"
            changed += 1
    assert changed, "real control must contain a canonical birth call"
elif mode == "wrong-operand-type":
    if birth["origin"] == "map-keys":
        operand = next(row for row in routine["source_locals"] if row["type"].startswith("HashMap<"))
        operand["type"] = "HashMap<Int, Int>"
        if "abi_type_name" in operand:
            operand["abi_type_name"] = "HashMap<Int, Int>"
    else:
        operand = next(row for row in routine["source_locals"]
                       if row["type"] == "Array<String>" and row["binding_syntax_id"] != birth["binding_syntax_id"])
        operand["type"] = "Array<Int>"
        if "abi_type_name" in operand:
            operand["abi_type_name"] = "Array<Int>"
elif mode in ("compact-semantic-types", "spaced-semantic-types"):
    # Raw ABI packets are deliberately unchanged. Only semantic source-local
    # presentation changes; producer consistency must join their canonical type.
    for current in document["routines"]:
        for row in current["source_locals"]:
            if row["type"].startswith("HashMap<"):
                row["type"] = row["type"].replace(", ", ",")
                if mode == "spaced-semantic-types":
                    row["type"] = row["type"].replace(",", ", ")
elif mode in ("malformed-type-angle", "malformed-type-comma", "unknown-source-type", "unknown-map-value-type",
              "coherent-wrong-key-type", "coherent-wrong-value-type"):
    operand = next(row for row in routine["source_locals"] if row["type"].startswith("HashMap<"))
    replacement = {
        "malformed-type-angle": "HashMap<String,Int",
        "malformed-type-comma": "HashMap<String,,Int>",
        "unknown-source-type": "Unknown",
        "unknown-map-value-type": "HashMap<String,Unknown>",
        "coherent-wrong-key-type": "HashMap<Int,Int>",
        "coherent-wrong-value-type": "HashMap<String,String>",
    }[mode]
    operand["type"] = replacement
    if mode.startswith("coherent-"):
        receiver_definitions = [row for row in instructions if row["kind"] == "def" and row["arg0"] == operand["name"]]
        assert len(receiver_definitions) == 1
        receiver = receiver_definitions[0]
        # This control uses an unsealed scalar runtime type marker, not a
        # synthesized ABI layout receipt. Never repair a sealed physical tuple.
        assert receiver["abi_layout"] is None and not receiver["abi_layout_required"] and receiver["abi_layout_id"] == 0
        receiver["abi_type_name"] = replacement
elif mode in ("wrong-key-argument", "wrong-value-argument", "wrong-call-arity"):
    call = next(row for row in instructions if row.get("arg0") == "MapSet")
    graph = call["expr0_graph"]
    if mode == "wrong-call-arity":
        root = graph["nodes"][graph["root"]]
        graph["root"] = root["left"]
        graph["nodes"] = graph["nodes"][:graph["root"] + 1]
    else:
        argument = next(row for row in graph["nodes"] if row["kind"] ==
                        ("string_literal" if mode == "wrong-key-argument" else "integer_literal"))
        argument["kind"] = "integer_literal" if mode == "wrong-key-argument" else "string_literal"
        argument["text"] = "1" if mode == "wrong-key-argument" else '"wrong-value"'
elif mode == "wrong-call-identity":
    changed = 0
    for node in definition["expr0_graph"]["nodes"]:
        if node.get("call_target_name") in ("MapKeys", "Clone"):
            node["call_target_syntax_id"] = int(routine["source_syntax_id"])
            changed += 1
    assert changed
else:
    raise SystemExit(f"unknown owned collection mutation: {mode}")

with open(output_path, "w", encoding="utf-8", newline="\n") as output:
    json.dump(document, output, separators=(",", ":"))
    output.write("\n")
