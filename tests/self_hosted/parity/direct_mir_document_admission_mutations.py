"""Build malformed pgy.mir.v1 documents from one current valid seed."""

import copy
import json
import pathlib
import sys


def main() -> int:
    seed_path = pathlib.Path(sys.argv[1])
    output_dir = pathlib.Path(sys.argv[2])
    text = seed_path.read_text(encoding="utf-8")
    seed = json.loads(text)

    (output_dir / "leading-comma.mir.json").write_text(
        "{" + "," + text[1:], encoding="utf-8"
    )
    (output_dir / "extra-root-close.mir.json").write_text(
        text + "}", encoding="utf-8"
    )

    duplicate = copy.deepcopy(seed)
    routine = next(
        routine for routine in duplicate["routines"]
        if sum(bool(block["instructions"]) for block in routine["blocks"]) >= 2
    )
    blocks = [block for block in routine["blocks"] if block["instructions"]]
    first = blocks[0]["instructions"][0]
    second = blocks[1]["instructions"][0]
    if first["id"] == second["id"]:
        raise RuntimeError("control seed already has duplicate InstructionId values")
    second["id"] = first["id"]
    (output_dir / "duplicate-instruction-id.mir.json").write_text(
        json.dumps(duplicate, separators=(",", ":")), encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
