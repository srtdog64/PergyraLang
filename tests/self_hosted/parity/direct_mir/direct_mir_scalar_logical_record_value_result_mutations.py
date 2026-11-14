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
         if row.get("name") == "RememberSession"),
        None,
    )
    production = next((row for row in document.get("routines", [])
                       if row.get("name") == "IntentStepLines"), None)
    nested = next((row for row in document.get("routines", [])
                   if row.get("name") == "ReplaceNestedGraph"), None)
    value_input = next((row for row in document.get("routines", [])
                        if row.get("name") == "RememberSessionCopy"), None)
    if kind.startswith("production-"):
        if production is None: raise SystemExit("fixture has no IntentStepLines")
    elif kind.startswith("nested-"):
        if nested is None: raise SystemExit("fixture has no ReplaceNestedGraph")
    elif kind.startswith("record-") or kind.startswith("member-"):
        if routine is None:
            raise SystemExit("fixture has no RememberSession routine")
    elif kind.startswith("value-input-") and value_input is None:
        raise SystemExit("fixture has no RememberSessionCopy routine")
    if kind == "production-copyout-pass":
        production["params"][10]["pass"] = "indirect"
    elif kind == "production-copyout-abi":
        production["params"][10]["abi_layout_required"] = True
        production["params"][10]["abi_layout_id"] = 1
    elif kind == "production-collection-carriage":
        production["params"][5]["carriage"] = "readonly-ref"
    elif kind == "production-readonly-pass":
        production["params"][0]["pass"] = "direct"
    elif kind == "production-scalar-carriage":
        production["params"][2]["carriage"] = "readonly-ref"
    elif kind == "record-copyout-carriage":
        routine["params"][0]["carriage"] = "readonly-ref"
    elif kind == "record-copyout-pass":
        routine["params"][0]["pass"] = "indirect"
    elif kind == "value-input-carriage":
        value_input["params"][0]["carriage"] = "readonly-ref"
    elif kind.startswith("nested-"):
        member = next(
            (instruction for block in nested.get("blocks", [])
             for instruction in block.get("instructions", [])
             if instruction.get("expr1") ==
                "analysis.expression_surfaces.expression_graph"),
            None,
        )
        if member is None:
            raise SystemExit("fixture has no nested member rebind")
        if kind == "nested-member":
            member["expr1_graph"]["nodes"][3]["text"] = "missing"
            member["expr1_graph"]["nodes"][4]["text"] = (
                "analysis.expression_surfaces.missing")
            member["expr1"] = "analysis.expression_surfaces.missing"
        elif kind == "nested-binding":
            member["expr1_graph"]["nodes"][0]["binding_kind"] = "none"
            member["expr1_graph"]["nodes"][0]["binding_ordinal"] = None
        elif kind == "nested-rhs-type":
            member["expr0"] = "1"
            member["expr0_graph"] = {
                "root": 0,
                "nodes": [{
                    "kind": "integer_literal", "text": "1",
                    "call_target_kind": "none", "call_target_name": "",
                    "call_target_syntax_id": 0, "binding_kind": "none",
                    "binding_ordinal": None, "left": None, "right": None,
                }],
            }
            member["uses"] = [
                use for use in member.get("uses", [])
                if use.startswith("analysis.")
            ]
            member["expr0_local_refs"] = []
        elif kind == "nested-missing-use":
            member["uses"] = [
                use for use in member.get("uses", [])
                if not use.startswith("graph.")
            ]
        else:
            raise SystemExit(f"unknown mutation: {kind}")
    elif kind.startswith("member-"):
        local_member = kind.startswith("member-local-")
        member = next(
            (instruction for block in routine.get("blocks", [])
             for instruction in block.get("instructions", [])
             if (instruction.get("arg1") == "local") == local_member and
             instruction.get("expr1") ==
                 ("local_session.keys" if local_member else "session.keys")),
            None)
        if member is None:
            raise SystemExit("fixture has no selected member rebind")
        if kind in ("member-field", "member-local-field"):
            member["expr1_graph"]["nodes"][1]["text"] = "missing"
            owner = "local_session" if local_member else "session"
            member["expr1_graph"]["nodes"][2]["text"] = f"{owner}.missing"
            member["expr1"] = f"{owner}.missing"
        elif kind == "member-rhs-type":
            member["expr0"] = "1"
            member["expr0_graph"] = {
                "root": 0,
                "nodes": [{
                    "kind": "integer_literal", "text": "1",
                    "call_target_kind": "none", "call_target_name": "",
                    "call_target_syntax_id": 0, "binding_kind": "none",
                    "binding_ordinal": None, "left": None, "right": None,
                }],
            }
            member["uses"] = [
                use for use in member.get("uses", [])
                if use.startswith("session.")
            ]
            member["expr0_local_refs"] = []
        elif kind == "member-binding":
            member["expr1_graph"]["nodes"][0]["binding_kind"] = "none"
            member["expr1_graph"]["nodes"][0]["binding_ordinal"] = None
        elif kind == "member-source-kind":
            member["source_type"] = "AST_LET_DECL"
        elif kind == "member-local-ref-missing":
            member["local_ref"] = None
        elif kind == "member-local-ref-foreign":
            member["local_ref"] = (
                f"parameter:{routine['source_syntax_id']}:0")
        elif kind == "member-local-use-missing":
            member["uses"] = member.get("uses", [])[1:]
        elif kind == "member-local-use-reordered":
            member["uses"] = list(reversed(member.get("uses", [])))
        elif kind == "member-local-result-stale":
            member["result"] = member.get("uses", [""])[0]
        elif kind == "member-local-binding":
            member["expr1_graph"]["nodes"][0]["binding_kind"] = (
                "formal_parameter")
            member["expr1_graph"]["nodes"][0]["binding_ordinal"] = 0
        else:
            raise SystemExit(f"unknown mutation: {kind}")
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")


if __name__ == "__main__":
    main()
