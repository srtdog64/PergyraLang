#!/usr/bin/env python3
"""Owned negative MIR mutations for the match exactly-once parity gate."""

import argparse
import json
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("mutation", choices=("binding-type", "option-tag"))
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    document = json.loads(args.source.read_text(encoding="utf-8"))
    changed = False
    for routine in document.get("routines", []):
        for block in routine.get("blocks", []):
            for instruction in block.get("instructions", []):
                if args.mutation == "binding-type":
                    if (
                        instruction.get("source_type") == "AST_MATCH_CASE"
                        and instruction.get("match_variant") == "Some"
                        and instruction.get("match_binding_types") == ["Int"]
                    ):
                        instruction["match_binding_types"] = ["String"]
                        changed = True
                        break
                else:
                    layout = instruction.get("abi_layout")
                    if isinstance(layout, dict) and layout.get("type") == "Option<Int>":
                        layout["primary_tag"] = 7
                        changed = True
                        break
            if changed:
                break
        if changed:
            break
    if not changed:
        raise SystemExit(f"mutation source not found: {args.mutation}")
    args.output.write_text(
        json.dumps(document, separators=(",", ":")), encoding="utf-8"
    )


if __name__ == "__main__":
    main()
