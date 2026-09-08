"""Generated MIR data for call-identity validation; never executable inputs."""
import copy
import json
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
document = json.loads(path.read_text(encoding="utf-8"))


def emit(name, edit):
    candidate = copy.deepcopy(document)
    edit(candidate)
    path.with_name(path.name + "." + name + ".json").write_text(
        json.dumps(candidate, separators=(",", ":")), encoding="utf-8"
    )


def first(candidate):
    return candidate["generic_method_specializations"][0]


emit("missing-table", lambda d: d.pop("generic_method_specializations"))
emit("missing-row", lambda d: d["generic_method_specializations"].pop(0))
emit("missing-anchors", lambda d: first(d).pop("occurrences"))
emit("empty-anchors", lambda d: first(d).__setitem__("occurrences", []))
emit("duplicate-anchor", lambda d: first(d)["occurrences"].append(copy.deepcopy(first(d)["occurrences"][0])))
for field in ("routine_syntax_id", "block_id", "instruction_id", "call_node"):
    emit("crossed-" + field, lambda d, field=field: first(d)["occurrences"][0].__setitem__(field, 2147483647))
emit("wrong-lane", lambda d: first(d)["occurrences"][0].__setitem__("expression_lane", 2))
emit("wrong-target", lambda d: first(d).__setitem__("call_target_syntax_id", 2147483647))
emit("wrong-formal", lambda d: first(d)["generic_params"].__setitem__(0, "OtherFormal"))
emit("row-order", lambda d: d["generic_method_specializations"].reverse())


def provenance_epoch(candidate):
    for row in candidate["generic_method_specializations"]:
        row["source_owner_syntax_id"] += 1000000


emit("provenance-epoch", provenance_epoch)
