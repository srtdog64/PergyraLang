#!/usr/bin/env python3
"""Canonical native wire audit and intent_execution mutation-corpus owner."""
import copy
import json
import pathlib
import sys

def generate_multi_source(source_arg, output_arg):
    source_path = pathlib.Path(source_arg)
    output_path = pathlib.Path(output_arg)
    source = source_path.read_text(encoding="utf-8")
    start = source.index("intent RunWorkflow(")
    body_start = source.index("{", start)
    depth = 0
    end = None
    for cursor in range(body_start, len(source)):
        if source[cursor] == "{":
            depth += 1
        elif source[cursor] == "}":
            depth -= 1
            if depth == 0:
                end = cursor + 1
                break
    assert end is not None
    second = source[start:end].replace(
        "intent RunWorkflow(", "intent RunWorkflowAgain(", 1
    )
    insert = source.index("\nfunc Observe", end)
    output_path.write_text(
        source[:insert] + "\n\n" + second + source[insert:],
        encoding="utf-8",
        newline="\n",
    )


if len(sys.argv) >= 2 and sys.argv[1] == "generate-multi-source":
    assert len(sys.argv) == 4, sys.argv
    generate_multi_source(sys.argv[2], sys.argv[3])
    raise SystemExit(0)

assert len(sys.argv) == 8 and sys.argv[1] == "build-corpus", sys.argv
(
    valid_path, multi_valid_path, multi_interleaved_path,
    build_dir_arg, join_map_arg, manifest_arg,
) = sys.argv[2:]
build_dir = pathlib.Path(build_dir_arg)
join_map_path = pathlib.Path(join_map_arg)
manifest_path = pathlib.Path(manifest_arg)

with open(valid_path, encoding="utf-8") as stream:
    base = json.load(stream)
with open(multi_valid_path, encoding="utf-8") as stream:
    multi = json.load(stream)

INTENT_SCHEMA = "pgy.selfhost.mir-intent-execution-plan.v3"
INTENT_KEYS = {"schema", "plan_digest", "steps", "terminals"}
STEP_KEYS = {
    "transition_id", "routine_syntax_id", "step_syntax_id", "step_name",
    "has_predecessor", "predecessor_transition_id",
    "predecessor_step_syntax_id", "predecessor_step_name",
    "action_syntax_id", "outcome_instruction_block_id",
    "outcome_instruction_id", "outcome_result_name", "outcome_type_name",
    "outcome_enum_name", "outcome_enum_syntax_id", "branch_block_id",
    "branch_instruction_id", "success_variant_index",
    "success_variant_name", "success_payload_name",
    "success_payload_type_name", "success_payload_decl_syntax_id",
    "success_successor_block_id",
    "failure_variant_index", "failure_variant_name", "failure_payload_name",
    "failure_payload_type_name", "failure_payload_decl_syntax_id",
    "failure_successor_block_id",
    "completion_block_id", "completion_instruction_id", "compensations",
    "where_zone_name", "where_zone_syntax_id",
}
COMPENSATION_KEYS = {
    "transition_id", "expression_syntax_id", "instruction_block_id",
    "instruction_id", "graph_root_id", "graph_digest", "call_target_name",
    "call_target_syntax_id",
}
TERMINAL_KEYS = {
    "terminal_transition_id", "routine_syntax_id", "role",
    "source_transition_id", "source_step_syntax_id", "source_step_name",
    "source_variant_index", "source_variant_name", "source_payload_name",
    "source_payload_type_name", "source_payload_decl_syntax_id",
    "result_instruction_block_id",
    "result_instruction_id", "result_definition_name", "result_type_name",
    "result_enum_name", "result_enum_syntax_id", "result_variant_index",
    "result_variant_name", "result_payload_name", "result_payload_type_name",
    "result_payload_decl_syntax_id",
    "expression_syntax_id", "graph_root_id", "graph_digest",
}

MODULUS = 268435456

def exact_keys(value, expected, label):
    assert isinstance(value, dict), (label, type(value))
    actual = set(value)
    assert actual == expected, (
        label,
        "missing=" + ",".join(sorted(expected - actual)),
        "extra=" + ",".join(sorted(actual - expected)),
    )

def positive_int(value, label):
    assert type(value) is int and value > 0, (label, value)
    return value

def nonnegative_int(value, label):
    assert type(value) is int and value >= 0, (label, value)
    return value

def nonempty_string(value, label):
    assert isinstance(value, str) and value != "", (label, value)
    return value

def unique_index(rows, key, label):
    result = {}
    for row in rows:
        value = row[key]
        assert value not in result, (label, value)
        result[value] = row
    return result

def hash_string(hash_value, value):
    payload = ("" if value is None else value).encode("utf-8")
    hash_value = (hash_value * 131 + len(payload)) % MODULUS
    for byte in payload:
        hash_value = (hash_value * 131 + byte) % MODULUS
    return hash_value

