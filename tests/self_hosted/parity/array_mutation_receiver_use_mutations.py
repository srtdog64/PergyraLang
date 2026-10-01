"""Generate receiver-use falsifiers from a verified, single-block MIR fixture.

This only writes MIR test inputs and a byte-hash manifest; it never projects or
executes an artifact. LocalRefs, types, and stale/current ValueIds come from the
producer's source_locals and ordered definitions, not fixture spelling.
"""

import argparse
import copy
import hashlib
import json
import pathlib


def receiver_sites_from_producer(program):
    if program.get("schema") != "pgy.mir.v1":
        raise ValueError("receiver fixture requires producer schema pgy.mir.v1")
    routines = program.get("routines")
    if not isinstance(routines, list) or len(routines) != 1:
        raise ValueError("receiver fixture requires exactly one routine")
    routine = routines[0]
    blocks = routine.get("blocks")
    if routine.get("params") != [] or not isinstance(blocks, list) or len(blocks) != 1:
        raise ValueError("receiver fixture requires one parameter-free, straight-line block")
    routine_id, block_id = routine.get("source_syntax_id"), blocks[0].get("id")
    if type(routine_id) is not int or routine_id <= 0 or type(block_id) is not int or block_id < 0:
        raise ValueError("producer routine/block identity is missing")
    declarations = routine.get("source_locals")
    if not isinstance(declarations, list) or len(declarations) != 3:
        raise ValueError("receiver fixture requires two arrays and one scalar declaration")
    local_types = {}
    for declaration in declarations:
        binding = declaration.get("binding_syntax_id")
        if type(binding) is not int or binding <= 0:
            raise ValueError("source local is missing its producer binding identity")
        local_ref = f"declaration:{binding}:0"
        value_type = declaration.get("type")
        if local_ref in local_types or value_type not in ("Array<Int>", "Int"):
            raise ValueError("source local identity/type does not fit the receiver fixture")
        local_types[local_ref] = value_type
    if list(local_types.values()).count("Array<Int>") != 2:
        raise ValueError("receiver fixture does not have exactly two Array<Int> locals")

    instructions = blocks[0].get("instructions")
    if not isinstance(instructions, list):
        raise ValueError("producer block has no instruction rows")
    definitions = {}
    histories = {local_ref: [] for local_ref in local_types}
    sites = {}
    for row, instruction in enumerate(instructions):
        kind = instruction.get("kind")
        if kind == "phi":
            raise ValueError("a phi requires a dominance owner, not fixture row order")
        if kind == "def":
            local_ref = instruction.get("local_ref")
            result = instruction.get("result")
            value_type = instruction.get("abi_type_name")
            if (local_ref not in local_types or value_type != local_types[local_ref]
                    or not isinstance(result, str) or not result or result in definitions):
                raise ValueError("definition is missing an exact typed source-local join")
            definitions[result] = {"local_ref": local_ref, "type": value_type, "row": row}
            histories[local_ref].append(result)
            continue
        operation = instruction.get("arg0")
        if kind != "stmt" or operation not in ("ArrayPush", "ArraySet", "ArrayPop"):
            continue
        if operation in sites:
            raise ValueError(f"receiver fixture repeats {operation}")
        local_ref = instruction.get("local_ref")
        uses = instruction.get("uses")
        if (local_types.get(local_ref) != "Array<Int>" or not isinstance(uses, list)
                or len(uses) != 1 or not isinstance(uses[0], str)):
            raise ValueError(f"{operation} does not carry one reused local receiver")
        history = histories[local_ref]
        if len(history) < 2 or uses[0] != history[-1]:
            raise ValueError(f"{operation} lacks a real stale/latest receiver pair")
        receiver = definitions.get(uses[0])
        if (receiver is None or receiver["local_ref"] != local_ref
                or receiver["type"] != local_types[local_ref]):
            raise ValueError(f"{operation} receiver does not join its typed LocalRef")
        alternatives = {
            value_type: [values[-1] for other_ref, values in histories.items()
                         if other_ref != local_ref and values and local_types[other_ref] == value_type]
            for value_type in ("Int", "Array<Int>")
        }
        if any(len(values) != 1 for values in alternatives.values()):
            raise ValueError(f"{operation} lacks distinct scalar/other-array definitions")
        if "expr0_graph" not in instruction or "expr1_graph" not in instruction:
            raise ValueError(f"{operation} is missing explicit producer graph fields")
        graph0, graph1 = instruction["expr0_graph"], instruction["expr1_graph"]
        if operation == "ArrayPop":
            if graph0 is not None or graph1 is not None:
                raise ValueError("producer Pop has an unexpected value/index graph")
        elif not isinstance(graph0, dict) or (
                operation == "ArraySet" and not isinstance(graph1, dict)) or (
                operation == "ArrayPush" and graph1 is not None):
            raise ValueError(f"{operation} value/index graph shape is not producer-issued")
        sites[operation] = {
            "instruction_row": row,
            "instruction_id": instruction.get("id"),
            "local_ref": local_ref,
            "type": local_types[local_ref],
            "receiver": uses[0],
            "stale": history[-2],
            "wrong_type": alternatives["Int"][0],
            "other_local": alternatives["Array<Int>"][0],
        }
    if set(sites) != {"ArrayPush", "ArraySet", "ArrayPop"}:
        raise ValueError("producer fixture does not issue Push, Set, and Pop")
    return {
        "routine_syntax_id": routine_id,
        "block_id": block_id,
        "source_local_types": local_types,
        "sites": sites,
    }


