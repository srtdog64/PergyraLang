"""Metamorphic and fail-closed mutations for the role-override MIR rung."""

import copy
import json
import pathlib
import sys
source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))

def declaration(document, kind):
    rows = [row for row in document["decls"] if row["nominal_kind"] == kind]
    if len(rows) != 1:
        raise RuntimeError(f"expected one {kind} declaration")
    return rows[0]


def routine(document, owner):
    rows = [row for row in document["routines"] if row.get("owner") == owner]
    if len(rows) != 1:
        raise RuntimeError(f"expected one routine owned by {owner}")
    return rows[0]


def main_instruction(document, index):
    rows = [row for row in document["routines"] if row["name"] == "Main"]
    if len(rows) != 1:
        raise RuntimeError("expected one Main routine")
    return rows[0]["blocks"][0]["instructions"][index]


def emit(name, suffix, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (target / f"{name}.{suffix}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def retarget(document, source_name, target_owner, target_method, call_kind):
    selected = next(row for row in document["routines"]
                    if row.get("owner") == target_owner and row["name"].endswith(target_method))
    target_name = selected["name"] if call_kind == "direct" else f"{target_owner}_{target_method}"
    changed = 0
    for row in document["routines"]:
        if row["name"] != "Main":
            continue
        for block in row["blocks"]:
            for instruction in block["instructions"]:
                for field in ("expr0_graph", "expr1_graph"):
                    for node in (instruction.get(field) or {}).get("nodes", []):
                        if node.get("call_target_name") == source_name and node.get("call_target_kind") == call_kind:
                            node.update(call_target_name=target_name, call_target_syntax_id=selected["source_syntax_id"])
                            changed += 1
                        # Change the direct callee leaf too, so a spelling/ID
                        # mismatch cannot accidentally serve as the falsifier.
                        if call_kind == "direct":
                            was_callee = node.get("text") == source_name
                            node["text"] = node["text"].replace(source_name, target_name)
                            if was_callee and node.get("binding_kind") == "declared_callable":
                                node["binding_syntax_id"] = selected["source_syntax_id"]
                    if call_kind == "direct" and instruction.get(field.removesuffix("_graph")):
                        key = field.removesuffix("_graph")
                        instruction[key] = instruction[key].replace(source_name, target_name)
    if changed != 1:
        raise RuntimeError(f"expected one {source_name} {call_kind} call, got {changed}")


if "--generality" in sys.argv[3:]:
    emit("unbound-direct-role-call", "negative", lambda d: retarget(
        d, "ObserveFirst", "FirstMarker", "Observe", "direct"))
    emit("impl-range-overlap", "negative", lambda d: next(
        r for r in d["decls"] if r["name"] == "FirstMarker")["impls"][0].update(method_start=1))
    emit("impl-nonobject-tail", "negative", lambda d: next(
        r for r in d["decls"] if r["name"] == "FirstMarker")["impls"].append(0))
    emit("impl-method-type", "negative", lambda d: next(
        r for r in d["decls"] if r["name"] == "ReceiverMark")["methods"][2]["params"][1].update(type="Bool"))
    for field, value in [("binding_syntax_id", 0), ("binding_ordinal", 1)]:
        emit(f"receiver-{field}", "negative", lambda d, field=field, value=value: next(
            r for r in d["routines"] if r.get("owner") == "FirstMarker" and r["name"] == "Change"
        )["blocks"][0]["instructions"][0]["expr1_graph"]["nodes"][0].update({field: value}))
    raise SystemExit(0)


emit("declaration-order", "positive", lambda d: d.update(decls=list(reversed(d["decls"]))))
emit("routine-order", "positive", lambda d: d.update(routines=[d["routines"][2], d["routines"][0], d["routines"][1]]))
emit("both-orders", "positive", lambda d: (
    d.__setitem__("decls", list(reversed(d["decls"]))),
    d.update(routines=[d["routines"][2], d["routines"][1], d["routines"][0]]),
))

emit("role-name", "negative", lambda d: declaration(d, "role").update(name="RenamedRole"))
emit("subject-name", "negative", lambda d: declaration(
    d, "subject"
).__setitem__("name", "RenamedTarget"))
emit("subject-wire-kind", "negative", lambda d: declaration(d, "subject").__setitem__("kind", "subject"))
emit("subject-value-kind", "negative", lambda d: declaration(d, "subject").__setitem__("nominal_kind", "class"))
emit("for-type", "negative", lambda d: declaration(d, "role").__setitem__(
    "for_type", "MissingTarget"
))
emit("role-routine-owner", "negative", lambda d: routine(
    d, "OverrideSurface"
).__setitem__("owner", "OverrideTarget"))
emit("role-method-name", "negative", lambda d: declaration(
    d, "role"
)["methods"][0].__setitem__("name", "Other"))
emit("missing-method-source-id", "negative", lambda d: declaration(
    d, "role"
)["methods"][0].pop("source_syntax_id"))
emit("crossed-method-source-id", "negative", lambda d: declaration(
    d, "role"
)["methods"][0].__setitem__(
    "source_syntax_id", routine(d, "OverrideTarget")["source_syntax_id"]
))
emit("direct_override_as_ability_impl", "negative", lambda d: declaration(
    d, "role"
).__setitem__("impls", [{
    "ability": {"base": "Invented", "actuals": []},
    "method_start": 0,
    "method_count": 1,
}]))
emit("direct_override_method_drop", "negative", lambda d: declaration(
    d, "role"
).__setitem__("methods", []))
emit("member-target", "negative", lambda d: main_instruction(
    d, 1
)["expr0_graph"]["nodes"][3].__setitem__(
    "call_target_name", "OverrideSurface_Name"
))
emit("member-target-id-zero", "negative", lambda d: main_instruction(
    d, 1
)["expr0_graph"]["nodes"][3].__setitem__("call_target_syntax_id", 0))
emit("member-target-id-wrong", "negative", lambda d: main_instruction(
    d, 1
)["expr0_graph"]["nodes"][3].__setitem__(
    "call_target_syntax_id", routine(d, "OverrideSurface")["source_syntax_id"]
))
emit("member-target-and-id", "negative", lambda d: retarget(
    d, "OverrideTarget_Name", "OverrideSurface", "Name", "member"))
emit("constructor-target", "negative", lambda d: main_instruction(
    d, 0
)["expr0_graph"]["nodes"][1].__setitem__(
    "call_target_name", "MissingTarget"
))
emit("role-source-id", "negative", lambda d: declaration(
    d, "role"
).__setitem__("source_syntax_id", declaration(d, "subject")["source_syntax_id"]))