def hash_int(hash_value, value):
    assert type(value) is int, value
    return (hash_value * 131 + value + 2) % MODULUS

def hash_step(hash_value, row):
    for key, kind in (
        ("transition_id", "int"), ("routine_syntax_id", "int"),
        ("step_syntax_id", "int"), ("step_name", "string"),
    ):
        hash_value = hash_int(hash_value, row[key]) if kind == "int" \
            else hash_string(hash_value, row[key])
    hash_value = hash_int(hash_value, 1 if row["has_predecessor"] else 0)
    for key, kind in (
        ("predecessor_transition_id", "int"),
        ("predecessor_step_syntax_id", "int"),
        ("predecessor_step_name", "string"), ("action_syntax_id", "int"),
        ("outcome_instruction_block_id", "int"),
        ("outcome_instruction_id", "int"),
        ("outcome_result_name", "string"), ("outcome_type_name", "string"),
        ("outcome_enum_name", "string"), ("outcome_enum_syntax_id", "int"),
        ("branch_block_id", "int"), ("branch_instruction_id", "int"),
        ("success_variant_index", "int"), ("success_variant_name", "string"),
        ("success_payload_name", "string"),
        ("success_payload_type_name", "string"),
        ("success_payload_decl_syntax_id", "int"),
        ("success_successor_block_id", "int"),
        ("failure_variant_index", "int"), ("failure_variant_name", "string"),
        ("failure_payload_name", "string"),
        ("failure_payload_type_name", "string"),
        ("failure_payload_decl_syntax_id", "int"),
        ("failure_successor_block_id", "int"),
        ("completion_block_id", "int"), ("completion_instruction_id", "int"),
    ):
        hash_value = hash_int(hash_value, row[key]) if kind == "int" \
            else hash_string(hash_value, row[key])
    hash_value = hash_int(hash_value, len(row["compensations"]))
    for fact in row["compensations"]:
        for key, kind in (
            ("transition_id", "int"), ("expression_syntax_id", "int"),
            ("instruction_block_id", "int"), ("instruction_id", "int"),
            ("graph_root_id", "int"), ("graph_digest", "int"),
            ("call_target_name", "string"),
            ("call_target_syntax_id", "int"),
        ):
            hash_value = hash_int(hash_value, fact[key]) if kind == "int" \
                else hash_string(hash_value, fact[key])
    hash_value = hash_string(hash_value, row["where_zone_name"])
    hash_value = hash_int(hash_value, row["where_zone_syntax_id"])
    return hash_value

def hash_terminal(hash_value, row):
    for key, kind in (
        ("terminal_transition_id", "int"), ("routine_syntax_id", "int"),
        ("role", "string"), ("source_transition_id", "int"),
        ("source_step_syntax_id", "int"), ("source_step_name", "string"),
        ("source_variant_index", "int"), ("source_variant_name", "string"),
        ("source_payload_name", "string"),
        ("source_payload_type_name", "string"),
        ("source_payload_decl_syntax_id", "int"),
        ("result_instruction_block_id", "int"),
        ("result_instruction_id", "int"),
        ("result_definition_name", "string"), ("result_type_name", "string"),
        ("result_enum_name", "string"), ("result_enum_syntax_id", "int"),
        ("result_variant_index", "int"), ("result_variant_name", "string"),
        ("result_payload_name", "string"),
        ("result_payload_type_name", "string"),
        ("result_payload_decl_syntax_id", "int"),
        ("expression_syntax_id", "int"), ("graph_root_id", "int"),
        ("graph_digest", "int"),
    ):
        hash_value = hash_int(hash_value, row[key]) if kind == "int" \
            else hash_string(hash_value, row[key])
    return hash_value

def program_digest(document):
    execution = document["intent_execution"]
    hash_value = hash_string(71, execution["schema"])
    row_routine_ids = {
        row["routine_syntax_id"] for row in execution["steps"]
    } | {
        row["routine_syntax_id"] for row in execution["terminals"]
    }
    ordered_ids = [
        row["source_syntax_id"] for row in document["routines"]
        if row.get("source_syntax_id") in row_routine_ids
    ]
    assert len(ordered_ids) == len(set(ordered_ids)), ordered_ids
    assert set(ordered_ids) == row_routine_ids, (ordered_ids, row_routine_ids)
    for routine_id in ordered_ids:
        for row in execution["steps"]:
            if row["routine_syntax_id"] == routine_id:
                hash_value = hash_step(hash_value, row)
        for row in execution["terminals"]:
            if row["routine_syntax_id"] == routine_id:
                hash_value = hash_terminal(hash_value, row)
    return 1073741824 + hash_value

def block_index(routine):
    return unique_index(routine["blocks"], "id", "routine block id")

