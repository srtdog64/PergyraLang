"""Refusal-only mutations of the existing legacy Intent MIR fixture.

The shell gate owns admission and asserts controlled failure without an artifact.
No mutated program is compiled to an executable or run.
"""
import copy
import json
import os
import sys

source, output_dir = sys.argv[1:]
with open(source, encoding="utf-8-sig") as stream:
    baseline = json.load(stream)

def intent_rows(document):
    routine = next(row for row in document["routines"]
                   if row.get("kind") == "intent")
    return [instruction for block in routine["blocks"]
            for instruction in block["instructions"]]


def intent_block(document, role):
    routine = next(row for row in document["routines"] if row.get("kind") == "intent")
    return next(block for block in routine["blocks"] if block.get("intent_block_role") == role)


def bindings(document):
    return [row for row in intent_rows(document) if row.get("name") == "IntentBinding"]


def mirror(document, binding):
    kind = "IntentParticipant" if binding["slot_anchor"] == "participant" else "IntentValue"
    return next(row for row in intent_rows(document)
                if row.get("name") == kind and row.get("arg0") == binding["arg0"])


# Positive IDs originate in each producer; agreement is relational, not an
# assertion that different parsers assign the same numeric source IDs.
baseline_bindings = bindings(baseline)
assert len(baseline_bindings) == 2
baseline_ids = [row["binding_source_syntax_id"] for row in baseline_bindings]
assert all(type(value) is int and value > 0 for value in baseline_ids)
assert len(set(baseline_ids)) == len(baseline_ids)
assert all(mirror(baseline, row)["binding_source_syntax_id"] == row["binding_source_syntax_id"]
           for row in baseline_bindings)
for routine in baseline["routines"]:
    roles = [block.get("intent_block_role") for block in routine["blocks"]]
    if routine.get("kind") == "intent":
        assert all(role in ("entry", "body", "cleanup", "rollback", "invalidation", "execution", "mirror") for role in roles)
        assert all(roles.count(role) == 1 for role in ("entry", "cleanup", "rollback", "invalidation"))
    else:
        assert all(role is None for role in roles)

