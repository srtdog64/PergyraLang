#!/usr/bin/env python3
"""Metamorphic and falsifying MIR cases for role-operator dispatch."""

from __future__ import annotations

import copy
import json
import pathlib
import sys


def write_case(out: pathlib.Path, name: str, doc: dict, kind: str) -> None:
    (out / f"{name}.{kind}.json").write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def changed(base: dict, out: pathlib.Path, name: str, mutate, kind: str) -> None:
    doc = copy.deepcopy(base)
    mutate(doc)
    if doc == base:
        raise SystemExit(f"mutation did not change MIR: {name}")
    write_case(out, name, doc, kind)


def declaration(doc: dict, kind: str) -> dict:
    return next(row for row in doc["decls"] if row["kind"] == kind)


def routine(doc: dict, kind: str) -> dict:
    return next(row for row in doc["routines"] if row["kind"] == kind)


def main_instructions(doc: dict) -> list[dict]:
    return routine(doc, "function")["blocks"][0]["instructions"]


def method_return(doc: dict) -> dict:
    return routine(doc, "method")["blocks"][0]["instructions"][0]


def operator_node(doc: dict) -> dict:
    return main_instructions(doc)[2]["expr0_graph"]["nodes"][2]


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: mutations.py INPUT.json OUTPUT_DIR")
    source = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for stale in list(out.glob("*.positive.json")) + list(
        out.glob("*.negative.json")
    ):
        stale.unlink()
    base = json.loads(source.read_text(encoding="utf-8"))

    changed(
        base, out, "declaration-order",
        lambda d: d["decls"].reverse(), "positive",
    )
    changed(
        base, out, "routine-order",
        lambda d: d["routines"].reverse(), "positive",
    )

    def display_drift(doc: dict) -> None:
        for row in doc["routines"]:
            for block in row["blocks"]:
                for instruction in block["instructions"]:
                    if isinstance(instruction.get("expr0"), str):
                        instruction["expr0"] = "display text is not authority"

    changed(base, out, "display-drift", display_drift, "positive")

    def coherent_rename(doc: dict) -> None:
        role = declaration(doc, "role")
        ability = declaration(doc, "ability")
        method = routine(doc, "method")
        role["name"] = "ScalarMath"
        ability["name"] = "Numeric"
        role["methods"][0]["name"] = "OperatorAdd"
        ability["methods"][0]["name"] = "OperatorAdd"
        role["impls"][0]["ability"]["base"] = "Numeric"
        method["owner"] = "ScalarMath"
        method["name"] = "OperatorAdd"
        operator_node(doc)["call_target_name"] = (
            "ScalarMath.Numeric.OperatorAdd"
        )

    changed(base, out, "coherent-rename", coherent_rename, "positive")

    def method_value(doc: dict) -> None:
        row = method_return(doc)
        row["expr0"] = "321"
        row["expr0_graph"]["nodes"][0]["text"] = "321"

    changed(base, out, "method-value", method_value, "positive")

    def operands(doc: dict) -> None:
        rows = main_instructions(doc)
        rows[0]["expr0"] = "9"
        rows[0]["expr0_graph"]["nodes"][0]["text"] = "9"
        rows[1]["expr0"] = "10"
        rows[1]["expr0_graph"]["nodes"][0]["text"] = "10"

    changed(base, out, "operand-values", operands, "positive")

    def target_none(doc: dict) -> None:
        node = operator_node(doc)
        node["call_target_kind"] = "none"
        node["call_target_name"] = ""

    def duplicate_self(doc: dict) -> None:
        params = routine(doc, "method")["params"]
        params[1] = copy.deepcopy(params[0])

    def extra_routine(doc: dict) -> None:
        doc["routines"].append(copy.deepcopy(routine(doc, "method")))
        doc["routines"][-1]["name"] = "Other"

    cases = {
        "target-none": target_none,
        "target-empty": lambda d: operator_node(d).update(call_target_name=""),
        "target-role": lambda d: operator_node(d).update(
            call_target_name="Other.Arithmetic.Add"
        ),
        "target-ability": lambda d: operator_node(d).update(
            call_target_name="IntMath.Other.Add"
        ),
        "target-method": lambda d: operator_node(d).update(
            call_target_name="IntMath.Arithmetic.Sub"
        ),
        "operator-kind": lambda d: operator_node(d).update(kind="subtract"),
        "role-for-type": lambda d: declaration(d, "role").update(
            for_type="Long"
        ),
        "impl-ability": lambda d: declaration(d, "role")["impls"][0][
            "ability"
        ].update(base="Other"),
        "impl-method-start": lambda d: declaration(d, "role")["impls"][0].update(
            method_start=1
        ),
        "impl-method-count": lambda d: declaration(d, "role")["impls"][0].update(
            method_count=0
        ),
        "missing-ability-method-id": lambda d: declaration(d, "ability")["methods"][0].pop("source_syntax_id"),
        "zero-ability-method-id": lambda d: declaration(d, "ability")["methods"][0].update(source_syntax_id=0),
        "duplicate-method-id": lambda d: declaration(d, "ability")["methods"][0].update(source_syntax_id=declaration(d, "role")["methods"][0]["source_syntax_id"]),
        "role-method-name": lambda d: declaration(d, "role")["methods"][0].update(
            name="Sub"
        ),
        "ability-method-name": lambda d: declaration(d, "ability")["methods"][0].update(
            name="Sub"
        ),
        "role-rhs-type": lambda d: declaration(d, "role")["methods"][0][
            "params"
        ][1].update(type="Long"),
        "ability-rhs-type": lambda d: declaration(d, "ability")["methods"][0][
            "params"
        ][1].update(type="Long"),
        "method-owner": lambda d: routine(d, "method").update(owner="Other"),
        "method-name": lambda d: routine(d, "method").update(name="Sub"),
        "method-receiver": lambda d: routine(d, "method").update(
            receiver_carriage="none"
        ),
        "method-rhs-type": lambda d: routine(d, "method")["params"][1].update(
            type="Long", abi_type_name="Long"
        ),
        "method-return-type": lambda d: routine(d, "method").update(
            {"return": "Long"}
        ),
        "method-return-abi": lambda d: method_return(d).update(
            abi_type_name="Long"
        ),
        "method-return-source": lambda d: method_return(d).update(
            source_type="AST_CALL"
        ),
        "method-return-graph": lambda d: method_return(d)["expr0_graph"][
            "nodes"
        ][0].update(kind="bool_literal", text="true"),
        "method-duplicate-self": duplicate_self,
        "caller-use": lambda d: main_instructions(d)[2].update(
            uses=["forged.1", "b.1"]
        ),
        "caller-edge": lambda d: operator_node(d).update(left=1),
        "lhs-abi": lambda d: main_instructions(d)[0].update(
            abi_type_name="Long"
        ),
        "extra-routine": extra_routine,
    }
    for name, mutate in cases.items():
        changed(base, out, name, mutate, "negative")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
