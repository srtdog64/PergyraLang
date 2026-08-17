import json
import sys

def main():
    if len(sys.argv) != 4:
        raise SystemExit("usage: mutations.py INPUT KIND OUTPUT")
    source, kind, output = sys.argv[1:]
    with open(source, encoding="utf-8") as stream:
        document = json.load(stream)
    composed = next((routine for routine in document.get("routines", [])
                     if routine.get("name") == "ValueBounds"), None)
    if composed is None and kind.startswith("composed-"):
        raise SystemExit("fixture has no ValueBounds routine")
    param = next((param for routine in document.get("routines", [])
         for param in routine.get("params", [])
         if param.get("type") == "Array<Int>" and
         param.get("carriage") == "value-result"), None)
    if param is None:
        raise SystemExit("fixture has no value-result Array<Int> parameter")
    if kind == "carriage":
        param["carriage"] = "readonly-ref"
    elif kind == "pass-shape":
        param["pass"] = "indirect"
    elif kind == "resource":
        param["resource"] = "own"
    elif kind == "abi-required":
        param["abi_layout_required"] = False
        param["abi_layout_id"] = 0
    elif kind == "composed-record-pass":
        composed["params"][0]["pass"] = "direct"
    elif kind == "composed-copyout-carriage":
        composed["params"][2]["carriage"] = "readonly-ref"
    elif kind == "call-nonaddressable":
        main_routine = next((routine for routine in document["routines"]
                             if routine.get("name") == "Main"), None)
        call = next((instruction for block in main_routine["blocks"]
                     for instruction in block["instructions"]
                     if instruction.get("arg0") == "Log"), None)
        if call is None:
            raise SystemExit("fixture has no value-result direct call")
        graph = call["expr0_graph"]
        nodes = graph["nodes"]
        def node(node_kind, text, left=None, right=None):
            return {"kind": node_kind, "text": text,
                    "call_target_kind": "none", "call_target_name": "",
                    "call_target_syntax_id": 0, "binding_kind": "none",
                    "binding_ordinal": None, "left": left, "right": right}
        base = len(nodes)
        nodes.extend([node("array_literal", "[]"),
                      node("integer_literal", "0"),
                      node("array_element", "[0]", base, base + 1),
                      node("call_argument", "ObserveWindow(..., [0])",
                           7, base + 2)])
        graph["root"] = base + 3
        call["uses"] = []
    else:
        raise SystemExit(f"unknown mutation: {kind}")
    with open(output, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")

if __name__ == "__main__":
    main()
