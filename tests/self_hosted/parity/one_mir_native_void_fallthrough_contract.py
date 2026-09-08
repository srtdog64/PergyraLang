"""Source comparison may erase only the native synthetic, effect-free Void exit."""
import copy
import json
import pathlib
import sys


def native_source_rows(routine):
    rows = routine["blocks"][0]["instructions"]
    if len(routine["blocks"]) != 1 or routine["return"] != "Void" or not rows:
        return rows
    tail = rows[-1]
    if tail["kind"] != "return" or tail["source_type"] is not None:
        return rows
    expected = {
        "id": tail["id"], "kind": "return", "name": "return",
        "abi_type_name": "Void", "abi_layout_id": 0,
        "abi_layout_required": False,
        **dict.fromkeys((
            "result", "arg0", "arg1", "slot_anchor", "abi_layout",
            "machine_layer", "machine_contact_kind", "expr0", "expr0_graph",
            "expr1", "expr1_graph", "speculation", "source_type",
            "match_variant", "destructure_element_type", "ast",
        )),
        **{key: [] for key in (
            "match_patterns", "match_bindings", "match_binding_types",
            "destructure_bindings", "uses",
        )},
    }
    if type(tail["id"]) is not int or tail["id"] < 0 or tail != expected:
        raise RuntimeError("native synthetic Void exit carries source, value, ABI or effect facts")
    return rows[:-1]


if __name__ == "__main__":
    if len(sys.argv) != 1:
        if len(sys.argv) != 3 or sys.argv[1] != "normalize-native":
            raise SystemExit("expected normalize-native <native MIR JSON>")
        document = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
        for row in document["routines"]:
            if len(row["blocks"]) == 1:
                row["blocks"][0]["instructions"] = native_source_rows(row)
        print(json.dumps(document, separators=(",", ":")))
        raise SystemExit(0)
    tail = {
        "id": 0, "kind": "return", "name": "return",
        "abi_type_name": "Void", "abi_layout_id": 0, "abi_layout_required": False,
        **dict.fromkeys((
            "result", "arg0", "arg1", "slot_anchor", "abi_layout",
            "machine_layer", "machine_contact_kind", "expr0", "expr0_graph",
            "expr1", "expr1_graph", "speculation", "source_type",
            "match_variant", "destructure_element_type", "ast",
        )),
        **{key: [] for key in (
            "match_patterns", "match_bindings", "match_binding_types",
            "destructure_bindings", "uses",
        )},
    }
    source = {"kind": "stmt", "source_type": "AST_CALL"}
    routine = {"return": "Void", "blocks": [{"instructions": [source, tail]}]}
    assert native_source_rows(routine) == [source]
    mutations = {
        "id": -1, "kind": "stmt", "name": "other", "source_type": "AST_RETURN",
        "expr0": "7", "expr0_graph": {"root": 0}, "uses": ["value.1"],
        "abi_layout_required": True, "abi_type_name": "Int", "abi_layout_id": 1,
        "machine_layer": {"effect": "write"}, "future_unknown_fact": True,
    }
    for key, value in mutations.items():
        changed = copy.deepcopy(routine)
        changed["blocks"][0]["instructions"][-1][key] = value
        try:
            retained = native_source_rows(changed)
        except RuntimeError:
            continue
        assert len(retained) == 2, f"comparison erased a non-synthetic exit: {key}"
    nonvoid = copy.deepcopy(routine)
    nonvoid["return"] = "Int"
    assert len(native_source_rows(nonvoid)) == 2
    print("[native-void-fallthrough] one exact exit plus 13 non-erasable controls: PASS")
