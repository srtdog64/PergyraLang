import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ProjectIndents"),
        None,
    )
    arena_routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ProjectArena"),
        None,
    )
    ready_routine = next(
        (row for row in document.get("routines", [])
         if row.get("name") == "ProjectionReady"),
        None,
    )
    if routine is None or len(routine.get("params", [])) != 2:
        raise SystemExit("fixture has no exact ProjectIndents signature")
    if arena_routine is None or len(arena_routine.get("params", [])) != 2:
        raise SystemExit("fixture has no exact ProjectArena signature")
    if ready_routine is None or len(ready_routine.get("params", [])) != 2:
        raise SystemExit("fixture has no exact ProjectionReady signature")
    record_array = routine["params"][0]
    if kind == "record-array-value-carriage":
        record_array["carriage"] = "value-result"
    elif kind == "record-array-missing-element":
        record_array["type"] = "Array<MissingIndexedRow>"
        record_array["abi_type_name"] = "Array<MissingIndexedRow>"
    elif kind == "record-array-length-missing-element":
        ready_array = ready_routine["params"][0]
        ready_array["type"] = "Array<MissingIndexedRow>"
        ready_array["abi_type_name"] = "Array<MissingIndexedRow>"
    elif kind == "record-array-physical-abi":
        record_array["abi_layout_required"] = True
        record_array["abi_layout_id"] = 1
    elif kind == "record-array-missing-member":
        changed = 0
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                for graph_name in ("expr0_graph", "expr1_graph"):
                    graph = instruction.get(graph_name)
                    if not graph:
                        continue
                    for node in graph.get("nodes", []):
                        if node.get("kind") == "leaf" and node.get("text") == "indent":
                            node["text"] = "missing_indent"
                            changed += 1
        if changed != 1:
            raise SystemExit(f"expected one member marker, changed {changed}")
    elif kind == "record-return-missing-declaration":
        arena_routine["return"] = "MissingIndexedArena"
    elif kind == "record-return-field-type":
        arena = next(
            (row for row in document.get("decls", [])
             if row.get("name") == "IndexedArena"),
            None,
        )
        if arena is None:
            raise SystemExit("fixture has no IndexedArena declaration")
        field = next(
            (row for row in arena.get("fields", [])
             if row.get("name") == "labels"),
            None,
        )
        if field is None:
            raise SystemExit("fixture has no IndexedArena.labels field")
        field["type"] = "Array<Int>"
    elif kind == "record-array-bool-return-type":
        ready_routine["return"] = "String"
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