def instruction_at(routine, block_id, instruction_id):
    blocks = block_index(routine)
    assert block_id in blocks, (routine["name"], block_id)
    matches = [
        row for row in blocks[block_id]["instructions"]
        if row["id"] == instruction_id
    ]
    assert len(matches) == 1, (
        routine["name"], block_id, instruction_id, len(matches)
    )
    return blocks[block_id], matches[0]

def variant_at(declaration, index):
    variants = declaration.get("variants")
    assert isinstance(variants, list) and 0 <= index < len(variants), (
        declaration.get("name"), index
    )
    return variants[index]

def payload_tobject(declarations_by_id, payload_type, payload_decl_syntax_id):
    declaration = declarations_by_id[positive_int(
        payload_decl_syntax_id, "payload declaration syntax id"
    )]
    assert declaration.get("kind") == "class", declaration
    assert declaration.get("nominal_kind") == "tobject", declaration
    assert declaration.get("name") == payload_type, declaration

def graph_calls(instruction, target_name):
    graph = instruction.get("expr0_graph")
    return isinstance(graph, dict) and any(
        node.get("kind") == "call"
        and node.get("call_target_name") == target_name
        for node in graph.get("nodes", [])
    )

assert base.get("schema") == "pgy.mir.v1", base.get("schema")
assert isinstance(base.get("decls"), list)
assert isinstance(base.get("routines"), list)
exact_keys(base["intent_execution"], INTENT_KEYS, "intent_execution")
execution = base["intent_execution"]
assert execution["schema"] == INTENT_SCHEMA, execution["schema"]
assert isinstance(execution["steps"], list) and len(execution["steps"]) == 2
assert isinstance(execution["terminals"], list) \
    and len(execution["terminals"]) == 3
for row_index, row in enumerate(execution["steps"]):
    exact_keys(row, STEP_KEYS, f"step[{row_index}]")
    assert type(row["has_predecessor"]) is bool
    for fact_index, fact in enumerate(row["compensations"]):
        exact_keys(
            fact, COMPENSATION_KEYS,
            f"step[{row_index}].compensations[{fact_index}]",
        )
for row_index, row in enumerate(execution["terminals"]):
    exact_keys(row, TERMINAL_KEYS, f"terminal[{row_index}]")
assert execution["plan_digest"] == program_digest(base), (
    execution["plan_digest"], program_digest(base)
)

assert multi.get("schema") == "pgy.mir.v1"
multi_execution = multi.get("intent_execution")
assert isinstance(multi_execution, dict)
exact_keys(multi_execution, INTENT_KEYS, "multi.intent_execution")
multi_steps = multi_execution["steps"]
multi_terminals = multi_execution["terminals"]
multi_routine_ids = list(dict.fromkeys(
    row["routine_syntax_id"] for row in multi_steps
))
assert len(multi_routine_ids) == 2, multi_routine_ids
assert all(
    sum(row["routine_syntax_id"] == routine_id for row in multi_steps) == 2
    for routine_id in multi_routine_ids
)
assert all(
    sum(row["routine_syntax_id"] == routine_id for row in multi_terminals)
        == 3
    for routine_id in multi_routine_ids
)
assert multi_execution["plan_digest"] == program_digest(multi), (
    multi_execution["plan_digest"], program_digest(multi)
)

legacy_hash = hash_string(71, multi_execution["schema"])
for row in multi_steps:
    legacy_hash = hash_step(legacy_hash, row)
for row in multi_terminals:
    legacy_hash = hash_terminal(legacy_hash, row)
legacy_digest = 1073741824 + legacy_hash
assert legacy_digest != multi_execution["plan_digest"], (
    "multi-routine fixture does not falsify flat steps-then-terminals digest",
    legacy_digest,
)

def round_robin_by_routine(rows, routine_ids):
    buckets = [
        [row for row in rows if row["routine_syntax_id"] == routine_id]
        for routine_id in routine_ids
    ]
    result = []
    for offset in range(max(len(bucket) for bucket in buckets)):
        for bucket in buckets:
            if offset < len(bucket):
                result.append(bucket[offset])
    return result

multi_interleaved = copy.deepcopy(multi)
interleaved_execution = multi_interleaved["intent_execution"]
interleaved_execution["steps"] = round_robin_by_routine(
    interleaved_execution["steps"], multi_routine_ids
)
interleaved_execution["terminals"] = round_robin_by_routine(
    interleaved_execution["terminals"], multi_routine_ids
)
assert [
    row["routine_syntax_id"]
    for row in interleaved_execution["steps"]
] != [row["routine_syntax_id"] for row in multi_steps]
interleaved_execution["plan_digest"] = program_digest(multi_interleaved)
assert interleaved_execution["plan_digest"] == \
    multi_execution["plan_digest"]
