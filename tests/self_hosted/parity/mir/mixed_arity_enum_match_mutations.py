"""Invalid owned MIR facts only; the runner never executes these inputs."""

import json
import sys


source, mutation, output = sys.argv[1:]
with open(source, encoding="utf-8") as stream:
    document = json.load(stream)
routine = next(row for row in document["routines"] if row["name"] == "ParcelScore")
instructions = [ins for block in routine["blocks"] for ins in block["instructions"]]
pair = next(ins for ins in instructions if ins.get("match_variant") == "Pair")
empty = next(ins for ins in instructions if ins.get("match_patterns") == ["Empty"])

if mutation == "missing-binding-type":
    pair["match_binding_types"].pop()
elif mutation == "missing-type-field":
    del pair["match_binding_types"]
elif mutation == "wrong-binding-type":
    pair["match_binding_types"][1] = "String"
elif mutation == "swapped-binding-types":
    labelled = next(ins for ins in instructions if ins.get("match_variant") == "Label")
    assert labelled["match_binding_types"] == ["String", "Int"]
    labelled["match_binding_types"].reverse()
elif mutation == "duplicate-binding":
    pair["match_bindings"][1] = "value"
    pair["match_patterns"] = ["Pair(value, value)"]
elif mutation == "wrong-variant-arity":
    pair["match_variant"] = "One"
    pair["match_patterns"] = ["One(value, tail)"]
elif mutation == "zero-variant-binding":
    empty["match_variant"] = "Empty"
    empty["match_patterns"] = ["Empty(ghost)"]
    empty["match_bindings"] = ["ghost"]
    empty["match_binding_types"] = ["Int"]
elif mutation == "zero-variant-invalid-arrays":
    empty["match_bindings"] = [17]
    empty["match_binding_types"] = [17]
elif mutation == "wrong-scrutinee-identity":
    main = next(row for row in document["routines"] if row["name"] == "Main")
    leaf = next(node for node in pair["expr0_graph"]["nodes"] if node["kind"] == "leaf")
    assert int(leaf["binding_syntax_id"]) != int(main["source_syntax_id"])
    leaf["binding_syntax_id"] = int(main["source_syntax_id"])
elif mutation == "payload-variant-without-arguments":
    main = next(row for row in document["routines"] if row["name"] == "Main")
    definition = next(ins for block in main["blocks"] for ins in block["instructions"]
                      if ins.get("arg0") == "empty" and ins.get("kind") == "def")
    leaf = next(node for node in definition["expr0_graph"]["nodes"] if node.get("text") == "Empty")
    leaf["text"] = "One"
    definition["expr0"] = "One"
elif mutation == "cross-arm-binding-use":
    # `middle` exists only in Triple's true arm, not Pair's return scope.
    consumer = next(ins for ins in instructions if (ins.get("expr0") or "") == "((value * 10) + tail)")
    leaf = next(node for node in consumer["expr0_graph"]["nodes"] if node.get("text") == "tail")
    leaf["text"] = "middle"
else:
    raise SystemExit(f"unknown mutation: {mutation}")

with open(output, "w", encoding="utf-8", newline="\n") as stream:
    json.dump(document, stream, separators=(",", ":"))
    stream.write("\n")
