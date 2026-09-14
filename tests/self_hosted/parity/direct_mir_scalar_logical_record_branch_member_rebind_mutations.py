import json
import sys


def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT MUTATION OUTPUT")
    with open(sys.argv[1], encoding="utf-8") as source:
        program = json.load(source)
    routine = next(
        row for row in program["routines"] if row["name"] == "UpdateState"
    )
    assignments = [
        instruction
        for block in routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("source_type") == "AST_ASSIGNMENT"
        and instruction.get("arg0") == "state"
    ]
    ok = next(row for row in assignments if row.get("expr1") == "state.ok")
    later = next(
        row for row in assignments if row.get("expr1") == "state.last_row"
    )
    copied_routine = next(
        row for row in program["routines"] if row["name"] == "DisableStateCopy"
    )
    copied = next(
        instruction
        for block in copied_routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("expr1") == "state.ok"
    )
    built_routine = next(
        row for row in program["routines"] if row["name"] == "BuildStateFromFormal"
    )
    built = next(
        instruction
        for block in built_routine["blocks"]
        for instruction in block["instructions"]
        if instruction.get("expr1") == "state.last_row"
    )
    mutation = sys.argv[2]
    if mutation == "non-dominating-prefix":
        later["uses"].insert(0, ok["result"])
    elif mutation == "missing-target-local-ref":
        ok["local_ref"] = None
    elif mutation == "foreign-target-local-ref":
        ok["local_ref"] = f"parameter:{routine['source_syntax_id']}:1"
    elif mutation == "wrong-rhs-type":
        ok["expr0"] = "1"
        ok["expr0_graph"] = {
            "root": 0,
            "nodes": [{
                "kind": "integer_literal", "text": "1",
                "call_target_kind": "none", "call_target_name": "",
                "call_target_syntax_id": 0, "binding_kind": "none",
                "binding_ordinal": None, "left": None, "right": None,
            }],
        }
    elif mutation == "wrong-default-carriage":
        copied["arg1"] = "inout_param"
    elif mutation == "wrong-default-binding":
        copied["expr1_graph"]["nodes"][0]["binding_kind"] = "none"
        copied["expr1_graph"]["nodes"][0]["binding_ordinal"] = None
    elif mutation == "member-name-binding":
        member = built["expr1_graph"]["nodes"][1]
        rhs = built["expr0_graph"]["nodes"][0]
        member["binding_syntax_id"] = rhs["binding_syntax_id"]
        member["binding_kind"] = "formal_parameter"
        member["binding_ordinal"] = 0
    else:
        raise SystemExit(f"unknown mutation: {mutation}")
    with open(sys.argv[3], "w", encoding="utf-8", newline="\n") as output:
        json.dump(program, output, separators=(",", ":"))
        output.write("\n")


if __name__ == "__main__":
    main()