with open(multi_interleaved_path, "w", encoding="utf-8") as stream:
    json.dump(multi_interleaved, stream, separators=(",", ":"))
    stream.write("\n")

routines_by_id = unique_index(
    base["routines"], "source_syntax_id", "routine source_syntax_id"
)
declarations_by_id = unique_index(
    base["decls"], "source_syntax_id", "declaration source_syntax_id"
)
declarations_by_name = unique_index(base["decls"], "name", "declaration name")
steps_by_transition = unique_index(
    execution["steps"], "transition_id", "step transition id"
)
steps_by_syntax = unique_index(
    execution["steps"], "step_syntax_id", "step syntax id"
)

roots_by_routine = {}
for row in execution["steps"]:
    transition_id = positive_int(row["transition_id"], "transition_id")
    assert transition_id == positive_int(row["step_syntax_id"], "step_syntax_id")
    routine_id = positive_int(row["routine_syntax_id"], "routine_syntax_id")
    routine = routines_by_id[routine_id]
    assert routine["kind"] == "intent", routine
    step_name = nonempty_string(row["step_name"], "step_name")
    where_zone_name = nonempty_string(
        row["where_zone_name"], "where_zone_name"
    )
    where_zone_id = positive_int(
        row["where_zone_syntax_id"], "where_zone_syntax_id"
    )
    where_zone = declarations_by_id[where_zone_id]
    assert where_zone["kind"] == "class", where_zone
    assert where_zone.get("nominal_kind") == "zone", where_zone
    assert where_zone["name"] == where_zone_name, (where_zone, row)
    action_id = positive_int(row["action_syntax_id"], "action_syntax_id")
    action = routines_by_id[action_id]
    assert action["kind"] == "method" and action.get("owner"), action
    assert action.get("return") == row["outcome_type_name"], (action, row)
    owner = declarations_by_name[action["owner"]]
    method_rows = [
        method for method in owner.get("methods", [])
        if method.get("name") == action["name"]
        and method.get("callable_kind") == "action"
        and method.get("return") == action.get("return")
    ]
    assert len(method_rows) == 1, (owner["name"], action["name"])
    assert method_rows[0]["contract"]["within"] == where_zone_name, (
        method_rows[0], row
    )

    has_predecessor = row["has_predecessor"]
    predecessor_id = row["predecessor_transition_id"]
    if has_predecessor:
        positive_int(predecessor_id, "predecessor_transition_id")
        predecessor = steps_by_transition[predecessor_id]
        assert predecessor["step_syntax_id"] == row["predecessor_step_syntax_id"]
        assert predecessor["step_name"] == row["predecessor_step_name"]
        assert predecessor["routine_syntax_id"] == routine_id
    else:
        assert predecessor_id == 0
        assert row["predecessor_step_syntax_id"] == 0
        assert row["predecessor_step_name"] is None
        roots_by_routine[routine_id] = roots_by_routine.get(routine_id, 0) + 1

    enum_id = positive_int(row["outcome_enum_syntax_id"], "outcome_enum_syntax_id")
    enum_decl = declarations_by_id[enum_id]
    assert enum_decl["kind"] == "enum"
    assert enum_decl["name"] == row["outcome_enum_name"]
    assert row["outcome_enum_name"] == row["outcome_type_name"]
    for prefix in ("success", "failure"):
        index = nonnegative_int(row[f"{prefix}_variant_index"], f"{prefix} variant")
        variant = variant_at(enum_decl, index)
        assert variant["name"] == row[f"{prefix}_variant_name"]
        assert variant["param_count"] == 1
        assert variant["param_types"] == [row[f"{prefix}_payload_type_name"]]
        payload_tobject(
            declarations_by_id,
            row[f"{prefix}_payload_type_name"],
            row[f"{prefix}_payload_decl_syntax_id"],
        )
        nonempty_string(row[f"{prefix}_payload_name"], f"{prefix} payload")

    outcome_block, outcome = instruction_at(
        routine, row["outcome_instruction_block_id"],
        row["outcome_instruction_id"],
    )
    assert outcome["kind"] == "stmt" and outcome["name"] == "IntentEval"
    assert outcome["arg0"] == "on" and outcome["arg1"] == step_name
    assert outcome["result"] == row["outcome_result_name"]
    assert outcome["abi_type_name"] == row["outcome_type_name"]
    assert graph_calls(outcome, action["name"]), (step_name, action["name"])

    branch_block, branch = instruction_at(
        routine, row["branch_block_id"], row["branch_instruction_id"]
    )
    assert outcome_block["id"] == branch_block["id"]
    assert branch["kind"] == "branch" and branch["name"] == "IntentOutcomeBranch"
    assert branch["arg0"] == row["success_variant_name"]
    assert branch["arg1"] == row["failure_variant_name"]
    assert branch_block.get("succ_true") == row["success_successor_block_id"]
    assert branch_block.get("succ_false") == row["failure_successor_block_id"]
    assert branch.get("expr0_graph", {}).get("digest") \
        == outcome.get("expr0_graph", {}).get("digest")

    completion_block, completion = instruction_at(
        routine, row["completion_block_id"], row["completion_instruction_id"]
    )
    assert row["completion_block_id"] == row["success_successor_block_id"]
    assert completion["kind"] == "stmt"
    assert completion["name"] == "IntentStepCompleted"
    assert completion["arg0"] == row["success_variant_name"]
    assert completion["arg1"] == step_name
    failure_block = block_index(routine)[row["failure_successor_block_id"]]
    assert not any(
        instruction.get("name") == "IntentStepCompleted"
        and instruction.get("arg1") == step_name
        for instruction in failure_block["instructions"]
    )

    for fact in row["compensations"]:
        assert fact["transition_id"] == transition_id
        compensation_block, compensation = instruction_at(
            routine, fact["instruction_block_id"], fact["instruction_id"]
        )
        assert compensation["kind"] == "stmt"
        assert compensation["name"] == "IntentEval"
        assert compensation["arg0"] == "compensate"
        assert compensation["arg1"] == step_name
        graph = compensation["expr0_graph"]
        assert graph["root"] == fact["graph_root_id"]
        assert graph["digest"] == fact["graph_digest"]
        callee = routines_by_id[positive_int(
            fact["call_target_syntax_id"], "compensation callee id"
        )]
        assert callee["kind"] == "method"
        assert callee["name"] == fact["call_target_name"]
        assert graph_calls(compensation, callee["name"])
        assert compensation_block["reachable"] is False