def write(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    path = os.path.join(output_dir, name + ".mir.json")
    with open(path, "w", encoding="utf-8", newline="\n") as stream:
        json.dump(document, stream, separators=(",", ":"))
        stream.write("\n")

def duplicate_mode(document):
    routine = next(row for row in document["routines"]
                   if row.get("kind") == "intent")
    block = routine["blocks"][0]
    row = next(item for item in block["instructions"]
               if item.get("name") == "IntentMode")
    duplicate = copy.deepcopy(row)
    duplicate["id"] = 999991
    block["instructions"].insert(2, duplicate)

def invalid_priority(document):
    row = next(item for item in intent_rows(document)
               if item.get("name") == "IntentEval" and
               item.get("arg0") == "priority")
    row["expr0"] = "not-an-int"
    row["expr0_graph"]["nodes"][0]["text"] = "not-an-int"

def drift_zone_field(document):
    zone = next(row for row in document["decls"]
                if row.get("nominal_kind") == "zone")
    zone["fields"][0]["name"] = "other_counter"

def drift_action_target(document):
    method = next(row for row in document["routines"]
                  if row.get("kind") == "method")
    method["blocks"][0]["instructions"][0]["expr1"] = "other_value"

def remove_invalidation_cleanup(document):
    block = intent_block(document, "invalidation")
    block["instructions"] = [row for row in block["instructions"]
                             if row.get("name") != "DetachInvalidation" or
                             row.get("arg1") != "increment"]


def crossed_cleanup_roles(document):
    rollback = intent_block(document, "rollback")
    invalidation = intent_block(document, "invalidation")
    rollback["intent_block_role"], invalidation["intent_block_role"] = "invalidation", "rollback"


def main_call_nodes(document):
    routine = next(row for row in document["routines"] if row["name"] == "Main")
    return routine["blocks"][0]["instructions"][2]["expr0_graph"]["nodes"]


def crossed_formal_order(document):
    routine = next(row for row in document["routines"] if row.get("kind") == "intent")
    rows = routine["blocks"][0]["instructions"]
    positions = [index for index, row in enumerate(rows) if row.get("name") == "IntentBinding"]
    assert len(positions) == 2
    first, second = positions
    rows[first], rows[second] = rows[second], rows[first]


def crossed_call_arguments(document):
    nodes = main_call_nodes(document)
    nodes[2]["text"], nodes[4]["text"] = nodes[4]["text"], nodes[2]["text"]


def duplicate_binding_id(document):
    first, second = bindings(document)
    second["binding_source_syntax_id"] = first["binding_source_syntax_id"]
    mirror(document, second)["binding_source_syntax_id"] = first["binding_source_syntax_id"]


def crossed_mirror_id(document):
    first, second = bindings(document)
    mirror(document, first)["binding_source_syntax_id"] = second["binding_source_syntax_id"]


def orphan_binding_mirror(document):
    routine = next(row for row in document["routines"] if row.get("kind") == "intent")
    duplicate = copy.deepcopy(mirror(document, bindings(document)[0]))
    duplicate["id"] = 999992
    duplicate["arg0"] = "orphan"
    routine["blocks"][0]["instructions"].insert(2, duplicate)


def nonbinding_identity(document):
    row = next(item for item in intent_rows(document) if item.get("name") == "CommitIntent")
    row["binding_source_syntax_id"] = bindings(document)[0]["binding_source_syntax_id"]


for name, mutation in (
    ("old-subject-wire", lambda d: d["decls"][0].update(kind="subject")),
    ("crossed-nominal-kind", lambda d: d["decls"][0].update(nominal_kind="class")),
    ("duplicate-mode", duplicate_mode),
    ("invalid-priority", invalid_priority),
    ("zone-field-drift", drift_zone_field),
    ("action-target-drift", drift_action_target),
    ("missing-invalidation-cleanup", remove_invalidation_cleanup),
    ("missing-call-target-id", lambda d: main_call_nodes(d)[1].update(call_target_syntax_id=0)),
    ("crossed-call-target-id", lambda d: main_call_nodes(d)[1].update(
        call_target_syntax_id=next(r["source_syntax_id"] for r in d["routines"] if r["kind"] == "method"))),
    ("crossed-call-binder-id", lambda d: main_call_nodes(d)[0].update(
        binding_syntax_id=next(r["source_syntax_id"] for r in d["routines"] if r["kind"] == "method"))),
    ("crossed-formal-order", crossed_formal_order),
    ("crossed-call-arguments", crossed_call_arguments),
    ("missing-binding-id", lambda d: bindings(d)[0].pop("binding_source_syntax_id")),
    ("zero-binding-id", lambda d: bindings(d)[0].update(binding_source_syntax_id=0)),
    ("string-binding-id", lambda d: bindings(d)[0].update(binding_source_syntax_id="17")),
    ("duplicate-binding-id", duplicate_binding_id),
    ("crossed-mirror-id", crossed_mirror_id),
    ("missing-mirror-id", lambda d: mirror(d, bindings(d)[0]).pop("binding_source_syntax_id")),
    ("orphan-binding-mirror", orphan_binding_mirror),
    ("nonbinding-identity", nonbinding_identity),
    ("missing-block-role", lambda d: intent_block(d, "entry").pop("intent_block_role")),
    ("mistyped-block-role", lambda d: intent_block(d, "entry").update(intent_block_role=0)),
    ("unknown-block-role", lambda d: intent_block(d, "entry").update(intent_block_role="unknown")),
    ("duplicate-root-block-role", lambda d: intent_block(d, "cleanup").update(intent_block_role="entry")),
    ("crossed-cleanup-block-role", crossed_cleanup_roles),
    ("cyclic-body-block-role", lambda d: intent_block(d, "entry").update(succ_true=intent_block(d, "entry")["id"])),
    ("nonintent-block-role", lambda d: next(r for r in d["routines"] if r["name"] == "Main")["blocks"][0].update(intent_block_role="entry")),
):
    write(name, mutation)

# A JSON object cannot model a duplicate key; encode this refusal input directly.
wire = json.dumps(baseline, separators=(",", ":"))
field = '"binding_source_syntax_id":' + str(baseline_ids[0])
assert field in wire
with open(os.path.join(output_dir, "duplicate-binding-key.mir.json"), "w",
          encoding="utf-8", newline="\n") as stream:
    stream.write(wire.replace(field, field + "," + field, 1) + "\n")
role_field = '"intent_block_role":"entry"'
assert role_field in wire
with open(os.path.join(output_dir, "duplicate-block-role-key.mir.json"), "w",
          encoding="utf-8", newline="\n") as stream:
    stream.write(wire.replace(role_field, role_field + "," + role_field, 1) + "\n")
