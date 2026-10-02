"""Change only one formal source ID, using real declarations as donors."""
import json
import sys


def exact_one(rows, description):
    if len(rows) != 1:
        raise SystemExit(f"fixture must have exactly one {description}")
    return rows[0]


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routines = document.get("routines", [])
    owner = exact_one([row for row in routines
                       if row.get("name") == "ReleaseOwnedString"], "owner")
    donor = exact_one([row for row in routines
                       if row.get("name") == "OtherStringFormal"], "donor")
    formal = exact_one(owner.get("params", []), "owning formal")
    foreign = exact_one(donor.get("params", []), "foreign formal")
    if (formal.get("name") != "value" or foreign.get("name") != "value" or
            formal.get("type") != "String" or foreign.get("type") != "String" or
            formal.get("carriage") != "owner-handle"):
        raise SystemExit("fixture no longer has the exact same-name/type donor")
    definition = exact_one(
        [row for block in owner.get("blocks", [])
         for row in block.get("instructions", [])
         if row.get("kind") == "def" and row.get("source_type") == "AST_LET_DECL"
         and row.get("arg0") == "owned_values" and row.get("expr0") == "[value]"],
        "ordinary owning literal definition")
    receiver = exact_one([row for row in owner.get("source_locals", [])
                          if row.get("name") == "owned_values"], "receiver")
    graph = definition.get("expr0_graph", {})
    nodes = graph.get("nodes", [])
    if (graph.get("root") != 2 or len(nodes) != 3 or
            nodes[0].get("kind") != "array_literal" or
            nodes[1].get("kind") != "leaf" or nodes[1].get("text") != "value" or
            nodes[1].get("binding_kind") != "formal_parameter" or
            nodes[1].get("binding_ordinal") != 0 or
            nodes[2].get("kind") != "array_element" or
            nodes[2].get("left") != 0 or nodes[2].get("right") != 1):
        raise SystemExit("fixture literal topology or physical ordinal drifted")
    source_id = formal.get("source_syntax_id")
    donors = {
        "literal_foreign_formal": foreign.get("source_syntax_id"),
        "literal_foreign_routine": donor.get("source_syntax_id"),
        "literal_receiver_binding": receiver.get("binding_syntax_id"),
    }
    if kind not in donors:
        raise SystemExit(f"unknown mutation: {kind}")
    ids = [source_id, *donors.values()]
    if (any(type(value) is not int or value <= 0 for value in ids) or
            len(set(ids)) != len(ids) or nodes[1].get("binding_syntax_id") != source_id):
        raise SystemExit("fixture has no distinct, positive, exact source IDs")
    # Keep name, type, ordinal, graph digest, SSA definition and every other row.
    # Missing/zero fields would test an earlier reader, not the new signature join.
    nodes[1]["binding_syntax_id"] = donors[kind]
    instructions = [row for routine in routines for block in routine.get("blocks", [])
                    for row in block.get("instructions", [])]
    row = next(index for index, instruction in enumerate(instructions)
               if instruction is definition)
    print(f"stage=literal node=2 row={row} source=AST_LET_DECL")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