assert all(count == 1 for count in roots_by_routine.values()), roots_by_routine
assert set(roots_by_routine) == {
    row["routine_syntax_id"] for row in execution["steps"]
}

terminal_ids = set()
terminal_expression_ids = set()
coverage = {
    transition_id: {"success": 0, "failure": 0}
    for transition_id in steps_by_transition
}
for row in execution["terminals"]:
    terminal_id = positive_int(row["terminal_transition_id"], "terminal id")
    assert terminal_id not in terminal_ids
    terminal_ids.add(terminal_id)
    expression_id = positive_int(row["expression_syntax_id"], "terminal expression id")
    assert expression_id not in terminal_expression_ids
    terminal_expression_ids.add(expression_id)
    assert terminal_id == expression_id
    routine = routines_by_id[positive_int(row["routine_syntax_id"], "terminal routine")]
    assert routine["kind"] == "intent"
    source = steps_by_transition[positive_int(
        row["source_transition_id"], "terminal source transition"
    )]
    assert source["routine_syntax_id"] == row["routine_syntax_id"]
    assert source["step_syntax_id"] == row["source_step_syntax_id"]
    assert source["step_name"] == row["source_step_name"]
    role = row["role"]
    assert role in ("success", "failure")
    coverage[source["transition_id"]][role] += 1
    assert row["source_variant_index"] == source[f"{role}_variant_index"]
    assert row["source_variant_name"] == source[f"{role}_variant_name"]
    assert row["source_payload_name"] == source[f"{role}_payload_name"]
    assert row["source_payload_type_name"] == source[f"{role}_payload_type_name"]
    assert row["source_payload_decl_syntax_id"] == \
        source[f"{role}_payload_decl_syntax_id"]
    assert row["result_payload_name"] == row["source_payload_name"]
    assert row["result_payload_type_name"] == row["source_payload_type_name"]
    assert row["result_payload_decl_syntax_id"] == \
        row["source_payload_decl_syntax_id"]
    assert routine["return"] == row["result_type_name"]
    result_enum = declarations_by_id[positive_int(
        row["result_enum_syntax_id"], "result enum id"
    )]
    assert result_enum["kind"] == "enum"
    assert result_enum["name"] == row["result_enum_name"]
    assert row["result_enum_name"] == row["result_type_name"]
    result_variant = variant_at(result_enum, row["result_variant_index"])
    assert result_variant["name"] == row["result_variant_name"]
    assert result_variant["param_count"] == 1
    assert result_variant["param_types"] == [row["result_payload_type_name"]]
    payload_tobject(
        declarations_by_id, row["result_payload_type_name"],
        row["result_payload_decl_syntax_id"],
    )
    result_block, result = instruction_at(
        routine, row["result_instruction_block_id"], row["result_instruction_id"]
    )
    assert result["kind"] == "return" and result["name"] == "IntentTerminalResult"
    assert result["result"] == row["result_definition_name"]
    assert result["abi_type_name"] == row["result_type_name"]
    assert result["arg0"] == row["result_variant_name"]
    assert result["arg1"] == row["result_payload_name"]
    assert result["expr0_graph"]["root"] == row["graph_root_id"]
    assert result["expr0_graph"]["digest"] == row["graph_digest"]
    assert result_block["reachable"] is True

