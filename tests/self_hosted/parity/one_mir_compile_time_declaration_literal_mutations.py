#!/usr/bin/env python3
"""Positive metamorphic and negative MIR cases for declaration erasure."""

from __future__ import annotations

import copy
import json
import pathlib
import sys


def write_case(out_dir: pathlib.Path, name: str, doc: dict, kind: str) -> None:
    path = out_dir / f"{name}.{kind}.json"
    path.write_text(
        json.dumps(doc, ensure_ascii=False, separators=(",", ":")),
        encoding="utf-8",
    )


def changed(base: dict, out_dir: pathlib.Path, name: str, mutate, kind: str) -> None:
    doc = copy.deepcopy(base)
    mutate(doc)
    if doc == base:
        raise SystemExit(f"mutation did not change the MIR: {name}")
    write_case(out_dir, name, doc, kind)


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: mutations.py INPUT.json OUTPUT_DIR")
    source = pathlib.Path(sys.argv[1])
    out_dir = pathlib.Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    base = json.loads(source.read_text(encoding="utf-8"))

    changed(base, out_dir, "zero-declaration", lambda d: d.update(decls=[]), "positive")

    def rename(d: dict) -> None:
        decl = d["decls"][0]
        decl["name"] = "RenamedContract"
        method = decl["methods"][0]
        method["name"] = "RenamedMethod"
        method["params"][0]["name"] = "receiver"
        method["params"][1]["name"] = "value"

    changed(base, out_dir, "coherent-rename", rename, "positive")
    changed(
        base,
        out_dir,
        "display-expr0-drift",
        lambda d: d["routines"][0]["blocks"][0]["instructions"][0].update(
            expr0="this display text is not semantic input"
        ),
        "positive",
    )

    def literal_73(d: dict) -> None:
        inst = d["routines"][0]["blocks"][0]["instructions"][0]
        inst["expr0"] = "Log(73)"
        inst["expr0_graph"]["nodes"][0]["text"] = "73"

    changed(base, out_dir, "literal-73", literal_73, "positive")

    cases = {
        "duplicate-declaration": lambda d: d["decls"].append(copy.deepcopy(d["decls"][0])),
        "declaration-tail": lambda d: d["decls"].append(7),
        "kind-mismatch": lambda d: d["decls"][0].update(nominal_kind="unknown"),
        "unsupported-kind": lambda d: d["decls"][0].update(kind="unknown", nominal_kind="unknown"),
        "source-id-zero": lambda d: d["decls"][0].update(source_syntax_id=0),
        "runtime-field": lambda d: d["decls"][0]["fields"].append({"name": "state"}),
        "missing-method": lambda d: d["decls"][0].update(methods=[]),
        "duplicate-method": lambda d: d["decls"][0]["methods"].append(copy.deepcopy(d["decls"][0]["methods"][0])),
        "method-extra-field": lambda d: d["decls"][0]["methods"][0].update(runtime_body="forbidden"),
        "return-type": lambda d: d["decls"][0]["methods"][0].update({"return": "Void"}),
        "callable-kind": lambda d: d["decls"][0]["methods"][0].update(callable_kind="action"),
        "contract-requires": lambda d: d["decls"][0]["methods"][0]["contract"].update(requires=[{"base": "Bound", "actuals": []}]),
        "contract-extra-field": lambda d: d["decls"][0]["methods"][0]["contract"].update(runtime=True),
        "missing-parameter": lambda d: d["decls"][0]["methods"][0]["params"].pop(),
        "receiver-type": lambda d: d["decls"][0]["methods"][0]["params"][0].update(type="Int"),
        "argument-type": lambda d: d["decls"][0]["methods"][0]["params"][1].update(type="String"),
        "duplicate-parameter-name": lambda d: d["decls"][0]["methods"][0]["params"][1].update(name=d["decls"][0]["methods"][0]["params"][0]["name"]),
        "extra-instruction": lambda d: d["routines"][0]["blocks"][0]["instructions"].append(copy.deepcopy(d["routines"][0]["blocks"][0]["instructions"][0])),
        "unreachable-block": lambda d: d["routines"][0]["blocks"][0].update(reachable=False),
        "instruction-kind": lambda d: d["routines"][0]["blocks"][0]["instructions"][0].update(kind="def"),
        "forged-use": lambda d: d["routines"][0]["blocks"][0]["instructions"][0].update(uses=["x.1"]),
        "graph-kind": lambda d: d["routines"][0]["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"][0].update(kind="bool_literal", text="true"),
        "graph-tail": lambda d: d["routines"][0]["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"].append({"kind": "integer_literal", "text": "9", "call_target_kind": "none", "call_target_name": "", "left": None, "right": None}),
        "noncanonical-integer": lambda d: d["routines"][0]["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"][0].update(text="07"),
        "forged-call-target": lambda d: d["routines"][0]["blocks"][0]["instructions"][0]["expr0_graph"]["nodes"][0].update(call_target_kind="function", call_target_name="Fake"),
    }
    for name, mutate in cases.items():
        changed(base, out_dir, name, mutate, "negative")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
