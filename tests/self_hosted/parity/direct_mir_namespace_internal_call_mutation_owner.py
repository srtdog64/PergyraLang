"""Own namespace-internal callee-binding mutations for the focused gate."""


def namespace_internal_call_edge(document):
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for lane in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(lane)
                    if not isinstance(graph, dict):
                        continue
                    nodes = graph.get("nodes", [])
                    for call in nodes:
                        if (call.get("call_target_kind") != "direct" or
                                call.get("call_target_name") !=
                                "InternalNames_Fact1"):
                            continue
                        callee_index = call.get("left")
                        if (not isinstance(callee_index, int) or
                                callee_index < 0 or callee_index >= len(nodes)):
                            return None
                        return call, nodes[callee_index]
    return None


def mutate_namespace_internal_callee_binding(document, kind):
    edge = namespace_internal_call_edge(document)
    if edge is None:
        raise SystemExit("fixture has no namespace-internal call edge")
    call, callee = edge
    if kind.endswith("missing"):
        callee["binding_syntax_id"] = 0
        callee["binding_kind"] = "none"
        callee["binding_ordinal"] = None
        return
    foreign = next(
        (routine.get("source_syntax_id")
         for routine in document.get("routines", [])
         if routine.get("name") == "InternalNames_Fact2"),
        None,
    )
    if not isinstance(foreign, int) or foreign <= 0:
        raise SystemExit("fixture has no foreign namespace callable")
    if foreign == call.get("call_target_syntax_id"):
        raise SystemExit("foreign callable identity is not distinct")
    callee["binding_syntax_id"] = foreign
    callee["binding_kind"] = "declared_callable"
    callee["binding_ordinal"] = None