children = {transition_id: 0 for transition_id in steps_by_transition}
for row in execution["steps"]:
    predecessor = row["predecessor_transition_id"]
    if predecessor != 0:
        children[predecessor] += 1
for transition_id, counts in coverage.items():
    assert counts["failure"] == 1, (transition_id, counts)
    assert counts["success"] == (0 if children[transition_id] else 1), (
        transition_id, counts, children
    )

JOIN_MAP = [
    ("intent_execution.schema", "native schema owner",
     "exact pgy.selfhost.mir-intent-execution-plan.v3"),
    ("intent_execution.plan_digest", "native program digest",
     "schema seed; routine inventory order; steps then terminals per routine"),
    ("step.transition_id", "step syntax identity",
     "positive, unique, exactly step_syntax_id"),
    ("step.routine_syntax_id", "routines[].source_syntax_id",
     "unique kind=intent routine; shared by its steps and terminals"),
    ("step.has_predecessor", "wire-only predecessor presence",
     "must equal nonzero predecessor handle; root false+0+0+null"),
    ("step.predecessor_*", "prior step exact identity",
     "transition_id + step_syntax_id + step_name in the same routine; no row-order fallback"),
    ("step.action_syntax_id", "method routine source identity",
     "unique kind=method; owner method is action; return equals outcome type"),
    ("step.where_zone_*", "zone declaration identity",
     "exact zone source_syntax_id + name; equals action contract within"),
    ("step.outcome_instruction_*", "intent routine block/instruction",
     "unique IntentEval(on), exact step/result/type and action graph target"),
    ("step.outcome_enum_*", "decls enum identity",
     "source_syntax_id + name; exact action result type"),
    ("step.success/failure_variant_*", "enum variant table",
     "zero-based index + name + exactly one payload type"),
    ("step.success/failure_payload_*", "exact tobject declaration",
     "payload_decl_syntax_id + exact name to kind=class, nominal_kind=tobject"),
    ("step.branch_* and successor_*", "intent CFG",
     "unique IntentOutcomeBranch and exact true/false successors"),
    ("step.completion_*", "success successor instruction",
     "one IntentStepCompleted on success only; none on failure"),
    ("compensation.transition_id", "owning step transition",
     "exact transition handle, never row position"),
    ("compensation.instruction_*", "intent routine block/instruction",
     "unique IntentEval(compensate), exact step and expression graph"),
    ("compensation.call_target_*", "method routine source identity",
     "stable ID + exact method name + graph call target"),
    ("terminal.terminal_transition_id", "terminal expression identity",
     "positive unique producer identity, equal expression_syntax_id"),
    ("terminal.source_*", "source step/branch",
     "exact step identity plus role-selected variant/payload declaration ID"),
    ("terminal.result_instruction_*", "intent routine return instruction",
     "unique IntentTerminalResult and exact result definition/type/graph"),
    ("terminal.result_enum_*", "decls enum identity",
     "source_syntax_id + name equals intent routine return type"),
    ("terminal.result_variant/payload_*", "result enum variant + tobject",
     "index/name/one payload, exact tobject ID, and source payload continuity"),
    ("terminal coverage", "per-routine transition graph",
     "one failure per step; one success only for a leaf step"),
]
with open(join_map_path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write("wire_field\towner_join\texact_rule\n")
    for row in JOIN_MAP:
        stream.write("\t".join(row) + "\n")

for old_path in build_dir.glob("negative-*.mir.json"):
    old_path.unlink()

mutations = []

def add_mutation(name, category, mutate, rehash=True):
    document = copy.deepcopy(base)
    mutate(document)
    if rehash and "intent_execution" in document:
        document["intent_execution"]["plan_digest"] = program_digest(document)
    path = build_dir / f"negative-{name}.mir.json"
    path.write_text(
        json.dumps(document, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    mutations.append((path.name, category))

add_mutation(
    "missing-intent-execution", "schema",
    lambda document: document.pop("intent_execution"), rehash=False,
)
add_mutation(
    "unknown-schema", "schema",
    lambda document: document["intent_execution"].__setitem__(
        "schema", "pgy.selfhost.mir-intent-execution-plan.v999"
    ),
)
add_mutation(
    "unexpected-step-field", "schema",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "source_order", 0
    ),
)
add_mutation(
    "missing-step-syntax-id", "missing-stable-id",
    lambda document: document["intent_execution"]["steps"][0].pop(
        "step_syntax_id"
    ), rehash=False,
)
add_mutation(
    "missing-step-zone-id", "missing-stable-id",
    lambda document: document["intent_execution"]["steps"][0].pop(
        "where_zone_syntax_id"
    ), rehash=False,
)
add_mutation(
    "empty-step-zone-name", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "where_zone_name", ""
    ),
)
add_mutation(
    "crosswired-step-zone-id", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "where_zone_syntax_id",
        document["intent_execution"]["steps"][0][
            "success_payload_decl_syntax_id"
        ],
    ),
)
add_mutation(
    "zero-action-syntax-id", "missing-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "action_syntax_id", 0
    ),
)
add_mutation(
    "missing-compensation-callee-id", "missing-stable-id",
    lambda document: document["intent_execution"]["steps"][0]
        ["compensations"][0].pop("call_target_syntax_id"), rehash=False,
)
add_mutation(
    "missing-terminal-enum-id", "missing-stable-id",
    lambda document: document["intent_execution"]["terminals"][0].pop(
        "result_enum_syntax_id"
    ), rehash=False,
)
for role, row_kind, row_index in (
    ("success_payload_decl_syntax_id", "steps", 0),
    ("failure_payload_decl_syntax_id", "steps", 0),
    ("source_payload_decl_syntax_id", "terminals", 0),
    ("result_payload_decl_syntax_id", "terminals", 0),
):
    add_mutation(
        f"missing-{role.replace('_', '-')}", "missing-stable-id",
        lambda document, key=role, kind=row_kind, index=row_index:
            document["intent_execution"][kind][index].pop(key),
        rehash=False,
    )
