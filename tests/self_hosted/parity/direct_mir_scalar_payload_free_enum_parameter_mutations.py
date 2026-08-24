import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    tone = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ToneLabel"),
        None,
    )
    declaration = next(
        (row for row in document.get("decls", [])
         if row.get("name") == "Tone"),
        None,
    )
    if tone is None or len(tone.get("params", [])) != 1:
        raise SystemExit("fixture has no exact ToneLabel signature")
    if declaration is None or len(declaration.get("variants", [])) != 2:
        raise SystemExit("fixture has no exact Tone declaration")
    parameter = tone["params"][0]
    tone_ordinal = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ToneOrdinal"),
        None,
    )
    if tone_ordinal is None:
        raise SystemExit("fixture has no ToneOrdinal routine")
    tone_name = next((row for row in document.get("routines", [])
                      if row.get("name") == "ToneName"), None)
    if tone_name is None:
        raise SystemExit("fixture has no ToneName routine")
    default_tone = next((row for row in document.get("routines", [])
                         if row.get("name") == "DefaultTone"), None)
    tones_differ = next((row for row in document.get("routines", [])
                         if row.get("name") == "TonesDiffer"), None)
    if default_tone is None or tones_differ is None:
        raise SystemExit("fixture has no unqualified enum return/inequality")
    def expression_nodes():
        for block in tone_ordinal.get("blocks", []):
            for instruction in block.get("instructions", []):
                graph = instruction.get("expr0_graph")
                if graph is not None:
                    yield graph.get("nodes", [])

    equality_nodes = next(
        (nodes for nodes in expression_nodes()
         if any(node.get("kind") == "equality" for node in nodes)),
        None,
    )
    if equality_nodes is None:
        raise SystemExit("fixture has no enum equality graph")
    if kind == "enum-parameter-carriage":
        parameter["carriage"] = "readonly-ref"
        parameter["pass"] = "indirect"
    elif kind == "enum-parameter-physical-abi":
        parameter["abi_layout_required"] = True
        parameter["abi_layout_id"] = 1
    elif kind == "enum-variant-payload":
        declaration["variants"][0]["param_count"] = 1
        declaration["variants"][0]["param_types"] = ["Int"]
    elif kind == "enum-missing-declaration":
        parameter["type"] = "MissingTone"
        parameter["abi_type_name"] = "MissingTone"
    elif kind == "enum-declaration-kind":
        declaration["kind"] = "struct"
        declaration["nominal_kind"] = "struct"
    elif kind == "enum-return-collection":
        tone["return"] = "Array<String>"
    elif kind == "enum-expression-wrong-owner":
        next(node for node in equality_nodes
             if node.get("text") == "Tone")["text"] = "Direction"
    elif kind == "enum-expression-wrong-variant":
        next(node for node in equality_nodes
             if node.get("text") == "Warm")["text"] = "Missing"
    elif kind == "enum-expression-missing-binding":
        formal = next(node for node in equality_nodes
                      if node.get("text") == "id")
        formal["binding_kind"] = "none"
        formal["binding_ordinal"] = None
    elif kind == "enum-expression-wrong-type":
        tone_ordinal["params"][0]["type"] = "Direction"
        tone_ordinal["params"][0]["abi_type_name"] = "Direction"
    elif kind == "enum-match-duplicate-variant":
        cases = [
            instruction
            for block in tone_name.get("blocks", [])
            for instruction in block.get("instructions", [])
            if instruction.get("source_type") == "AST_MATCH_CASE"
        ]
        if len(cases) != 2:
            raise SystemExit("fixture has no exact two-case ToneName match")
        cases[1]["match_patterns"] = list(cases[0]["match_patterns"])
    elif kind == "enum-unqualified-variant-missing":
        graph = next(
            instruction["expr0_graph"]
            for block in default_tone.get("blocks", [])
            for instruction in block.get("instructions", [])
            if instruction.get("expr0_graph") is not None
        )
        next(node for node in graph["nodes"]
             if node.get("text") == "Warm")["text"] = "Missing"
    elif kind == "enum-inequality-wrong-type":
        tones_differ["params"][1]["type"] = "Direction"
        tones_differ["params"][1]["abi_type_name"] = "Direction"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
