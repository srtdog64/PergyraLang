"""Semantic variants and malformed expression graphs for String concat."""

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


def instructions(document):
    return [
        instruction
        for block in document["routines"][0]["blocks"]
        for instruction in block["instructions"]
    ]


def definition(document, name):
    matches = [
        item for item in instructions(document)
        if item["kind"] == "def" and item["arg0"] == name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one definition for {name}")
    return matches[0]


def branch(document):
    matches = [item for item in instructions(document) if item["kind"] == "branch"]
    if len(matches) != 1:
        raise RuntimeError("expected one branch")
    return matches[0]


def display_only(document):
    for item in instructions(document):
        if item.get("expr0") is not None:
            item["expr0"] = "display text is not authority"
        if item.get("expr1") is not None:
            item["expr1"] = "display text is not authority"


def semantic_fail(document):
    target = definition(document, "a")
    target["expr0_graph"]["nodes"][0]["text"] = '"xx"'


def bad_first_concat_child(document):
    branch(document)["expr0_graph"]["nodes"][2]["right"] = 99


def bad_second_concat_kind(document):
    branch(document)["expr0_graph"]["nodes"][4]["kind"] = "logical_and"


def bad_equality_literal_kind(document):
    node = branch(document)["expr0_graph"]["nodes"][5]
    node["kind"] = "integer_literal"
    node["text"] = "0"


def bad_equality_root_child(document):
    branch(document)["expr0_graph"]["nodes"][6]["right"] = 99


def bad_use_identity(document):
    branch(document)["uses"] = ["a.1", "b.1", "missing.1"]


def bad_leaf_identity(document):
    branch(document)["expr0_graph"]["nodes"][3]["text"] = "missing"


emit("display-only", display_only)
emit("semantic-fail", semantic_fail)
emit("bad-first-concat-child", bad_first_concat_child)
emit("bad-second-concat-kind", bad_second_concat_kind)
emit("bad-equality-literal-kind", bad_equality_literal_kind)
emit("bad-equality-root-child", bad_equality_root_child)
emit("bad-use-identity", bad_use_identity)
emit("bad-leaf-identity", bad_leaf_identity)