add_mutation(
    "duplicate-transition-id", "duplicate-stable-id",
    lambda document: document["intent_execution"]["steps"][1].update({
        "transition_id": document["intent_execution"]["steps"][0]["transition_id"],
        "step_syntax_id": document["intent_execution"]["steps"][0]["step_syntax_id"],
    }),
)
add_mutation(
    "duplicate-terminal-id", "duplicate-stable-id",
    lambda document: document["intent_execution"]["terminals"][1].__setitem__(
        "terminal_transition_id",
        document["intent_execution"]["terminals"][0]["terminal_transition_id"],
    ),
)
add_mutation(
    "duplicate-routine-id", "duplicate-stable-id",
    lambda document: document["routines"][1].__setitem__(
        "source_syntax_id", document["routines"][0]["source_syntax_id"]
    ), rehash=False,
)
add_mutation(
    "crosswired-routine-id", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "routine_syntax_id",
        document["intent_execution"]["steps"][0]["action_syntax_id"],
    ),
)
add_mutation(
    "crosswired-action-id", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "action_syntax_id",
        document["intent_execution"]["steps"][1]["action_syntax_id"],
    ),
)
add_mutation(
    "crosswired-outcome-enum-id", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "outcome_enum_syntax_id",
        document["intent_execution"]["steps"][1]["outcome_enum_syntax_id"],
    ),
)

def foreign_payload_decl_syntax_id(document, current):
    candidates = []
    for step in document["intent_execution"]["steps"]:
        candidates.extend((
            step["success_payload_decl_syntax_id"],
            step["failure_payload_decl_syntax_id"],
        ))
    return next(candidate for candidate in candidates if candidate != current)

for role, row_kind, row_index in (
    ("success_payload_decl_syntax_id", "steps", 0),
    ("failure_payload_decl_syntax_id", "steps", 0),
    ("source_payload_decl_syntax_id", "terminals", 0),
    ("result_payload_decl_syntax_id", "terminals", 0),
):
    def crosswire_payload_id(document, key=role, kind=row_kind, index=row_index):
        row = document["intent_execution"][kind][index]
        row[key] = foreign_payload_decl_syntax_id(document, row[key])
    add_mutation(
        f"crosswired-{role.replace('_', '-')}", "crosswired-stable-id",
        crosswire_payload_id,
    )
add_mutation(
    "crosswired-compensation-callee-id", "crosswired-stable-id",
    lambda document: document["intent_execution"]["steps"][0]
        ["compensations"][0].__setitem__(
            "call_target_syntax_id",
            document["intent_execution"]["steps"][1]
                ["compensations"][0]["call_target_syntax_id"],
        ),
)
add_mutation(
    "success-variant-index", "variant-payload",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "success_variant_index",
        document["intent_execution"]["steps"][0]["failure_variant_index"],
    ),
)
add_mutation(
    "success-payload-type", "variant-payload",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "success_payload_type_name",
        document["intent_execution"]["steps"][0]["failure_payload_type_name"],
    ),
)
def payload_not_tobject(document):
    payload_name = document["intent_execution"]["steps"][0]
    payload_name = payload_name["success_payload_type_name"]
    declaration = next(
        row for row in document["decls"] if row["name"] == payload_name
    )
    declaration["nominal_kind"] = "object"
add_mutation("payload-not-tobject", "variant-payload", payload_not_tobject)

