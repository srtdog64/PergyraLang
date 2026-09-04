"""Metamorphic and fail-closed mutations for the role-override MIR rung."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
target = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))


def declaration(document, kind):
    rows = [row for row in document["decls"] if row["kind"] == kind]
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


emit("declaration-order", "positive", lambda d: d.__setitem__(
    "decls", list(reversed(d["decls"]))
))
emit("routine-order", "positive", lambda d: d.__setitem__(
    "routines", [d["routines"][2], d["routines"][0], d["routines"][1]]
))
emit("both-orders", "positive", lambda d: (
    d.__setitem__("decls", list(reversed(d["decls"]))),
    d.__setitem__(
        "routines", [d["routines"][2], d["routines"][1], d["routines"][0]]
    ),
))

emit("role-name", "negative", lambda d: declaration(d, "role").__setitem__(
    "name", "RenamedRole"
))
emit("subject-name", "negative", lambda d: declaration(
    d, "subject"
).__setitem__("name", "RenamedTarget"))
emit("for-type", "negative", lambda d: declaration(d, "role").__setitem__(
    "for_type", "MissingTarget"
))
emit("role-routine-owner", "negative", lambda d: routine(
    d, "OverrideSurface"
).__setitem__("owner", "OverrideTarget"))
emit("role-method-name", "negative", lambda d: declaration(
    d, "role"
)["methods"][0].__setitem__("name", "Other"))
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
emit("constructor-target", "negative", lambda d: main_instruction(
    d, 0
)["expr0_graph"]["nodes"][1].__setitem__(
    "call_target_name", "MissingTarget"
))
emit("role-source-id", "negative", lambda d: declaration(
    d, "role"
).__setitem__("source_syntax_id", declaration(d, "subject")["source_syntax_id"]))
