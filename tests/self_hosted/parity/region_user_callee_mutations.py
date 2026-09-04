import copy
import json
from pathlib import Path
import sys


source = Path(sys.argv[1])
output_dir = Path(sys.argv[2])
document = json.loads(source.read_text(encoding="utf-8"))
sink = next(routine for routine in document["routines"] if routine["name"] == "Sink")
assert sink["return"] == "Void"
assert len(sink["params"]) == 1
parameter = sink["params"][0]
assert parameter["type"] == "String"
assert parameter["carriage"] == "readonly-ref"
assert parameter["resource"] == "none"
assert parameter["pass"] == "direct"
assert parameter["abi_layout_id"] == 0
assert parameter["abi_layout_required"] is False


def write(name, mutate):
    mutated = copy.deepcopy(document)
    mutated_sink = next(
        routine for routine in mutated["routines"] if routine["name"] == "Sink"
    )
    mutate(mutated_sink, mutated_sink["params"][0])
    (output_dir / f"{name}.mir.json").write_text(
        json.dumps(mutated, separators=(",", ":")), encoding="utf-8"
    )


write("pass-indirect", lambda routine, param: param.update({"pass": "indirect"}))
write("resource-region", lambda routine, param: param.update({"resource": "region"}))
write(
    "abi-required",
    lambda routine, param: param.update(
        {"abi_layout_required": True, "abi_layout_id": 1}
    ),
)
write(
    "carriage-value-result",
    lambda routine, param: param.update({"carriage": "value-result"}),
)
write(
    "return-string",
    lambda routine, param: routine.update({"return": "String"}),
)