def duplicate_payload_declaration(document):
    payload_name = document["intent_execution"]["steps"][0]
    payload_name = payload_name["success_payload_type_name"]
    declaration = next(
        row for row in document["decls"] if row["name"] == payload_name
    )
    document["decls"].append(copy.deepcopy(declaration))
add_mutation(
    "duplicate-payload-declaration", "variant-payload",
    duplicate_payload_declaration,
)

add_mutation(
    "predecessor-boolean-drift", "predecessor",
    lambda document: document["intent_execution"]["steps"][1].__setitem__(
        "has_predecessor", False
    ),
)
add_mutation(
    "predecessor-cycle", "predecessor",
    lambda document: document["intent_execution"]["steps"][1].update({
        "predecessor_transition_id": document["intent_execution"]["steps"][1]["transition_id"],
        "predecessor_step_syntax_id": document["intent_execution"]["steps"][1]["step_syntax_id"],
        "predecessor_step_name": document["intent_execution"]["steps"][1]["step_name"],
    }),
)
add_mutation(
    "root-predecessor-name", "predecessor",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "predecessor_step_name", document["intent_execution"]["steps"][1]["step_name"]
    ),
)
add_mutation(
    "completion-failure-block", "completion",
    lambda document: document["intent_execution"]["steps"][0].__setitem__(
        "completion_block_id",
        document["intent_execution"]["steps"][0]["failure_successor_block_id"],
    ),
)
add_mutation(
    "completion-instruction-crosswire", "completion",
    lambda document: document["intent_execution"]["steps"][1].__setitem__(
        "completion_instruction_id",
        document["intent_execution"]["steps"][0]["completion_instruction_id"],
    ),
)
def completion_instruction_kind(document):
    step = document["intent_execution"]["steps"][0]
    routine = next(
        row for row in document["routines"]
        if row["source_syntax_id"] == step["routine_syntax_id"]
    )
    block = next(row for row in routine["blocks"] if row["id"] == step["completion_block_id"])
    instruction = next(
        row for row in block["instructions"]
        if row["id"] == step["completion_instruction_id"]
    )
    instruction["name"] = "IntentFailureCompleted"
add_mutation(
    "completion-instruction-kind", "completion", completion_instruction_kind
)

add_mutation(
    "compensation-transition", "compensation",
    lambda document: document["intent_execution"]["steps"][0]
        ["compensations"][0].__setitem__(
            "transition_id",
            document["intent_execution"]["steps"][1]["transition_id"],
        ),
)
add_mutation(
    "compensation-graph-digest", "compensation",
    lambda document: document["intent_execution"]["steps"][0]
        ["compensations"][0].__setitem__(
            "graph_digest",
            document["intent_execution"]["steps"][0]
                ["compensations"][0]["graph_digest"] + 1,
        ),
)
add_mutation(
    "duplicate-compensation-expression-id", "compensation",
    lambda document: document["intent_execution"]["steps"][1]
        ["compensations"][0].__setitem__(
            "expression_syntax_id",
            document["intent_execution"]["steps"][0]
                ["compensations"][0]["expression_syntax_id"],
        ),
)
add_mutation(
    "missing-terminal", "terminal",
    lambda document: document["intent_execution"]["terminals"].pop(),
)
add_mutation(
    "terminal-source-crosswire", "terminal",
    lambda document: document["intent_execution"]["terminals"][0].update({
        "source_transition_id": document["intent_execution"]["steps"][0]["transition_id"],
        "source_step_syntax_id": document["intent_execution"]["steps"][0]["step_syntax_id"],
        "source_step_name": document["intent_execution"]["steps"][0]["step_name"],
    }),
)
add_mutation(
    "terminal-result-enum-id", "terminal",
    lambda document: document["intent_execution"]["terminals"][0].__setitem__(
        "result_enum_syntax_id",
        document["intent_execution"]["steps"][0]["outcome_enum_syntax_id"],
    ),
)
add_mutation(
    "terminal-result-variant", "terminal",
    lambda document: document["intent_execution"]["terminals"][0].__setitem__(
        "result_variant_index",
        document["intent_execution"]["terminals"][1]["result_variant_index"],
    ),
)
add_mutation(
    "terminal-transition-expression-drift", "terminal",
    lambda document: document["intent_execution"]["terminals"][0].__setitem__(
        "terminal_transition_id",
        document["intent_execution"]["terminals"][0]["terminal_transition_id"] + 1000,
    ),
)
add_mutation(
    "plan-digest", "plan-digest",
    lambda document: document["intent_execution"].__setitem__(
        "plan_digest", document["intent_execution"]["plan_digest"] + 1
    ), rehash=False,
)

with open(manifest_path, "w", encoding="utf-8", newline="\n") as stream:
    stream.write("file\tcategory\n")
    for row in mutations:
        stream.write("\t".join(row) + "\n")

assert len(mutations) >= 28, len(mutations)
