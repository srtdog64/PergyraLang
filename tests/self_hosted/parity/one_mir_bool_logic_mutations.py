"""Graph-owned semantic variants and falsifiers for scalar Bool programs."""

import copy
import json
import pathlib
import sys


source = pathlib.Path(sys.argv[1])
output_dir = pathlib.Path(sys.argv[2])
baseline = json.loads(source.read_text(encoding="utf-8"))


def emit(name, mutate):
    document = copy.deepcopy(baseline)
    mutate(document)
    (output_dir / f"{name}.json").write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


def routine(document, name):
    matches = [item for item in document["routines"] if item["name"] == name]
    if len(matches) != 1:
        raise RuntimeError(f"expected one routine named {name}")
    return matches[0]


def block(document, block_id):
    matches = [
        item for item in routine(document, "Main")["blocks"]
        if item["id"] == block_id
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected Main block {block_id}")
    return matches[0]


def instruction(document, block_id, instruction_id):
    matches = [
        item for item in block(document, block_id)["instructions"]
        if item["id"] == instruction_id
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected instruction {instruction_id} in Main block {block_id}"
        )
    return matches[0]


def other_true(document):
    matches = [
        item for item in block(document, 0)["instructions"]
        if item.get("result") == "other.1"
    ]
    if len(matches) != 1:
        raise RuntimeError("expected the Main other.1 definition")
    target = matches[0]
    target["expr0_graph"]["nodes"][0]["text"] = "true"
def logical_and_variant(document):
    target = instruction(document, 6, 3)
    target["expr0_graph"]["nodes"][5]["kind"] = "logical_and"

def modulo_three(document):
    target = next(
        item
        for item_block in routine(document, "IsEven")["blocks"]
        for item in item_block["instructions"]
        if item.get("kind") == "return"
    )
    target["expr0_graph"]["nodes"][1]["text"] = "3"

def set_is_even_modulo(document, text):
    target = next(
        item
        for item_block in routine(document, "IsEven")["blocks"]
        for item in item_block["instructions"]
        if item.get("kind") == "return"
    )
    target["expr0_graph"]["nodes"][1]["text"] = text

def display_only(document):
    for item in document["routines"]:
        for item_block in item["blocks"]:
            for item_instruction in item_block["instructions"]:
                if item_instruction.get("expr0") is not None:
                    item_instruction["expr0"] = "display text is not authority"
                if item_instruction.get("expr1") is not None:
                    item_instruction["expr1"] = "display text is not authority"


def reverse_routines(document):
    document["routines"] = list(reversed(document["routines"]))


def bad_use_identity(document):
    instruction(document, 2, 1)["uses"] = ["flag.1"]


def bad_logical_kind(document):
    instruction(document, 4, 2)["expr0_graph"]["nodes"][3]["kind"] = "add"


def bad_backedge(document):
    block(document, 14)["succ_true"] = 15


def bad_call_target(document):
    target = instruction(document, 12, 7)["expr0_graph"]["nodes"][1]
    target["call_target_syntax_id"] = 999999


def bad_call_argument_type(document):
    target = instruction(document, 12, 7)
    target["expr0_graph"]["nodes"][2]["text"] = "flag"
    target["uses"] = ["flag.1"]


def bad_short_circuit_rhs(document):
    target = instruction(document, 4, 2)
    target["expr0_graph"] = {
        "root": 6,
        "nodes": [
            {"kind": "bool_literal", "text": "false"},
            {"kind": "integer_literal", "text": "1"},
            {"kind": "integer_literal", "text": "0"},
            {"kind": "modulo", "text": "1 % 0", "left": 1, "right": 2},
            {"kind": "integer_literal", "text": "0"},
            {"kind": "equality", "text": "(1 % 0) == 0", "left": 3, "right": 4},
            {"kind": "logical_and", "text": "false && ((1 % 0) == 0)", "left": 0, "right": 5},
        ],
    }
    target["uses"] = []


def bad_phi_incoming_identity(document):
    instruction(document, 11, 5)["uses"] = ["i.1", "i.1"]


def non_scalar_callable_signature(document):
    callable_routine = routine(document, "IsEven")
    callable_routine["params"][0]["type"] = "Bool"
    callable_routine["params"][0]["abi_type_name"] = "Bool"


emit("other-true", other_true)
emit("logical-and", logical_and_variant)
emit("modulo-three", modulo_three)
emit("display-only", display_only)
emit("routine-order", reverse_routines)
emit("bad-use-identity", bad_use_identity)
emit("bad-logical-kind", bad_logical_kind)
emit("bad-backedge", bad_backedge)
emit("bad-call-target", bad_call_target)
emit("bad-call-argument-type", bad_call_argument_type)
emit("bad-short-circuit-rhs", bad_short_circuit_rhs)
emit("bad-phi-incoming-identity", bad_phi_incoming_identity)
emit("modulo-zero", lambda document: set_is_even_modulo(document, "0"))
emit("modulo-minus-one", lambda document: set_is_even_modulo(document, "-1"))
emit("non-scalar-callable-signature", non_scalar_callable_signature)
