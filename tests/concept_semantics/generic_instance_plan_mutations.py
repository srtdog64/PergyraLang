"""MIR instance-input consistency controls; these documents are never executed."""
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


def template(candidate):
    target = candidate["generic_method_specializations"][0]["call_target_syntax_id"]
    return next(r for r in candidate["routines"] if r["source_syntax_id"] == target)


def return_receipt(candidate):
    return next(i for b in template(candidate)["blocks"] for i in b["instructions"]
                if i["kind"] == "return")


emit("routine-order", lambda d: d["routines"].reverse())
emit("missing-return", lambda d: template(d).pop("return"))
# The local/return body type view must not turn an invalid physical receipt
# into the canonical scalar absence merely because T was instantiated.
emit("return-layout", lambda d: return_receipt(d).__setitem__("abi_layout_id", 42))
emit("return-required", lambda d: return_receipt(d).__setitem__("abi_layout_required", True))
emit("missing-constraints", lambda d: template(d).pop("generic_constraints"))
emit("empty-constraints", lambda d: template(d).__setitem__("generic_constraints", []))
emit("crossed-constraint", lambda d: template(d)["generic_constraints"][0].__setitem__("name", "OtherFormal"))
emit("missing-bound", lambda d: template(d)["generic_constraints"][0].pop("constraint"))
emit("null-bound", lambda d: template(d)["generic_constraints"][0].__setitem__("constraint", None))
emit("duplicate-constraint", lambda d: template(d)["generic_constraints"].append(
    copy.deepcopy(template(d)["generic_constraints"][0])))
emit("scalar-constraint", lambda d: template(d)["generic_constraints"].append(0))

if any(d.get("nominal_kind") == "subject" for d in document["decls"]):
    def cell(candidate):
        return next(d for d in candidate["decls"] if d.get("nominal_kind") == "subject")

    emit("cell-missing-id", lambda d: cell(d).pop("source_syntax_id"))
    emit("cell-field-missing-id", lambda d: cell(d)["fields"][0].pop("source_syntax_id"))
    emit("cell-field-crossed-id", lambda d: cell(d)["fields"][0].__setitem__(
        "source_syntax_id", cell(d)["source_syntax_id"]))
    emit("cell-physical-layout", lambda d: cell(d).__setitem__("abi_layout_id", 42))
    emit("cell-unknown-field-type", lambda d: cell(d)["fields"][0].__setitem__("type", "UnknownCellField"))
    emit("cell-return-escape", lambda d: template(d).__setitem__("return", cell(d)["name"]))
