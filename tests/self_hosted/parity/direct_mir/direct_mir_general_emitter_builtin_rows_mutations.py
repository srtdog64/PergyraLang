"""MIR mutations that must keep the general direct-MIR emitter refusing.

seed-random-string-argument: the first `SeedRandom(7)` statement of `Main`
    gets a String literal argument. Its expression graph digest is recomputed
    the way the MIR writer computes it, so the reader admits the graph and the
    refusal comes from the SeedRandom row, whose parameter is Int.
"""
import json
import sys

MODULUS = 268435456


def hash_int(value, number):
    return (value * 131 + number + 2) % MODULUS


def hash_text(value, text):
    data = text.encode("utf-8")
    out = (value * 131 + len(data)) % MODULUS
    for byte in data:
        out = (out * 131 + byte) % MODULUS
    return out


def graph_digest(graph):
    """Mirror of SelfMirExpressionGraphDigest over one persisted graph."""
    nodes = graph["nodes"]
    value = hash_int(71, graph["root"])
    value = hash_int(value, len(nodes))
    for node in nodes:
        value = hash_text(value, node["kind"])
        value = hash_text(value, node["text"])
        value = hash_text(value, node["call_target_kind"])
        value = hash_text(value, node["call_target_name"])
        value = hash_int(value, node["runtime_call_abi_id"])
        value = hash_int(value, -1 if node["left"] is None else node["left"])
        value = hash_int(value, -1 if node["right"] is None else node["right"])
    return 1073741824 + value


def seed_random_string_argument(document):
    main = [row for row in document["routines"] if row.get("name") == "Main"]
    if len(main) != 1:
        raise SystemExit("fixture lost its Main routine")
    for block in main[0]["blocks"]:
        for instruction in block["instructions"]:
            if instruction.get("expr0") != "SeedRandom(7)":
                continue
            graph = instruction["expr0_graph"]
            if graph_digest(graph) != graph["digest"]:
                raise SystemExit("digest mirror disagrees with the MIR writer")
            literals = [node for node in graph["nodes"]
                        if node["kind"] == "integer_literal" and node["text"] == "7"]
            if len(literals) != 1:
                raise SystemExit("SeedRandom argument literal is missing")
            literals[0]["kind"] = "string_literal"
            literals[0]["text"] = '"seven"'
            for node in graph["nodes"]:
                if node["kind"] == "call_argument":
                    node["text"] = 'SeedRandom("seven")'
            instruction["expr0"] = 'SeedRandom("seven")'
            graph["digest"] = graph_digest(graph)
            return
    raise SystemExit("fixture lost its SeedRandom(7) statement")


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:4]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    if kind == "seed-random-string-argument":
        seed_random_string_argument(document)
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8") as stream:
        json.dump(document, stream, separators=(",", ":"))


if __name__ == "__main__":
    main()
