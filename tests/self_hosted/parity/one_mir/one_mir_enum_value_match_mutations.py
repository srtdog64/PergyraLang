#!/usr/bin/env python3
"""Metamorphic and falsifying MIR cases for payload-free enum CFG."""

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


def instruction(doc: dict, block: int, row: int) -> dict:
    return doc["routines"][0]["blocks"][block]["instructions"][row]


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: mutations.py INPUT.json OUTPUT_DIR")
    source = pathlib.Path(sys.argv[1])
    out = pathlib.Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    for stale in list(out.glob("*.positive.json")) + list(out.glob("*.negative.json")):
        stale.unlink()
    base = json.loads(source.read_text(encoding="utf-8"))

    def coherent_rename(doc: dict) -> None:
        doc["decls"][0]["name"] = "Bearing"
        doc["decls"][0]["variants"][2]["name"] = "Zenith"
        graph = instruction(doc, 0, 0)["expr0_graph"]["nodes"]
        graph[0]["text"] = "Bearing"
        graph[1]["text"] = "Zenith"
        graph[2]["text"] = "display text may drift"

    changed(base, out, "coherent-rename", coherent_rename, "positive")

    def display_drift(doc: dict) -> None:
        for block in doc["routines"][0]["blocks"]:
            for row in block["instructions"]:
                if isinstance(row.get("expr0"), str):
                    row["expr0"] = "non-semantic display"

    changed(base, out, "display-drift", display_drift, "positive")
    changed(
        base,
        out,
        "case-one",
        lambda d: instruction(d, 0, 2).update(match_patterns=["1"]),
        "positive",
    )

    def selected_ordinal_one(doc: dict) -> None:
        variants = doc["decls"][0]["variants"]
        variants[:] = [variants[0], variants[2], variants[1], variants[3]]

    changed(base, out, "selected-ordinal-one", selected_ordinal_one, "positive")

    def arm_literals(doc: dict) -> None:
        instruction(doc, 1, 0)["expr0_graph"]["nodes"][0]["text"] = "333"
        instruction(doc, 1, 0)["expr0"] = "Log(333)"
        instruction(doc, 2, 0)["expr0_graph"]["nodes"][0]["text"] = "9"
        instruction(doc, 2, 0)["expr0"] = "Log(9)"

    changed(base, out, "arm-literals", arm_literals, "positive")

    def payload(doc: dict) -> None:
        variant = doc["decls"][0]["variants"][2]
        variant["param_count"] = 1
        variant["param_types"] = ["Int"]

    def duplicate_variant(doc: dict) -> None:
        doc["decls"][0]["variants"][3]["name"] = "South"

    def merge_nonempty(doc: dict) -> None:
        doc["routines"][0]["blocks"][3]["instructions"].append(
            copy.deepcopy(instruction(doc, 2, 0))
        )

    def extra_entry(doc: dict) -> None:
        doc["routines"][0]["blocks"][0]["instructions"].insert(
            2, copy.deepcopy(instruction(doc, 0, 1))
        )

    cases = {
        "duplicate-declaration": lambda d: d["decls"].append(copy.deepcopy(d["decls"][0])),
        "kind-axis-mismatch": lambda d: d["decls"][0].update(nominal_kind="struct"),
        "source-id-zero": lambda d: d["decls"][0].update(source_syntax_id=0),
        "declaration-extra-field": lambda d: d["decls"][0].update(runtime=True),
        "runtime-field": lambda d: d["decls"][0]["fields"].append({"name": "state"}),
        "runtime-method": lambda d: d["decls"][0]["methods"].append({"name": "run"}),
        "missing-variant": lambda d: d["decls"][0].update(variants=[]),
        "duplicate-variant": duplicate_variant,
        "variant-payload": payload,
        "variant-count-mismatch": lambda d: d["decls"][0]["variants"][2].update(param_count=1),
        "variant-extra-field": lambda d: d["decls"][0]["variants"][2].update(tag=2),
        "definition-abi-type": lambda d: instruction(d, 0, 0).update(abi_type_name="Long"),
        "definition-result": lambda d: instruction(d, 0, 0).update(result="forged.1"),
        "selection-owner": lambda d: instruction(d, 0, 0)["expr0_graph"]["nodes"][0].update(text="Other"),
        "selection-variant": lambda d: instruction(d, 0, 0)["expr0_graph"]["nodes"][1].update(text="Missing"),
        "selection-root": lambda d: instruction(d, 0, 0)["expr0_graph"].update(root=1),
        "selection-node-kind": lambda d: instruction(d, 0, 0)["expr0_graph"]["nodes"][2].update(kind="leaf"),
        "selection-call-target": lambda d: instruction(d, 0, 0)["expr0_graph"]["nodes"][0].update(call_target_kind="function", call_target_name="Fake"),
        "entry-log-local": lambda d: instruction(d, 0, 1)["expr0_graph"]["nodes"][0].update(text="other"),
        "entry-log-use": lambda d: instruction(d, 0, 1).update(uses=["forged.1"]),
        "branch-pattern-noncanonical": lambda d: instruction(d, 0, 2).update(match_patterns=["02"]),
        "branch-pattern-missing": lambda d: instruction(d, 0, 2).update(match_patterns=[]),
        "branch-pattern-duplicate": lambda d: instruction(d, 0, 2).update(match_patterns=["2", "3"]),
        "branch-binding": lambda d: instruction(d, 0, 2).update(match_bindings=["x"], match_binding_types=["Int"]),
        "branch-local": lambda d: instruction(d, 0, 2)["expr0_graph"]["nodes"][0].update(text="other"),
        "branch-use": lambda d: instruction(d, 0, 2).update(uses=["forged.1"]),
        "successor-out-of-range": lambda d: d["routines"][0]["blocks"][0].update(succ_true=9),
        "successor-swapped": lambda d: d["routines"][0]["blocks"][0].update(succ_true=2, succ_false=1),
        "arm-unreachable": lambda d: d["routines"][0]["blocks"][1].update(reachable=False),
        "arm-successor": lambda d: d["routines"][0]["blocks"][1].update(succ_true=2),
        "merge-nonempty": merge_nonempty,
        "true-log-kind": lambda d: instruction(d, 1, 0)["expr0_graph"]["nodes"][0].update(kind="bool_literal", text="true"),
        "false-log-use": lambda d: instruction(d, 2, 0).update(uses=["d.1"]),
        "extra-entry-instruction": extra_entry,
    }
    for name, mutate in cases.items():
        changed(base, out, name, mutate, "negative")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
