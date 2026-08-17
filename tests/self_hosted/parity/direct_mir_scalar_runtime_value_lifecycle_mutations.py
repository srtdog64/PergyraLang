import copy
import json
import sys


def runtime_instruction(routine, source_name):
    for block in routine.get("blocks", []):
        for instruction in block.get("instructions", []):
            row = instruction.get("runtime_call_abi")
            if isinstance(row, dict) and row.get("source") == source_name:
                return block, instruction
    raise SystemExit(f"fixture has no {source_name} runtime-call row")


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8-sig") as stream:
        document = json.load(stream)
    routine = next((row for row in document.get("routines", [])
                    if any(isinstance(instruction.get("runtime_call_abi"), dict)
                           and instruction["runtime_call_abi"].get("source") ==
                           "AllocatorResult"
                           for block in row.get("blocks", [])
                           for instruction in block.get("instructions", []))), None)
    if routine is None:
        raise SystemExit("fixture has no runtime-value lifecycle routine")

    if kind == "wrong-layout":
        _, instruction = runtime_instruction(routine, "AllocatorResult")
        layout = instruction.get("abi_layout")
        if not isinstance(layout, dict):
            raise SystemExit("AllocatorResult has no ABI layout")
        layout["size"] = int(layout["size"]) + 8
    elif kind == "wrong-graph-runtime-id":
        _, instruction = runtime_instruction(routine, "AllocatorResult")
        graph = instruction.get("expr0_graph")
        if not isinstance(graph, dict):
            raise SystemExit("AllocatorResult has no expression graph")
        call = next((node for node in graph.get("nodes", [])
                     if node.get("kind") == "call" and
                     node.get("call_target_name") == "AllocatorResult"), None)
        if call is None:
            raise SystemExit("AllocatorResult has no call node")
        call["runtime_call_abi_id"] = 1
    elif kind == "wrong-runtime-row":
        _, instruction = runtime_instruction(routine, "TextBuilderAppend")
        instruction["runtime_call_abi"]["c_symbol"] = "pgy_text_builder_drop"
    elif kind == "foreign-local":
        _, instruction = runtime_instruction(routine, "TextBuilderFinish")
        graph = instruction.get("expr0_graph")
        if not isinstance(graph, dict):
            raise SystemExit("TextBuilderFinish has no expression graph")
        owner_leaf = next((node for node in graph.get("nodes", [])
                           if node.get("kind") == "leaf" and
                           node.get("text") == "output"), None)
        if owner_leaf is None:
            raise SystemExit("TextBuilderFinish has no output owner leaf")
        owner_leaf["text"] = "result_allocator"
        instruction["uses"] = ["result_allocator.1", "result_allocator.1"]
    elif kind == "missing-terminal-destroy":
        block, instruction = runtime_instruction(routine, "AllocatorDestroy")
        block["instructions"].remove(instruction)
    elif kind == "use-after-terminal":
        _, append = runtime_instruction(routine, "TextBuilderAppend")
        finish_block, finish = runtime_instruction(routine, "TextBuilderFinish")
        late_append = copy.deepcopy(append)
        late_append["id"] = max(
            row.get("id", 0)
            for block in routine.get("blocks", [])
            for row in block.get("instructions", [])) + 1
        position = finish_block["instructions"].index(finish) + 1
        finish_block["instructions"].insert(position, late_append)
    else:
        raise SystemExit(f"unknown mutation: {kind}")

    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