def receiver_mutations_from_sites(baseline, metadata):
    cases = []
    for operation, site in metadata["sites"].items():
        for mutation in ("wrong-type", "missing", "duplicate", "stale", "other-local"):
            program = copy.deepcopy(baseline)
            instruction = program["routines"][0]["blocks"][0]["instructions"][site["instruction_row"]]
            uses = instruction["uses"]
            if mutation == "missing":
                instruction["uses"] = uses[1:]
            elif mutation == "duplicate":
                instruction["uses"] = [uses[0]] + uses
            else:
                replacement = {
                    "wrong-type": site["wrong_type"],
                    "stale": site["stale"],
                    "other-local": site["other_local"],
                }[mutation]
                instruction["uses"] = [replacement] + uses[1:]
            stem = f"receiver-{mutation}-{operation.removeprefix('Array').lower()}"
            cases.append((stem, program))

    # Absence means explicit JSON null, not a missing or malformed graph field.
    pop = metadata["sites"]["ArrayPop"]
    for lane in ("expr0_graph", "expr1_graph"):
        for mutation in ("missing", "malformed"):
            program = copy.deepcopy(baseline)
            instruction = program["routines"][0]["blocks"][0]["instructions"][pop["instruction_row"]]
            if mutation == "missing":
                del instruction[lane]
            else:
                instruction[lane] = "not-a-null-graph"
            cases.append((f"receiver-pop-{mutation}-{lane.replace('_', '-')}", program))
    return cases


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=pathlib.Path, help="verified producer MIR JSON")
    parser.add_argument("output", type=pathlib.Path, help="dedicated generated-evidence directory")
    arguments = parser.parse_args()
    source_bytes = arguments.source.read_bytes()
    baseline = json.loads(source_bytes)
    metadata = receiver_sites_from_producer(baseline)
    cases = receiver_mutations_from_sites(baseline, metadata)
    case_bytes = [(stem, (json.dumps(program, indent=2) + "\n").encode("utf-8"))
                  for stem, program in cases]
    source_hash = hashlib.sha256(source_bytes).hexdigest()
    manifest = {
        "schema": "pgy.array-mutation-receiver-use-falsifiers.v1",
        "source_schema": baseline["schema"],
        "source_sha256": source_hash,
        "source_bytes": len(source_bytes),
        "producer_metadata": metadata,
        "cases": [stem for stem, _ in cases],
        "case_sha256": {stem: hashlib.sha256(data).hexdigest() for stem, data in case_bytes},
    }
    filenames = [f"{stem}.json" for stem, _ in cases] + ["mutations.list", "mutations.manifest.json"]
    if len(filenames) != len(set(filenames)) or any((arguments.output / name).exists() for name in filenames):
        raise ValueError("receiver evidence output would overwrite an existing case or manifest")
    if arguments.source.read_bytes() != source_bytes:
        raise ValueError("producer MIR bytes changed during receiver metadata validation")
    arguments.output.mkdir(parents=True, exist_ok=True)
    for stem, data in case_bytes:
        (arguments.output / f"{stem}.json").write_bytes(data)
    (arguments.output / "mutations.list").write_bytes("".join(f"{stem}\n" for stem, _ in cases).encode("utf-8"))
    (arguments.output / "mutations.manifest.json").write_bytes((json.dumps(manifest, indent=2) + "\n").encode("utf-8"))
    if arguments.source.read_bytes() != source_bytes:
        raise ValueError("producer MIR bytes changed during receiver falsifier generation")
    print(json.dumps({"cases": len(cases), "source_sha256": source_hash, "input_unchanged": True}))


if __name__ == "__main__":
    main()
